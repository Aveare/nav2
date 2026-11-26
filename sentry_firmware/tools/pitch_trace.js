"use strict";
/**
 * @file  tools/pitch_trace.js
 * @brief 非侵入式采样 pitch 轴(pitch 角 / 目标 / 速度环输出 / 电流),用于判"云台是否真的在自己动"。
 *
 * 为什么需要它(这条是踩过大坑换来的):
 *   本工程之前的读数工具(read_chassis / read_motor / read_yaw / pitch_calib)全部走
 *       variables.read 取指针 → memory.read 按偏移解码
 *   而 **memory.read 必须先暂停内核**。核心一停:
 *     gimbal_task 不再发 CAN 控制帧 → C620 约 100ms 无帧触发信号丢失保护(输出按 0)→
 *     枪管被重力砸下去 → 恢复核心后力矩回来又抬起来。
 *   于是"每读一个点云台就抽一下",读数与现象互相污染(2026-11-16 实测:用户看到"一读探针就往下砸")。
 *
 *   正确通道是 EmberProbe 的 **variables.sample**(后台 DAP 采样,不暂停内核),
 *   它一次调用就返回整段原始样本;本工具因此一次 RPC 拿到整条波形,而不是逐点往返。
 *   变量用 DWARF 路径直接给:`ins_state.Pitch`、`pitch_motor->measure.real_current`。
 *
 * 判振荡的指标(为什么这么算):
 *   - 每两点之间的 Δpitch 若超过 DEADBAND(默认 0.02°)才算一次有效变化,滤掉浮点噪声;
 *   - 有效变化的方向来回翻转 ≥ REVERSAL_MIN(默认 6)次且峰峰值 ≥ RANGE_MIN(默认 1.0°)
 *     → 判"往复运动(振荡)";峰峰值 < 0.3° → 判"静止";其余判"单向/慢漂"。
 *   峰峰值与反转次数是这轮排障里唯一能区分"真振荡"和"我读一次砸一次"的硬指标。
 *
 * 用法:
 *   node tools/pitch_trace.js                          # 采 10s,50ms 间隔,存 build/pitch_trace.csv
 *   node tools/pitch_trace.js --seconds 20 --interval 20 --label 中挡自稳
 *   node tools/pitch_trace.js --vars "ins_state.Pitch,ins_state.Gyro[0]" --out build/x.csv
 */

const fs = require("fs");
const path = require("path");
const { call } = require(path.join(__dirname, "..", ".agents", "skills", "_emberprobe", "agent-client.js"));

const WORKSPACE = path.resolve(__dirname, "..");

/** 默认采样变量:角度/角速度/目标/输出/积分/电流/转速,覆盖"是不是在动"与"为什么动"。 */
const DEFAULT_VARS = [
    "ins_state.Pitch",
    "ins_state.Gyro[0]",
    "gimbal_cmd_recv.pitch",
    "pitch_motor->measure.real_current",
    "pitch_motor->motor_controller.speed_PID.Output",
    "pitch_motor->motor_controller.speed_PID.Iout",
    "pitch_motor->measure.speed_aps"
];

/** 固件/桥接两条硬限幅:variables.sample 的 count 上限 1000、intervalMs 上限 60000、下限 5。 */
const MAX_COUNT = 1000;
const MIN_INTERVAL_MS = 5;

const DEADBAND_DEG = 0.02;
const REVERSAL_MIN = 6;
const RANGE_MIN_DEG = 1.0;
const STILL_RANGE_DEG = 0.3;

function parseArgs(argv) {
    const out = { seconds: 10, interval: 50, out: "build/pitch_trace.csv", label: "", vars: DEFAULT_VARS };
    for (let i = 0; i < argv.length; i++) {
        const key = argv[i];
        const val = argv[i + 1];
        if (key === "--seconds") (out.seconds = Number(val)), i++;
        else if (key === "--interval") (out.interval = Number(val)), i++;
        else if (key === "--out") (out.out = val), i++;
        else if (key === "--label") (out.label = val), i++;
        else if (key === "--vars") (out.vars = String(val).split(",").map((s) => s.trim()).filter(Boolean)), i++;
        else if (key === "--preset") {
            /* 症状预设:tools/trace_presets.json(变量契约受 check_symbol_contract 守卫) */
            const presets = JSON.parse(fs.readFileSync(path.join(__dirname, "trace_presets.json"), "utf8"));
            const p = presets[val];
            if (!p) throw new Error(`未知预设: ${val}(可选: ${Object.keys(presets).filter((k) => !k.startsWith("_")).join(", ")})`);
            out.vars = p.vars.slice();
            out.interval = p.interval_ms;
            out.seconds = p.seconds;
            out.out = `build/trace_${val}.csv`;
            out.label = out.label || val;
            i++;
        }
        else if (key === "--help" || key === "-h") out.help = true;
        else throw new Error(`未知参数: ${key}`);
    }
    if (!Number.isFinite(out.seconds) || out.seconds <= 0) throw new Error("--seconds 必须是正数");
    if (!Number.isFinite(out.interval) || out.interval < MIN_INTERVAL_MS) throw new Error(`--interval 必须 ≥ ${MIN_INTERVAL_MS}ms`);
    return out;
}

/** 变量名太长,CSV 表头与统计里用短名,便于人读。 */
function shortName(name) {
    return name
        .replace("gimbal_cmd_recv.pitch", "ref")
        .replace("pitch_motor->motor_controller.speed_PID.", "pid_")
        .replace("pitch_motor->measure.", "m_")
        .replace("ins_state.", "ins_")
        .replace(/[\[\]]/g, "");
}

function statsOf(values) {
    const clean = values.filter((v) => typeof v === "number" && Number.isFinite(v));
    if (!clean.length) return null;
    const min = Math.min(...clean);
    const max = Math.max(...clean);
    const sum = clean.reduce((a, b) => a + b, 0);
    return { n: clean.length, min, max, mean: sum / clean.length, range: max - min, first: clean[0], last: clean[clean.length - 1] };
}

/**
 * 振荡判据。只用 pitch 角序列:
 * 计数"方向翻转"而不是数过零点 —— 云台绕着某个平衡角来回时不必穿越 0。
 */
function judgeOscillation(pitchSeries) {
    let reversals = 0;
    let travel = 0;
    let lastDir = 0;
    for (let i = 1; i < pitchSeries.length; i++) {
        const a = pitchSeries[i - 1];
        const b = pitchSeries[i];
        if (typeof a !== "number" || typeof b !== "number") continue;
        const d = b - a;
        if (Math.abs(d) < DEADBAND_DEG) continue;
        travel += Math.abs(d);
        const dir = d > 0 ? 1 : -1;
        if (lastDir !== 0 && dir !== lastDir) reversals++;
        lastDir = dir;
    }
    const s = statsOf(pitchSeries);
    const range = s ? s.range : 0;
    let verdict;
    if (reversals >= REVERSAL_MIN && range >= RANGE_MIN_DEG) verdict = "往复运动(振荡)";
    else if (range < STILL_RANGE_DEG) verdict = "静止(无明显运动)";
    else verdict = "单向/慢漂(无往复)";
    return { reversals, travel, range, verdict };
}

function fmt(v, digits = 3) {
    if (typeof v !== "number" || !Number.isFinite(v)) return "-";
    return v.toFixed(digits);
}

async function main() {
    const opt = parseArgs(process.argv.slice(2));
    if (opt.help) {
        console.log('用法: node tools/pitch_trace.js [--seconds 10] [--interval 50] [--out build/pitch_trace.csv] [--label 名称] [--vars "a,b"]');
        return 0;
    }

    // 采样本前先确认"没人在暂停核心":留着一个 paused 的调试会话会让所有值冻结成常数。
    let status = null;
    try {
        status = await call(WORKSPACE, "debug.status", {}, 15000);
    } catch {
        status = null;
    }
    if (status && status.state === "paused") {
        console.error("调试会话当前是 paused —— 读数会冻结。先 node .agents/skills/mcu-debug-control/scripts/debug.js --workspace . --continue,或 --stop 释放会话。");
        return 1;
    }

    const count = Math.max(2, Math.min(MAX_COUNT, Math.round((opt.seconds * 1000) / opt.interval)));
    const t0 = Date.now();
    const result = await call(
        WORKSPACE,
        "variables.sample",
        { variables: opt.vars.map((name) => ({ name })), count, intervalMs: opt.interval },
        Math.min(2147483647, count * opt.interval + 30000)
    );
    const dur = (Date.now() - t0) / 1000;

    const samples = Array.isArray(result.samples) ? result.samples : [];
    if (!samples.length) {
        console.error("采样返回 0 个样本 —— 检查探针/ELF 绑定(node tools/doctor.js)。");
        return 1;
    }

    // CSV:一行一个采样点,列顺序与 --vars 一致,便于直接丢进 Excel/画图。
    const cols = opt.vars.map(shortName);
    const outPath = path.resolve(WORKSPACE, opt.out);
    fs.mkdirSync(path.dirname(outPath), { recursive: true });
    const lines = [`t_ms,elapsed_s,${cols.join(",")}`];
    const baseTs = samples[0].timestamp;
    for (const sample of samples) {
        const cells = opt.vars.map((name) => {
            const v = sample.values?.[name]?.value;
            return typeof v === "number" && Number.isFinite(v) ? String(v) : "";
        });
        const rel = (sample.timestamp ?? baseTs) - baseTs;
        lines.push(`${rel},${(rel / 1000).toFixed(3)},${cells.join(",")}`);
    }
    fs.writeFileSync(outPath, lines.join("\n") + "\n", "utf8");

    const seriesOf = (name) => samples.map((s) => s.values?.[name]?.value);
    const pitchVar = opt.vars.find((v) => /(^INS\.Pitch$|->measure\.angle)/.test(v)) || opt.vars[0];
    const pitchSeries = seriesOf(pitchVar);
    const ps = statsOf(pitchSeries);
    const judge = judgeOscillation(pitchSeries);
    const outVar = opt.vars.find((v) => /speed_PID\.Output$/.test(v));
    const outStats = outVar ? statsOf(seriesOf(outVar)) : null;
    const curVar = opt.vars.find((v) => /real_current$/.test(v));
    const curStats = curVar ? statsOf(seriesOf(curVar)) : null;
    const actualHz = samples.length / dur;

    console.log(`采样完成: ${samples.length} 点 / ${dur.toFixed(2)}s → 实际 ${actualHz.toFixed(1)} Hz(请求 ${(1000 / opt.interval).toFixed(1)} Hz)`);
    if (opt.label) console.log(`标签: ${opt.label}`);
    console.log(`CSV: ${opt.out}`);
    if (ps) {
        console.log(`\n[pitch 角度 ${shortName(pitchVar)}]`);
        console.log(`  首 ${fmt(ps.first)}° → 末 ${fmt(ps.last)}°  均 ${fmt(ps.mean)}°  min ${fmt(ps.min)}°  max ${fmt(ps.max)}°`);
        console.log(`  峰峰 ${fmt(ps.range)}°  累计行程 ${fmt(judge.travel)}°  方向翻转 ${judge.reversals} 次`);
        console.log(`  判定: ${judge.verdict}`);
    }
    if (outStats) console.log(`[速度环 Output] min ${fmt(outStats.min, 0)}  max ${fmt(outStats.max, 0)}  均 ${fmt(outStats.mean, 0)}`);
    if (curStats) console.log(`[电流码] min ${fmt(curStats.min, 0)}  max ${fmt(curStats.max, 0)}  均 ${fmt(curStats.mean, 0)}`);
    console.log(`\n判定口径: 翻转≥${REVERSAL_MIN} 且峰峰≥${RANGE_MIN_DEG}° → 振荡;峰峰<${STILL_RANGE_DEG}° → 静止。`);
    return 0;
}

main()
    .then((code) => process.exit(code))
    .catch((e) => {
        console.error(e.stack || e.message);
        process.exit(1);
    });
