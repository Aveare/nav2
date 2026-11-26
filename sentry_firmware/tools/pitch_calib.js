"use strict";
/**
 * @file  tools/pitch_calib.js
 * @brief pitch 重力前馈标定:PITCH_GRAVITY_A / PHASE / OFFSET 的采点、拟合与验证。
 *
 * 要解决什么:
 *   加装自瞄模块后云台前端质量变大,速度环要靠积分扛住重力矩。角度环有 0.3° 死区,
 *   死区内速度环没有比例项,稳态输出全由积分项提供,上限 IntegralLimit=1200;
 *   一旦重力矩需要更多电流,表现就是低位抬不起来 / 保持不住。
 *   正解是按角度给一路重力前馈(gimbal.c 已接好,只是 PITCH_GRAVITY_FF_GAIN=0 关着)。
 *
 * 原理(为什么静止读数就是"保持重力所需的电流"):
 *   静止且已收敛时,速度环 err≈0 -> P 项≈0、D 项≈0,`speed_PID.Output` 里剩下的就是
 *   维持当前角度所需的电流码(C620 原始给定域,±16384 ↔ ±20A)。前馈要抵消的正是它,
 *   所以"每个角度静止读 Output"就是一组 (θ, I) 标定点,无需额外测力计。
 *
 * 模型(gimbal.c:300):I(θ) = A·cos(θ − φ) + B,θ 取 IMU 欧拉 Pitch(重力是世界系量)。
 *   注意本车机械行程只有 PITCH_MIN_ANGLE..PITCH_MAX_ANGLE(-20°..+25°),cos 在该区间
 *   只变化约 10%,A 与 B 强相关、2 参数拟合条件数差。因此 fit 会同时给出:
 *     - 常数模型 I = B(只填 OFFSET):窄行程下已能吃掉 90%+ 的重力负担,最稳;
 *     - 余弦模型 I = A·cos(θ)+B;
 *     - 带相位余弦模型(φ 网格搜索)。
 *   三者都打印残差 RMS,按残差与参数可信度取舍,不盲信某一个。
 *
 * 为什么不用 variables.read 读这些量:
 *   复合类型(`->` 成员)在本工程 LTO 后的 DWARF 里不可用,variables.read 会静默返回垃圾。
 *   这里走 debug.evaluate(暂停态求值),路径由 gdb 解析,成员访问可靠。
 *
 * 用法:
 *   node tools/pitch_calib.js add <标签>     采一个点(车静止在该角度时执行)
 *   node tools/pitch_calib.js jam [标签]     连续采样"卡住/被顶住"的瞬态,判它是缺力还是卡死
 *   node tools/pitch_calib.js show           看已采的点
 *   node tools/pitch_calib.js fit            拟合,打印建议宏值
 *   node tools/pitch_calib.js clear          清空采集数据
 *
 * jam 子命令要解决什么:
 *   前馈只补"重力"这一类与角度相关的静态力矩。若真正原因是机械顶住/线缆拉死,
 *   前馈会把堵转电流顶得更狠,只会更热。所以标定之前先判一次"卡住是什么性质":
 *     - `speed_PID.Output` 贴 ±4000(MaxOut)且角度一位不动 -> 电流已到上限仍不动,
 *       要么力矩真不够,要么被硬顶住;需要再做一次提高 MaxOut 的对照实验来区分;
 *     - `Output` 贴 ±1200(IntegralLimit)且角度缓慢下移 -> 积分撑不住,属"缺力",
 *       正好是重力前馈能解决的问题;
 *     - 角度来回抖 ±几度、Output 大幅摆动 -> 摩擦极限环,前馈也治不好,先查机械。
 *   采样期间请持续用杆往上推(即"试图抬起"),这样记录到的才是真实的抗争状态。
 *
 * 采点前要满足:遥控中挡/下挡(云台 GYRO 模式,电机会保持)、杆回中、云台停稳 2s 以上。
 * 采点会暂停核心约 0.2s(静止状态无风险),结束后会校验核心确实恢复运行。
 */

const fs = require("fs");
const path = require("path");
const { call } = require(path.join(__dirname, "..", ".agents", "skills", "_emberprobe", "agent-client.js"));

const WORKSPACE = path.resolve(__dirname, "..");
const CSV = path.join(WORKSPACE, "build", "pitch_calib.csv");
const JAM_CSV = path.join(WORKSPACE, "build", "pitch_jam.csv");
const BOOT_MS_VAR = "chassis_last_ms";
const SETTLE_SPEED_APS = 30; // 转速超过它说明还在动,采到的不是稳态
const MAX_OUT_REF = 8000;   // 与 gimbal.c 中 pitch speed_PID.MaxOut 保持一致(2026-11-15 判据实验 4000→8000)    // speed_PID.MaxOut(gimbal.c:206),峰值电流码上限
const INT_LIMIT_REF = 1200;  // speed_PID.IntegralLimit(gimbal.c:204),静止保持的实际上限
const TEMP_WARN = 55;        // M3508/C620 堵转测温提醒阈值,℃

// 标定要读的量(表达式由 gdb 解析,均可静态解析的成员路径)
const EXPR = {
    ang: "gimba_IMU_data->Pitch",                                  // θ,IMU 欧拉 pitch(度)
    ref: "gimbal_cmd_recv.pitch",                                  // 角度环目标(度),ref-ang 即角度误差
    out: "pitch_motor->motor_controller.speed_PID.Output",          // 稳态保持电流码
    iout: "pitch_motor->motor_controller.speed_PID.Iout",           // 其中积分项(判断是否贴 IntegralLimit)
    spd: "pitch_motor->measure.speed_aps",                          // 转速(判稳态)
    gyro: "gimba_IMU_data->Gyro[0]",                                // 速度环实际反馈(IMU X 轴角速度),查符号用
    cur: "pitch_motor->measure.real_current",                       // 实际电流
    temp: "pitch_motor->measure.temperature"                        // 温度
};

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/** 读一个标量全局变量 */
async function readVar(name) {
    const r = await call(WORKSPACE, "variables.read", { variables: [{ name }] }, 20000);
    return r.values?.[name]?.value ?? null;
}

/** 恢复目标并确认真的在跑(同 read_motor.js:侵入式读取可能把核心留在暂停) */
async function resumeAndVerify() {
    let lastErr = null;
    for (let i = 0; i < 5; i++) {
        try {
            await call(WORKSPACE, "debug.control", { action: "continue" }, 30000);
        } catch (e) {
            if (e.code !== "TARGET_NOT_PAUSED") lastErr = e.message;
        }
        await sleep(350);
        try {
            const s = await call(WORKSPACE, "debug.status", null, 20000);
            if (s.state !== "running") continue;
            const a = await readVar(BOOT_MS_VAR);
            await sleep(300);
            const b = await readVar(BOOT_MS_VAR);
            if (typeof a === "number" && typeof b === "number" && b > a) return { bootMs: b };
            lastErr = `${BOOT_MS_VAR} 两次读数不增长(${a} → ${b}),核心疑似仍停着`;
        } catch (e) {
            lastErr = e.message;
        }
    }
    return { failed: true, lastErr };
}

/** 从 debug.evaluate 的返回里抠出数值(不同后端外壳形状不同,逐个试) */
function pickValue(r) {
    if (r == null) return null;
    if (typeof r === "number") return r;
    const cands = [r.value, r.result?.value, r.value?.value, r.result?.result, r.result];
    for (const c of cands) {
        if (typeof c === "number") return c;
        if (c && typeof c.value === "number") return c.value;
        if (typeof c === "string") {
            const n = Number(c.trim());
            if (c.trim() !== "" && !Number.isNaN(n)) return n;
            // uint8_t 这类字符型量 gdb 可能回成 `22 '\\026'`,抠前导数字
            const m = c.match(/-?\d+(\.\d+)?/);
            if (m) return Number(m[0]);
        }
        if (c && typeof c.value === "string") {
            const m = c.value.match(/-?\d+(\.\d+)?/);
            if (m) return Number(m[0]);
        }
    }
    return null;
}

/** 暂停一次,批量求值,再校验恢复 */
async function evaluateAll(exprs) {
    const status = await call(WORKSPACE, "debug.status", null, 20000);
    if (!status.session) {
        throw new Error("没有活动的调试会话。先执行: node .agents/skills/mcu-debug-control/scripts/debug.js --workspace <工程> --start --attach");
    }
    if (status.paused) {
        const r = await resumeAndVerify();
        if (r && r.failed) throw new Error(`目标处于暂停状态且无法恢复(${r.lastErr || "未知"})`);
    }

    await call(WORKSPACE, "debug.control", { action: "pause" }, 30000);
    const vals = {};
    const raw = {};
    try {
        for (const [key, expression] of Object.entries(exprs)) {
            const r = await call(WORKSPACE, "debug.evaluate", { expression }, 30000);
            raw[key] = r;
            vals[key] = pickValue(r);
        }
        vals[BOOT_MS_VAR] = await readVar(BOOT_MS_VAR);
    } finally {
        // 恢复统一走 resumeAndVerify
    }

    const resumed = await resumeAndVerify();
    if (resumed && resumed.failed) {
        const e = new Error(`恢复目标失败(${resumed.lastErr || "未知"}),这次读数可能是冻结值`);
        e.raw = raw;
        throw e;
    }
    return { vals, raw };
}

// ---------------------------------------------------------------- 采点

/** 采一次:暂停 → 批量求值 → 校验恢复,返回归一化后的点 */
async function sampleOnce(label) {
    const { vals, raw } = await evaluateAll(EXPR);
    const num = (k) => (typeof vals[k] === "number" ? vals[k] : null);
    const p = {
        at: new Date().toISOString(),
        label: label || "",
        ang: num("ang"),
        ref: num("ref"),
        out: num("out"),
        iout: num("iout"),
        spd: num("spd"),
        gyro: num("gyro"),
        cur: num("cur"),
        temp: num("temp"),
        bootMs: typeof vals[BOOT_MS_VAR] === "number" ? vals[BOOT_MS_VAR] : null
    };
    if (p.ang != null && p.ref != null) p.err = Number((p.ref - p.ang).toFixed(2));
    if (p.ang == null || p.out == null) {
        const e = new Error("没能从 debug.evaluate 取到 ang/out 数值(返回形状可能变了),原始返回见 raw");
        e.raw = raw;
        throw e;
    }
    return p;
}

async function addPoint(label) {
    const p = await sampleOnce(label);

    const warnings = [];
    if (p.spd != null && Math.abs(p.spd) > SETTLE_SPEED_APS) warnings.push(`转速 |${p.spd.toFixed(1)}| > ${SETTLE_SPEED_APS}apS,可能还没停稳`);
    if (p.iout != null && p.out != null && Math.abs(p.iout) >= 0.98 * INT_LIMIT_REF && Math.abs(p.out) >= 0.98 * INT_LIMIT_REF)
        warnings.push(`Iout 已贴近 IntegralLimit ${INT_LIMIT_REF} —— 正说明这个角度积分顶满了,属于该标的点`);

    fs.mkdirSync(path.dirname(CSV), { recursive: true });
    if (!fs.existsSync(CSV)) fs.writeFileSync(CSV, "at,label,ang_deg,ref_deg,err_deg,out,iout,spd_aps,cur,temp,boot_ms\n");
    fs.appendFileSync(CSV, `${p.at},${p.label},${p.ang},${p.ref},${p.err},${p.out},${p.iout},${p.spd},${p.cur},${p.temp},${p.bootMs}\n`);
    return { p, warnings, count: readRows().length };
}

/**
 * 连续采样"卡住"状态并判定性质。
 * 采样期间请持续按杆上推(试图抬起),记录到的才是"电机与阻挡僵持"的真实状态。
 */
async function jamTest(label, secs) {
    const rows = [];
    const t0 = Date.now();
    // 每个样本的真实耗时约 0.7~1.0s(暂停核心只占其中 ~0.25s,其余是恢复校验),按样本数循环更可控
    const targetSamples = Math.max(5, Math.round((secs * 1000) / 900));
    let stopped = null;

    for (let i = 0; i < targetSamples; i++) {
        let p;
        try {
            p = await sampleOnce(label || "jam");
        } catch (e) {
            stopped = `采样中断:${e.message}`;
            break;
        }
        rows.push(p);
        if (p.temp != null && p.temp > TEMP_WARN && !stopped) {
            stopped = `温度 ${p.temp}℃ 超过 ${TEMP_WARN}℃,已停止采样(继续堵转会烧)`;
            break;
        }
        if (i < targetSamples - 1) await sleep(150);
    }

    fs.mkdirSync(path.dirname(JAM_CSV), { recursive: true });
    if (!fs.existsSync(JAM_CSV)) fs.writeFileSync(JAM_CSV, "at,label,ang_deg,ref_deg,err_deg,out,iout,spd_aps,gyro,cur,temp,boot_ms\n");
    for (const r of rows) fs.appendFileSync(JAM_CSV, `${r.at},${r.label},${r.ang},${r.ref},${r.err},${r.out},${r.iout},${r.spd},${r.gyro},${r.cur},${r.temp},${r.bootMs}\n`);

    if (rows.length === 0) return { ok: false, message: stopped || "没采到样本" };

    const abs = (x) => Math.abs(x);
    const angs = rows.map((r) => r.ang);
    const outs = rows.map((r) => abs(r.out));
    const iouts = rows.map((r) => abs(r.iout));
    const spds = rows.map((r) => abs(r.spd));
    const cur = rows.map((r) => abs(r.cur));
    const temps = rows.map((r) => r.temp).filter((t) => typeof t === "number");

    const angDrift = angs[angs.length - 1] - angs[0];
    const angSpan = Math.max(...angs) - Math.min(...angs);
    const maxOut = Math.max(...outs);
    const maxIout = Math.max(...iouts);
    const maxCur = Math.max(...cur);
    const errs = rows.map((r) => r.err).filter((e) => typeof e === "number");
    const absErr = errs.map((e) => Math.abs(e));
    const meanAbsErr = absErr.length ? absErr.reduce((s, x) => s + x, 0) / absErr.length : null;
    const peakPinned = maxOut >= 0.95 * MAX_OUT_REF;
    const intPinned = maxIout >= 0.95 * INT_LIMIT_REF;
    const still = angSpan < 2.0;
    const sagging = angDrift < -1.0;
    const stuckErr = meanAbsErr != null && meanAbsErr > 2.0 && still;
    const oscillating = angSpan > 4.0 && Math.max(...spds) > SETTLE_SPEED_APS;
    const tempRise = temps.length >= 2 ? temps[temps.length - 1] - temps[0] : null;

    let verdict;
    let advice;
    if (still && stuckErr) {
        verdict = `角度卡在误差 ${meanAbsErr.toFixed(2)}° 不动、Output 峰值 ${Math.round(maxOut)}${intPinned ? `、Iout 贴 ${INT_LIMIT_REF}` : ""}${peakPinned ? `、已顶 MaxOut ${MAX_OUT_REF}` : ""}`;
        if (peakPinned) {
            advice = "电流到上限仍追不上目标。**判据实验**:把 speed_PID.MaxOut 临时提到 6000 重烧再试 —— 能跟上=缺力(前馈/更高上限);纹丝不动=机械顶住(前馈无用,先修机械)";
        } else if (intPinned) {
            advice = "积分顶满 IntegralLimit=1200 而角度误差消不掉:速度环没有更多持续力矩可用。**这正是重力前馈要解决的问题**,按 add→fit 标定后把静态负担交给前馈,积分就能退下来";
        } else {
            advice = "既没顶 MaxOut 也没顶积分却追不上目标:怀疑机械摩擦/线缆张力或反馈异常,先手摸手感(零力挡)再谈前馈";
        }
    } else if (intPinned && sagging) {
        verdict = `积分贴 IntegralLimit=${INT_LIMIT_REF} 且角度持续下移(${angDrift.toFixed(2)}°)`;
        advice = "典型缺力:积分撑不住重力矩。这正是重力前馈要解决的问题,按 add/fit 标定后应显著改善";
    } else if (oscillating) {
        verdict = `角度来回抖(跨度 ${angSpan.toFixed(2)}°),不是静态卡住`;
        advice = "摩擦极限环/异响类问题(参见 DEV-09/DEV-10 记录),前馈治不好,先查机械啮合与线缆";
    } else if (sagging) {
        verdict = `角度在缓慢下移(${angDrift.toFixed(2)}°),Output 未满`;
        advice = "接近缺力但还没到限幅,前馈加上去多半能解决";
    } else {
        verdict = `未判定:角度跨度 ${angSpan.toFixed(2)}°、位移 ${angDrift.toFixed(2)}°、Output 峰值 ${Math.round(maxOut)}`;
        advice = "看明细;若采样期间杆没推上去,重跑一次 jam 并持续上推";
    }

    return {
        ok: true,
        samples: rows.length,
        seconds: Number(((Date.now() - t0) / 1000).toFixed(1)),
        stopped,
        ang: { first: Number(angs[0].toFixed(2)), last: Number(angs[angs.length - 1].toFixed(2)), span: Number(angSpan.toFixed(2)), drift: Number(angDrift.toFixed(2)) },
        out: { max: Math.round(maxOut), peakPinned },
        iout: { max: Math.round(maxIout), intPinned },
        cur: { max: maxCur },
        temp: { min: temps.length ? Math.min(...temps) : null, max: temps.length ? Math.max(...temps) : null, rise: tempRise },
        verdict,
        advice,
        rows: rows.map((r) => ({ ang: Number(r.ang.toFixed(2)), ref: r.ref, err: r.err, out: Math.round(r.out), iout: Math.round(r.iout), spd: Number(r.spd.toFixed(1)), gyro: r.gyro, cur: r.cur, temp: r.temp }))
    };
}

function readRows() {
    if (!fs.existsSync(CSV)) return [];
    const lines = fs.readFileSync(CSV, "utf8").split(/\r?\n/).filter((l) => l.trim() !== "");
    if (lines.length <= 1) return [];
    return lines.slice(1).map((l) => {
        const c = l.split(",");
        return {
            at: c[0], label: c[1],
            ang: Number(c[2]), ref: Number(c[3]), err: Number(c[4]),
            out: Number(c[5]), iout: Number(c[6]),
            spd: Number(c[7]), cur: Number(c[8]), temp: Number(c[9]), bootMs: Number(c[10])
        };
    });
}

// ---------------------------------------------------------------- 拟合

/** 最小二乘:y ≈ k1*x1 + k2*x2 + b,返回 {k1,k2,b,sse} */
function ls2(rows, x1f, x2f) {
    // 正规方程 3x3(手动解,避免引入依赖)
    let s11 = 0, s12 = 0, s1c = 0, s22 = 0, s2c = 0, n = 0, sy = 0, s1y = 0, s2y = 0;
    for (const r of rows) {
        const x1 = x1f(r), x2 = x2f(r);
        s11 += x1 * x1; s12 += x1 * x2; s1c += x1;
        s22 += x2 * x2; s2c += x2; n += 1;
        sy += r.out; s1y += x1 * r.out; s2y += x2 * r.out;
    }
    // 解 [s11 s12 s1c; s12 s22 s2c; s1c s2c n] * [k1 k2 b] = [s1y s2y sy]
    const A = [[s11, s12, s1c], [s12, s22, s2c], [s1c, s2c, n]];
    const B = [s1y, s2y, sy];
    const sol = solve3(A, B);
    if (!sol) return null;
    let sse = 0;
    for (const r of rows) {
        const pred = sol[0] * x1f(r) + sol[1] * x2f(r) + sol[2];
        sse += (r.out - pred) ** 2;
    }
    return { k1: sol[0], k2: sol[1], b: sol[2], sse };
}

/** 常数模型:y ≈ b */
function lsConst(rows) {
    const n = rows.length;
    const b = rows.reduce((s, r) => s + r.out, 0) / n;
    let sse = 0;
    for (const r of rows) sse += (r.out - b) ** 2;
    return { b, sse };
}

function solve3(A, B) {
    const M = A.map((row, i) => [...row, B[i]]);
    const n = 3;
    for (let c = 0; c < n; c++) {
        let p = c;
        for (let r = c + 1; r < n; r++) if (Math.abs(M[r][c]) > Math.abs(M[p][c])) p = r;
        if (Math.abs(M[p][c]) < 1e-9) return null;
        [M[c], M[p]] = [M[p], M[c]];
        for (let r = 0; r < n; r++) {
            if (r === c) continue;
            const f = M[r][c] / M[c][c];
            for (let k = c; k <= n; k++) M[r][k] -= f * M[c][k];
        }
    }
    return [M[0][3] / M[0][0], M[1][3] / M[1][1], M[2][3] / M[2][2]];
}

const DEG2RAD = Math.PI / 180;
const rms = (sse, n) => Math.sqrt(sse / Math.max(n, 1));

function fit() {
    const rows = readRows();
    if (rows.length < 3) return { ok: false, message: `只有 ${rows.length} 个点,至少 3 个(建议 8 个以上且覆盖行程两端)` };

    const angs = rows.map((r) => r.ang);
    const span = Math.max(...angs) - Math.min(...angs);

    // ① 常数模型
    const c0 = lsConst(rows);
    // ② 余弦模型(A·cosθ + B)
    const c1 = ls2(rows, (r) => Math.cos(r.ang * DEG2RAD), () => 0);
    // ③ 带相位余弦:φ 网格搜索(±40°,1° 步长),每个 φ 用 ② 的两个基拟合
    let best = null;
    for (let phi = -40; phi <= 40; phi += 1) {
        const r = ls2(rows, (x) => Math.cos((x.ang - phi) * DEG2RAD), () => 0);
        if (r && (!best || r.sse < best.sse)) best = { ...r, phi };
    }

    const n = rows.length;
    const out = {
        ok: true,
        points: n,
        angRange: [Math.min(...angs), Math.max(...angs)],
        angleSpanDeg: Number(span.toFixed(1)),
        models: {
            constant: { OFFSET: Math.round(c0.b), rms: Number(rms(c0.sse, n).toFixed(1)) },
            cos: c1 ? { A: Math.round(c1.k1), OFFSET: Math.round(c1.b), rms: Number(rms(c1.sse, n).toFixed(1)) } : null,
            cosPhase: best ? { A: Math.round(best.k1), PHASE: best.phi, OFFSET: Math.round(best.b), rms: Number(rms(best.sse, n).toFixed(1)) } : null
        }
    };

    // 推荐:残差接近时优先常数模型(参数少、窄行程下不易过拟合)
    const simple = Math.abs(c1 ? c1.k1 : 0) < 0.25 * Math.abs(c0.b) || span < 30;
    out.recommend = simple
        ? { note: "行程窄/cos 项贡献小,推荐只填 OFFSET(常数重力前馈),A 保持 0", ...out.models.constant, A: 0 }
        : { note: "cos 项显著,推荐余弦模型", ...out.models.cos };

    out.rows = rows.map((r) => ({ ang: Number(r.ang.toFixed(2)), out: Math.round(r.out), iout: Math.round(r.iout), spd: Number(r.spd.toFixed(1)) }));
    return out;
}

// ---------------------------------------------------------------- CLI

(async () => {
    const argv = process.argv.slice(2);
    const cmd = argv[0];
    const wantJson = argv.includes("--json");

    if (!cmd || cmd === "help" || cmd === "-h") {
        process.stdout.write(
            "用法:\n" +
            "  node tools/pitch_calib.js add <标签>    采一个点(车静止在目标角度)\n" +
            "  node tools/pitch_calib.js jam [标签] --secs 8\n" +
            "                                          连续采样\"卡住\"状态,判缺力/卡死/振荡\n" +
            "                                          (采样期间请持续上推,试图抬起)\n" +
            "  node tools/pitch_calib.js show          看已采的点\n" +
            "  node tools/pitch_calib.js fit           拟合并打印建议宏值\n" +
            "  node tools/pitch_calib.js clear         清空采集数据\n"
        );
        return;
    }

    if (cmd === "add") {
        const label = argv[1] && !argv[1].startsWith("--") ? argv[1] : "";
        const { p, warnings, count } = await addPoint(label);
        if (wantJson) {
            process.stdout.write(JSON.stringify({ ok: true, point: p, warnings, count }, null, 2) + "\n");
        } else {
            process.stdout.write(
                `第 ${count} 点  ${label}\n` +
                `  θ=${p.ang.toFixed(2)}°  目标=${p.ref == null ? "?" : p.ref.toFixed(2)}°  误差=${p.err == null ? "?" : p.err.toFixed(2)}°\n` +
                `  Output=${p.out.toFixed(0)}  Iout=${p.iout.toFixed(0)}  ` +
                `转速=${p.spd.toFixed(1)}apS  电流=${p.cur}  温度=${p.temp}℃\n`
            );
            for (const w of warnings) process.stdout.write(`  ⚠ ${w}\n`);
        }
        return;
    }

    if (cmd === "show") {
        const rows = readRows();
        if (rows.length === 0) return void process.stdout.write("还没有采点。\n");
        const lines = ["  #  θ(°)    目标    误差   Output   Iout   转速   电流  温度  标签"];
        rows.forEach((r, i) => {
            lines.push(
                `  ${String(i + 1).padStart(2)} ${r.ang.toFixed(2).padStart(7)} ${String(r.ref).padStart(7)} ${String(r.err).padStart(7)} ` +
                `${String(Math.round(r.out)).padStart(8)} ${String(Math.round(r.iout)).padStart(6)} ${r.spd.toFixed(1).padStart(7)} ` +
                `${String(r.cur).padStart(6)} ${String(r.temp).padStart(5)}  ${r.label}`
            );
        });
        process.stdout.write(lines.join("\n") + `\n共 ${rows.length} 点,文件 ${path.relative(WORKSPACE, CSV)}\n`);
        return;
    }

    if (cmd === "fit") {
        const r = fit();
        if (wantJson) return void process.stdout.write(JSON.stringify(r, null, 2) + "\n");
        if (!r.ok) return void process.stdout.write(`拟合失败:${r.message}\n`);

        const lines = [];
        lines.push(`样本 ${r.points} 点  行程 ${r.angRange[0].toFixed(1)}° ~ ${r.angRange[1].toFixed(1)}°  (跨度 ${r.angleSpanDeg}°)`);
        lines.push("");
        lines.push("  模型                         残差RMS");
        lines.push(`  常数   I = ${r.models.constant.OFFSET}                  ${r.models.constant.rms}`);
        if (r.models.cos) lines.push(`  余弦   I = ${r.models.cos.A}·cosθ + ${r.models.cos.OFFSET}     ${r.models.cos.rms}`);
        if (r.models.cosPhase) lines.push(`  带相位 I = ${r.models.cosPhase.A}·cos(θ−${r.models.cosPhase.PHASE}°) + ${r.models.cosPhase.OFFSET}  ${r.models.cosPhase.rms}`);
        lines.push("");
        const rec = r.recommend;
        lines.push(`推荐:${rec.note}`);
        if (rec.PHASE === undefined) {
            lines.push(`  #define PITCH_GRAVITY_A 0.0f`);
            lines.push(`  #define PITCH_GRAVITY_PHASE 0.0f`);
            lines.push(`  #define PITCH_GRAVITY_OFFSET ${rec.OFFSET}.0f`);
        } else {
            lines.push(`  #define PITCH_GRAVITY_A ${rec.A}.0f`);
            lines.push(`  #define PITCH_GRAVITY_PHASE ${rec.PHASE}.0f`);
            lines.push(`  #define PITCH_GRAVITY_OFFSET ${rec.OFFSET}.0f`);
        }
        lines.push(`  #define PITCH_GRAVITY_FF_GAIN 1.0f`);
        lines.push("");
        lines.push("注意:FF_LIMIT 由 A 与 OFFSET 自动推导(config:460),改完 A/OFFSET 不必再动手。");
        process.stdout.write(lines.join("\n") + "\n");
        return;
    }

    if (cmd === "jam") {
        const args = argv.slice(1).filter((a) => a !== "--json");
        let label = "";
        let secs = 8;
        for (let i = 0; i < args.length; i++) {
            if (args[i] === "--secs") secs = Math.max(2, Math.min(30, Number(args[++i]) || 8));
            else if (!label) label = args[i];
        }
        const r = await jamTest(label, secs);
        if (wantJson) return void process.stdout.write(JSON.stringify(r, null, 2) + "\n");
        if (!r.ok) return void process.stdout.write(`jam 采样失败:${r.message}\n`);

        const L = [];
        L.push(`jam "${label || "jam"}"  ${r.samples} 样本 / ${r.seconds}s${r.stopped ? "  ⚠ " + r.stopped : ""}`);
        L.push(`  θ 首尾 ${r.ang.first}° → ${r.ang.last}°   跨度 ${r.ang.span}°   位移 ${r.ang.drift}°`);
        L.push(`  Output 峰值 ${r.out.max}${r.out.peakPinned ? `(已顶 MaxOut ${MAX_OUT_REF})` : ""}   Iout 峰值 ${r.iout.max}${r.iout.intPinned ? `(已顶 IntegralLimit ${INT_LIMIT_REF})` : ""}`);
        L.push(`  实际电流峰值 ${r.cur.max}   温度 ${r.temp.min}→${r.temp.max}℃${r.temp.rise != null ? `(+${r.temp.rise})` : ""}`);
        L.push("");
        L.push(`结论:${r.verdict}`);
        L.push(`处置:${r.advice}`);
        L.push("");
        L.push("明细(θ/ 目标/ 误差/ Output/ Iout/ 转速/ 陀螺X/ 电流/ 温度):");
        for (const x of r.rows) L.push(`  ${String(x.ang).padStart(7)}  ${String(x.ref).padStart(7)}  ${String(x.err).padStart(7)}  ${String(x.out).padStart(6)}  ${String(x.iout).padStart(6)}  ${String(x.spd).padStart(7)}  ${String(x.gyro).padStart(7)}  ${String(x.cur).padStart(6)}  ${x.temp}`);
        process.stdout.write(L.join("\n") + "\n");
        return;
    }

    if (cmd === "clear") {
        if (fs.existsSync(CSV)) fs.unlinkSync(CSV);
        process.stdout.write("已清空采集数据。\n");
        return;
    }

    process.stdout.write(`未知命令 ${cmd},用 node tools/pitch_calib.js help 看用法\n`);
    process.exitCode = 2;
})().catch((error) => {
    const payload = { ok: false, code: error.code || null, message: error.message, details: error.details || null };
    if (error.raw) payload.raw = error.raw;
    process.stdout.write(JSON.stringify(payload, null, 2) + "\n");
    process.exitCode = 1;
});
