/**
 * gimbal_axis_probe.js —— 云台两轴共模污染核验(纯主机侧,零固件改动)
 *
 * 用途:判定 pitch 速度环当前绑定的 `Gyro[0]` 是否真是 pitch 轴角速度。
 * 背景(DEV-20261030-02 / 2026-11-16 实测):
 *   ① `application/gimbal/gimbal.c:220` pitch 速度反馈 = `gimbal_imu_data->Gyro[0]`;
 *      `gimbal.c:155` yaw = `Gyro[2]`。
 *   ② IMU 装在俯仰框上时 pitch 轴固定在框体里 → θ̇ 应严格等于**某一根**陀螺轴;
 *      若绑错,则该轴混入 yaw 分量(≈ψ̇·sinθ),被速度环 Kp(50/D2R≈2865 码每 rad/s)放大成
 *      几百至几千码的假力矩 → 表现为"yaw 微动 → pitch 异响"。
 *
 * 判据(采完自动给结论):
 *   - pitch 相位里三轴响应最大的那根 = 真正的 pitch 轴;
 *   - 若它不是 Gyro[0] → 当前绑定错,给出泄漏量与折算电流码;
 *   - 再看 yaw 相位里 Gyro[0] 是否跟着动、pitch 电机是否跟着出力(交叉反馈闭环实证)。
 *
 * 采样走 EmberProbe `variables.sample`(DWARF 指针路径,~24Hz,**不暂停核心**)。
 * 注意:用 memory.read / debug.evaluate 读会暂停核心 → GimbalTask 停发 CAN → C620 约 100ms 无帧
 * 即按 0 输出,枪管被重力砸下(DEV-20261116-03 的观测伪影)。所以这里只用变量采样。
 *
 * 用法:
 *   node tools/gimbal_axis_probe.js selftest          # 只读一次,验证通道/ELF/固件一致性
 *   node tools/gimbal_axis_probe.js run               # 三段式采集:静止10s → 只动yaw15s → 只动pitch15s
 *   node tools/gimbal_axis_probe.js run --idle 8 --yaw 12 --pitch 12
 *   node tools/gimbal_axis_probe.js report            # 重新分析 build/gimbal_axis_probe.csv
 *   --json 追加到任意子命令,输出机器可读结果
 */
"use strict";

const fs = require("fs");
const path = require("path");
const { call } = require(path.join(__dirname, "..", ".agents", "skills", "_emberprobe", "agent-client.js"));

const WORKSPACE = path.resolve(__dirname, "..");
const OUT_CSV = path.join(WORKSPACE, "build", "gimbal_axis_probe.csv");

const D2R = Math.PI / 180;
const RAD2DEG = 180 / Math.PI;
const KP_CODES_PER_RADS = 50 / D2R; // gimbal.c:192 speed_PID.Kp = 50/D2R ≈ 2865 码/(rad/s)
const SAMPLE_CHUNK = 100;           // 单个 variables.sample 请求过长会被打断,分块(实测安全值)
const SAMPLE_INTERVAL = 20;         // ms,实际可达 ~24Hz

/* 要读的量:DWARF 成员路径。名字与 gimbal.c / gimbal.h 里的全局一致。
 * 注:yaw 是 GM6020,没有 C620 的 real_current 语义差异,这里仍读 measure.real_current
 * (单位同样是电流环给定域),只用于判"是否被反作用带动出力"。 */
const CH = {
    gx: "gimbal_imu_data->Gyro[0]",  // 芯片/体轴 X 角速度(rad/s),pitch 速度环当前用的就是它
    gy: "gimbal_imu_data->Gyro[1]",  // 体轴 Y 角速度(rad/s)
    gz: "gimbal_imu_data->Gyro[2]",  // 体轴 Z 角速度(rad/s),yaw 速度环用的是它
    pitch: "gimbal_imu_data->Pitch", // 欧拉 pitch(度)= pitch 角度环反馈
    roll: "gimbal_imu_data->Roll",
    yaw: "gimbal_imu_data->Yaw",
    pTgt: "gimbal_cmd_recv.pitch",  // pitch 目标角(度)
    yTgt: "gimbal_cmd_recv.yaw",    // yaw 目标角(度)
    pOut: "pitch_motor->motor_controller.speed_PID.Output",
    pIout: "pitch_motor->motor_controller.speed_PID.Iout",
    pSpd: "pitch_motor->measure.speed_aps",
    yOut: "yaw_motor->motor_controller.speed_PID.Output",
    /* 注意:不要加 `yaw_motor->measure.speed_aps` —— 实测批量/采样请求里它会被解析成
     * `measure` 整个 composite(返回 diagnostic LIVE_READ_BUDGET_EXCEEDED),单独读才正常。
     * yaw 角速度用 gz(Gyro[2])代替,语义上就是 yaw 速度环反馈。 */
    auth: "authority_source"        // 3 = RUN(已解锁)
};

const AUTH_NAME = { 0: "RC_LOST", 1: "KILL_SWITCH", 2: "STOP_LATCHED", 3: "RUN" };
const ENUM_NUM = { ROBOT_STOP: 0, ROBOT_READY: 1, AUTH_RC_LOST: 0, AUTH_KILL_SWITCH: 1, AUTH_STOP_LATCHED: 2, AUTH_RUN: 3 };

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/** 枚举字面量 → 数值(与 pitch_calib.js 同款:后端可能回字面量而不是数字) */
function toNum(v) {
    if (typeof v === "number") return v;
    if (typeof v === "string") {
        const t = v.trim();
        if (t in ENUM_NUM) return ENUM_NUM[t];
        const n = Number(t);
        return Number.isFinite(n) ? n : null;
    }
    if (v && typeof v === "object") {
        for (const c of [v.value, v.result, v.result?.value]) {
            const n = toNum(c);
            if (n !== null) return n;
        }
    }
    return null;
}

/** 一次变量读(非侵入)。返回 { vals, consistency } */
async function readOnce(names) {
    const r = await call(WORKSPACE, "variables.read", { variables: names.map((name) => ({ name })) }, 30000);
    const vals = {};
    for (const n of names) vals[n] = toNum(r.values?.[n]?.value ?? r.values?.[n]);
    return { vals, consistency: r.firmwareConsistency || null };
}

/**
 * 分块非侵入采样一段(按**墙钟时间**收口,不按点数)。
 * 为什么不按点数:实测 variables.sample 的实际速率受探针 RPC 往返影响(标称 20ms 时实际 ~24Hz),
 * 按点数算会让"15s 相位"实际跑成 30s,和用户手里的动作时间轴对不上。
 * 失败容错:单块重试 3 次,块间失败不中止,拿到多少存多少;一点都没采到才抛错。
 */
async function samplePhase(label, secs, onCue) {
    const names = Object.keys(CH).map((k) => CH[k]);

    // 先用一次轻量读把通道叫醒(会话可能还在建立),不消耗采样窗口
    try {
        await readOnce(names.slice(0, 2));
    } catch (e) {
        // 叫醒失败不致命,让下面的采样报真正的错
    }
    if (onCue) onCue();

    const rows = [];
    const t0 = Date.now();
    const deadline = t0 + secs * 1000;
    let failedChunks = 0;
    const CHUNK = 40;   // ≈1.5s 一块;单请求太长会被打断(实测)
    const MAX_ROWS = 2000;

    while (Date.now() < deadline && rows.length < MAX_ROWS) {
        let batch = null;
        for (let attempt = 0; attempt < 3 && !batch; attempt++) {
            try {
                const r = await call(
                    WORKSPACE,
                    "variables.sample",
                    { variables: names.map((name) => ({ name })), count: CHUNK, intervalMs: SAMPLE_INTERVAL },
                    30000
                );
                batch = Array.isArray(r.samples) ? r.samples : null;
            } catch (e) {
                if (attempt === 2) {
                    failedChunks++;
                    if (!rows.length) throw e; // 一点都没采到才算是真失败
                }
                await sleep(250);
            }
        }
        if (!batch) break;
        for (const s of batch) {
            const row = { t: s.timestamp };
            for (const k of Object.keys(CH)) row[k] = toNum(s.values?.[CH[k]]);
            rows.push(row);
        }
    }
    return { label, rows, failedChunks, wanted: rows.length, seconds: (Date.now() - t0) / 1000 };
}

/* ---------------- 统计 ---------------- */

function nums(rows, key) {
    return rows.map((r) => r[key]).filter((v) => typeof v === "number" && Number.isFinite(v));
}
function stat(rows, key) {
    const a = nums(rows, key);
    if (!a.length) return null;
    const mean = a.reduce((s, x) => s + x, 0) / a.length;
    const min = Math.min(...a);
    const max = Math.max(...a);
    const rms = Math.sqrt(a.reduce((s, x) => s + x * x, 0) / a.length);
    return { n: a.length, mean, min, max, range: max - min, peak: Math.max(Math.abs(min), Math.abs(max)), rms };
}
/** 皮尔逊相关;点数不足或某一列方差为 0 时返回 null */
function corr(rows, ka, kb) {
    const pairs = rows.filter((r) => typeof r[ka] === "number" && typeof r[kb] === "number").map((r) => [r[ka], r[kb]]);
    if (pairs.length < 8) return null;
    const n = pairs.length;
    const ma = pairs.reduce((s, p) => s + p[0], 0) / n;
    const mb = pairs.reduce((s, p) => s + p[1], 0) / n;
    let saa = 0, sbb = 0, sab = 0;
    for (const [a, b] of pairs) {
        saa += (a - ma) ** 2;
        sbb += (b - mb) ** 2;
        sab += (a - ma) * (b - mb);
    }
    if (saa <= 1e-12 || sbb <= 1e-12) return null;
    return sab / Math.sqrt(saa * sbb);
}

/* ---------------- 分析 ---------------- */

const AXIS_NAME = { gx: "Gyro[0]", gy: "Gyro[1]", gz: "Gyro[2]" };

function analyse(phases, meta) {
    const byLabel = {};
    for (const p of phases) byLabel[p.label] = p;

    const idle = byLabel.idle;
    const yawP = byLabel.yaw;
    const pitchP = byLabel.pitch;

    const out = { meta, phases: {}, findings: [], verdict: null };

    for (const p of phases) {
        out.phases[p.label] = {
            samples: p.rows.length,
            seconds: p.seconds,
            failedChunks: p.failedChunks,
            gx: stat(p.rows, "gx"),
            gy: stat(p.rows, "gy"),
            gz: stat(p.rows, "gz"),
            pitch: stat(p.rows, "pitch"),
            yaw: stat(p.rows, "yaw"),
            pOut: stat(p.rows, "pOut"),
            yOut: stat(p.rows, "yOut"),
            pitchSpeed: stat(p.rows, "pSpd")
        };
    }

    // 1) 真正的 pitch 轴:在"只动 pitch"相位里三轴范围最大的那根
    let pitchAxis = null;
    let yawAxis = null;
    if (pitchP && pitchP.rows.length > 8) {
        const r = { gx: stat(pitchP.rows, "gx")?.range ?? 0, gy: stat(pitchP.rows, "gy")?.range ?? 0, gz: stat(pitchP.rows, "gz")?.range ?? 0 };
        pitchAxis = Object.keys(r).sort((a, b) => r[b] - r[a])[0];
        out.pitchPhaseRanges = r;
        out.findings.push(`只动 pitch 时三轴峰峰值:${AXIS_NAME.gx} ${(r.gx * RAD2DEG).toFixed(1)}°/s、${AXIS_NAME.gy} ${(r.gy * RAD2DEG).toFixed(1)}°/s、${AXIS_NAME.gz} ${(r.gz * RAD2DEG).toFixed(1)}°/s → pitch 轴=bod ${AXIS_NAME[pitchAxis]}`);
    }
    if (yawP && yawP.rows.length > 8) {
        const r = { gx: stat(yawP.rows, "gx")?.range ?? 0, gy: stat(yawP.rows, "gy")?.range ?? 0, gz: stat(yawP.rows, "gz")?.range ?? 0 };
        yawAxis = Object.keys(r).sort((a, b) => r[b] - r[a])[0];
        out.yawPhaseRanges = r;
        out.findings.push(`只动 yaw 时三轴峰峰值:${AXIS_NAME.gx} ${(r.gx * RAD2DEG).toFixed(1)}°/s、${AXIS_NAME.gy} ${(r.gy * RAD2DEG).toFixed(1)}°/s、${AXIS_NAME.gz} ${(r.gz * RAD2DEG).toFixed(1)}°/s → yaw 轴=bod ${AXIS_NAME[yawAxis]}`);
    }

    // 2) 泄漏量:yaw 相位里"当前 pitch 通道(Gyro[0])"与"yaw 通道"的范围比
    if (yawP && yawAxis && yawAxis !== "gx") {
        const leak = out.yawPhaseRanges.gx;
        const ref = out.yawPhaseRanges[yawAxis];
        const ratio = ref > 1e-6 ? leak / ref : null;
        const inducedCodes = Math.abs(stat(yawP.rows, "gx")?.peak ?? 0) * KP_CODES_PER_RADS;
        out.leakage = { ratio, leakRad: leak, refRad: ref, inducedCodes };
        if (ratio !== null) {
            out.findings.push(
                `yaw 相位里 Gyro[0] 峰峰值 ${(leak * RAD2DEG).toFixed(1)}°/s = yaw 通道的 ${(ratio * 100).toFixed(1)}%;` +
                    `按 Kp ${KP_CODES_PER_RADS.toFixed(0)} 码/(rad/s) 折算 ≈ ${inducedCodes.toFixed(0)} 码假力矩`
            );
        }
    }

    // 3) 交叉闭环实证:yaw 相位里 Gyro[0] 与 pitch 电机输出是否相关
    let crossCorr = null;
    if (yawP && yawP.rows.length > 8) {
        crossCorr = corr(yawP.rows, "gx", "pOut");
        if (crossCorr !== null) {
            out.findings.push(`yaw 相位 r(Gyro[0], pitch.Output) = ${crossCorr.toFixed(2)}(>0.5 说明 pitch 电机在跟 yaw 动作出力)`);
        }
    }

    // 4) 解锁状态检查:yaw/pitch 相位必须在 RUN,否则数据无效
    for (const p of [yawP, pitchP]) {
        if (!p || !p.rows.length) continue;
        const a = stat(p.rows, "auth");
        if (a) {
            const unlocked = p.rows.filter((r) => r.auth === 3).length / p.rows.length;
            if (unlocked < 0.9) out.findings.push(`⚠ ${p.label} 相位只有 ${(unlocked * 100).toFixed(0)}% 的样本处于 RUN(解锁),结论可能失真`);
        }
    }

    // 5) 裁决
    if (pitchAxis === "gx" && (out.leakage?.ratio ?? 0) < 0.15) {
        out.verdict = "OK-绑定正确:Gyro[0] 确实是 pitch 轴,且 yaw 泄漏 <15%。刺响另有原因(机械反作用/摩擦/纹波)。";
    } else if (pitchAxis && pitchAxis !== "gx") {
        out.verdict =
            `BUG-绑定错误:pitch 轴实为 ${AXIS_NAME[pitchAxis]},而 gimbal.c:220 绑的是 Gyro[0]。` +
            (out.leakage ? ` yaw 动作经 Gyro[0] 注入 ≈${out.leakage.inducedCodes.toFixed(0)} 码假力矩。` : "") +
            ` 修法:other_speed_feedback_ptr 改 &gimbal_imu_data->${AXIS_NAME[pitchAxis]}(gimbal.c:220),并做符号核验。`;
    } else if (pitchAxis === "gx" && (out.leakage?.ratio ?? 0) >= 0.15) {
        out.verdict = `MIXED-Gyro[0] 虽是 pitch 主轴,但 yaw 泄漏达 ${(out.leakage.ratio * 100).toFixed(0)}%:需按安装角投影或用欧拉式 θ̇=Gy1·cosφ−Gy2·sinφ(DEV-20261030-02)。`;
    } else {
        out.verdict = "数据不足:确认车已上电、探针已连接、解锁后重采。";
    }

    out.crossCorr = crossCorr;
    return out;
}

function fmtConsistency(c) {
    if (!c) return "n/a";
    if (typeof c === "string") return c;
    const st = c.status || "?";
    const sha = (c.flashedSha256 || c.elfSha256 || "").slice(0, 8);
    return `${st}${sha ? ` (${sha})` : ""}`;
}

function fmt(v, w, d = 2) {
    return typeof v === "number" ? v.toFixed(d).padStart(w) : "?".padStart(w);
}

function printReport(res) {
    const L = [];
    L.push(`采集:${res.meta.at}  固件一致性=${fmtConsistency(res.meta.consistency)}`);
    for (const name of ["idle", "yaw", "pitch"]) {
        const p = res.phases[name];
        if (!p) continue;
        L.push("");
        L.push(`[${name}] ${p.samples} 样本 / ${fmt(p.seconds, 4, 1)}s${p.failedChunks ? `,失败块 ${p.failedChunks}` : ""}`);
        L.push(`  Gyro[0] ${fmt(p.gx?.range * RAD2DEG, 7, 1)}°/s峰峰 | Gyro[1] ${fmt(p.gy?.range * RAD2DEG, 7, 1)} | Gyro[2] ${fmt(p.gz?.range * RAD2DEG, 7, 1)}`);
        L.push(`  pitch ${fmt(p.pitch?.min, 7, 2)}~${fmt(p.pitch?.max, 7, 2)}° | yaw ${fmt(p.yaw?.min, 7, 2)}~${fmt(p.yaw?.max, 7, 2)}°`);
        L.push(`  pitch.Out ${fmt(p.pOut?.min, 6, 0)}~${fmt(p.pOut?.max, 6, 0)} | yaw.Out ${fmt(p.yOut?.min, 6, 0)}~${fmt(p.yOut?.max, 6, 0)}`);
    }
    L.push("");
    L.push("判读:");
    for (const s of res.findings) L.push("  · " + s);
    L.push("");
    L.push("结论:" + res.verdict);
    return L.join("\n");
}

function writeCsv(phases) {
    const head = ["phase", "t", ...Object.keys(CH)];
    const lines = [head.join(",")];
    for (const p of phases) {
        for (const r of p.rows) {
            lines.push([p.label, r.t, ...Object.keys(CH).map((k) => (typeof r[k] === "number" ? r[k] : ""))].join(","));
        }
    }
    fs.mkdirSync(path.dirname(OUT_CSV), { recursive: true });
    fs.writeFileSync(OUT_CSV, lines.join("\n") + "\n", "utf8");
    return lines.length - 1;
}

function readCsv() {
    if (!fs.existsSync(OUT_CSV)) throw new Error(`没有 ${OUT_CSV},先跑 run`);
    const lines = fs.readFileSync(OUT_CSV, "utf8").trim().split(/\r?\n/);
    const head = lines[0].split(",");
    const phases = new Map();
    for (const line of lines.slice(1)) {
        const c = line.split(",");
        const label = c[0];
        const row = {};
        for (let i = 2; i < head.length; i++) {
            const v = c[i];
            row[head[i]] = v === "" || v === undefined ? null : Number(v);
        }
        if (!phases.has(label)) phases.set(label, []);
        phases.get(label).push(row);
    }
    return [...phases.entries()].map(([label, rows]) => ({ label, rows, wanted: rows.length, failedChunks: 0 }));
}

/* ---------------- 子命令 ---------------- */

async function selftest() {
    const names = Object.values(CH);
    const { vals, consistency } = await readOnce(names);
    const L = ["通道自检(variables.read,非侵入):", `  固件一致性: ${fmtConsistency(consistency)}`];
    for (const [k, n] of Object.entries(CH)) {
        const v = vals[n];
        L.push(`  ${k.padEnd(6)} = ${v === null ? "✗ 读不到" : String(v).padStart(12)}   (${n})`);
    }
    const missing = Object.entries(CH).filter(([, n]) => vals[n] === null).map(([k]) => k);
    if (missing.length) L.push(`  ⚠ 读不到:${missing.join(", ")} —— 检查 ELF 是否为板上的固件(变量被优化掉/名字变了)`);
    else L.push("  ✓ 全部通道可读");
    const auth = vals[CH.auth];
    L.push(`  当前权限: ${AUTH_NAME[auth] ?? auth}(采集 yaw/pitch 相位需要 RUN)`);
    return { text: L.join("\n"), raw: { consistency, missing, auth, vals } };
}

async function run(opts) {
    if (opts.arm > 0 && !opts.quiet) {
        process.stdout.write(
            `[准备] ${opts.arm}s 后开始 —— 顺序:静止 ${opts.idle}s → 只动 yaw ${opts.yaw}s → 只动 pitch ${opts.pitch}s\n`
        );
        for (let left = opts.arm; left > 0; left -= 5) {
            process.stdout.write(`  ${left}s...\n`);
            await sleep(Math.min(5000, left * 1000));
        }
    }
    const order = [
        { label: "idle", secs: opts.idle, cue: `[1/3] 静止 ${opts.idle}s —— 手离开杆,枪管自然保持` },
        { label: "yaw", secs: opts.yaw, cue: `[2/3] 只动 yaw ${opts.yaw}s —— 只需左右打 yaw 杆,别碰 pitch` },
        { label: "pitch", secs: opts.pitch, cue: `[3/3] 只动 pitch ${opts.pitch}s —— 只上下打 pitch,yaw 杆回中` }
    ];
    const phases = [];
    for (const p of order) {
        const r = await samplePhase(p.label, p.secs, () => {
            if (!opts.quiet) process.stdout.write(p.cue + "\n");
        });
        phases.push(r);
        if (r.failedChunks) process.stdout.write(`  ⚠ ${p.label} 有 ${r.failedChunks} 个采样块失败\n`);
    }
    const meta = { at: new Date().toISOString(), consistency: null };
    try {
        meta.consistency = (await readOnce([CH.gx])).consistency;
    } catch (e) {
        /* 忽略 */
    }
    const res = analyse(phases, meta);
    res.csvRows = writeCsv(phases);
    return res;
}

/* ---------------- 入口 ---------------- */

async function main() {
    const argv = process.argv.slice(2);
    const cmd = argv[0] || "selftest";
    const wantJson = argv.includes("--json");
    const quiet = argv.includes("--quiet");
    const argOf = (flag, dflt) => {
        const i = argv.indexOf(flag);
        return i >= 0 ? Number(argv[i + 1]) : dflt;
    };

    if (cmd === "selftest") {
        const r = await selftest();
        process.stdout.write((wantJson ? JSON.stringify({ ok: true, ...r.raw, text: r.text }, null, 2) : r.text) + "\n");
        return;
    }
    if (cmd === "run") {
        const res = await run({
            idle: argOf("--idle", 12),
            yaw: argOf("--yaw", 15),
            pitch: argOf("--pitch", 15),
            arm: argOf("--arm", 20),
            quiet
        });
        process.stdout.write((wantJson ? JSON.stringify({ ok: true, ...res }, null, 2) : printReport(res) + `\n\nCSV: ${OUT_CSV} (${res.csvRows} 行)`) + "\n");
        return;
    }
    if (cmd === "report") {
        const phases = readCsv();
        const meta = { at: "(from csv)", consistency: null };
        const res = analyse(phases, meta);
        process.stdout.write((wantJson ? JSON.stringify({ ok: true, ...res }, null, 2) : printReport(res)) + "\n");
        return;
    }
    throw new Error(`未知子命令 ${cmd}(可用:selftest | run | report)`);
}

main().catch((error) => {
    const payload = { ok: false, code: error.code || null, message: error.message, details: error.details || null };
    process.stderr.write(JSON.stringify(payload) + "\n");
    process.stdout.write(JSON.stringify(payload) + "\n");
    process.exitCode = 1;
});
