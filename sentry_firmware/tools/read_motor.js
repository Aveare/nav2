"use strict";
/**
 * @file  tools/read_motor.js
 * @brief 通用"读任意一个 DJIMotorInstance 的反馈"工具:pitch / friction / loader 等都能读。
 *
 * 为什么需要它:
 *   `tools/read_yaw.js` 只服务 yaw 标定;排查其它电机(C620 有无反馈、摩擦轮在不在线)时
 *   需要同一套解码。故把"读指针 → memory.read 464B → 按偏移解码"抽成通用工具,
 *   电机实例变量名由命令行给。
 *
 * 为什么不用 `variables.read xxx->measure.ecd`:
 *   本工程 ELF 经 LTO 后复合类型布局在 DWARF 中不可用,`->` 会被当成"指针变量自身+偏移",
 *   静默返回垃圾。variables.read 只支持顶层标量 → 先读指针,再 memory.read 按偏移解码
 *   (偏移取自 arm-none-eabi-gdb `ptype /o DJIMotorInstance`,与 tools/read_chassis.js 同套)。
 *
 * 在线判据:dt = 距上一帧 CAN 反馈的时间(DWT_GetDeltaT),<5ms 才认为电机在持续发帧。
 *   电机掉线时 ecd/angle 全是上电时的冻结值(或 0),**不能用来标定**;本工具会标"掉线"。
 *
 * 已知电机实例变量(static 也能按名字读):
 *   yaw_motor / pitch_motor      application/gimbal/gimbal.c:224-225
 *   friction_l / friction_r / loader   application/shoot/shoot.c:29 起
 *   motor_lf / motor_rf / motor_lb / motor_rb   application/chassis/chassis.c
 *
 * 用法:
 *   node tools/read_motor.js pitch_motor
 *   node tools/read_motor.js pitch_motor --json
 *   node tools/read_motor.js yaw_motor friction_l friction_r      (多个名字)
 */

const path = require("path");
const { call } = require(path.join(__dirname, "..", ".agents", "skills", "_emberprobe", "agent-client.js"));

const WORKSPACE = path.resolve(__dirname, "..");

// DJIMotorInstance 内的偏移(与 tools/read_chassis.js、read_yaw.js 一致,gdb ptype /o 求得)
const OFFSET = { lastEcd: 0, ecd: 2, angle: 4, speed: 8, current: 12, temp: 14, totalAngle: 16, totalRound: 20, feedCnt: 452, dt: 456 };
const INSTANCE_BYTES = 464;

const ONLINE_DT_MS = 5; // 反馈帧间隔阈值,>5ms 视为掉线
const ECD_TO_DEG = 360 / 8192; // ECD_ANGLE_COEF_DJI
const BOOT_MS_VAR = "chassis_last_ms"; // DWT 毫秒时间线:核心在跑才会增长

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/** 读一个标量全局变量 */
async function readVar(name) {
    const r = await call(WORKSPACE, "variables.read", { variables: [{ name }] }, 20000);
    return r.values?.[name]?.value ?? null;
}

/**
 * 恢复目标并确认它真的在跑:连发 continue,直到 status 报 running 且 chassis_last_ms 两次读数在增长。
 * 必须循环校验:本工程 variables.read/memory.read 是侵入式读取,偶发残留暂停会让 continue
 * 看起来生效、核心仍停着 —— 实测中把板子留在 paused 里过,之后所有读数都是冻结值。
 */
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

/** 解码一个电机实例的 464 字节 */
function decode(buf) {
    const ecd = buf.readUInt16LE(OFFSET.ecd);
    const dtMs = buf.readFloatLE(OFFSET.dt) * 1000;
    return {
        lastEcd: buf.readUInt16LE(OFFSET.lastEcd),
        ecd,
        angleDeg: Number(buf.readFloatLE(OFFSET.angle).toFixed(3)),
        angleFromEcd: Number((ecd * ECD_TO_DEG).toFixed(3)), // 交叉校验:两者应一致(差>1° 说明正在转)
        speed: Number(buf.readFloatLE(OFFSET.speed).toFixed(1)),
        current: buf.readInt16LE(OFFSET.current),
        temp: buf[OFFSET.temp],
        totalAngle: Number(buf.readFloatLE(OFFSET.totalAngle).toFixed(2)),
        totalRound: buf.readInt32LE(OFFSET.totalRound),
        dtMs: Number(dtMs.toFixed(3)),
        online: dtMs > 0 && dtMs < ONLINE_DT_MS
    };
}

/** 读一批电机:暂停一次读全部,再统一恢复*/
async function readMotors(names) {
    const status = await call(WORKSPACE, "debug.status", null, 20000);
    if (!status.session) {
        throw new Error("没有活动的调试会话。先执行: node .agents/skills/mcu-debug-control/scripts/debug.js --workspace <工程> --start --attach");
    }
    if (status.paused) {
        const r = await resumeAndVerify();
        if (r && r.failed) throw new Error(`目标处于暂停状态且无法恢复(${r.lastErr || "未知"}),读数会全是冻结值`);
    }

    await call(WORKSPACE, "debug.control", { action: "pause" }, 30000);
    const out = { ok: true, at: {}, motors: {} };
    try {
        const ptrRes = await call(WORKSPACE, "variables.read", { variables: names.map((n) => ({ name: n })) }, 30000);
        for (const name of names) {
            const ptr = ptrRes.values?.[name]?.value;
            if (typeof ptr !== "number" || ptr === 0) {
                out.motors[name] = { ok: false, message: `指针读取失败或为 0(${JSON.stringify(ptrRes.values?.[name])}):电机未上电/未初始化` };
                continue;
            }
            const r = await call(WORKSPACE, "memory.read", { address: ptr, length: INSTANCE_BYTES }, 30000);
            const buf = Array.isArray(r?.bytes) ? Buffer.from(r.bytes) : Buffer.from(String(r?.hex || ""), "hex");
            if (buf.length < INSTANCE_BYTES) {
                out.motors[name] = { ok: false, message: `内存读取不完整 ${buf.length}/${INSTANCE_BYTES}` };
                continue;
            }
            out.motors[name] = { ok: true, ptr: "0x" + ptr.toString(16), ...decode(buf) };
        }
        out.at.bootMs = await readVar(BOOT_MS_VAR);
    } finally {
        // 恢复交给下面的 resumeAndVerify(需要校验,不能只发一次 continue)
    }

    const resumed = await resumeAndVerify();
    if (resumed && resumed.failed) {
        out.ok = false;
        out.message = `恢复目标失败(${resumed.lastErr || "未知"}),读数可能是冻结值`;
    } else {
        out.at.bootMsAfter = resumed.bootMs;
    }
    return out;
}

(async () => {
    const args = process.argv.slice(2).filter((a) => !a.startsWith("--"));
    const wantJson = process.argv.includes("--json");
    if (args.length === 0) {
        process.stderr.write("用法: node tools/read_motor.js <电机实例变量名...> [--json]\n例:  node tools/read_motor.js pitch_motor\n");
        process.exitCode = 2;
        return;
    }

    const out = await readMotors(args);

    if (wantJson) {
        process.stdout.write(JSON.stringify(out, null, 2) + "\n");
    } else {
        const lines = [];
        for (const name of args) {
            const m = out.motors[name];
            if (!m || m.ok === false) {
                lines.push(`${name}: 读不到 —— ${m?.message || "未知原因"}`);
                continue;
            }
            lines.push(
                `${name}: ${m.online ? "在线" : "掉线(dt=" + m.dtMs + "ms)"}  ptr=${m.ptr}  ` +
                `ecd=${m.ecd}  单圈角=${m.angleDeg}°  (由 ecd 推=${m.angleFromEcd}°)  ` +
                `累计=${m.totalAngle}°  温度=${m.temp}℃  电流=${m.current}  转速=${m.speed}`
            );
        }
        if (out.message) lines.push(`⚠ ${out.message}`);
        process.stdout.write(lines.join("\n") + "\n");
    }
    if (!out.ok) process.exitCode = 1;
})().catch((error) => {
    process.stdout.write(JSON.stringify({ ok: false, code: error.code || null, message: error.message, details: error.details || null }, null, 2) + "\n");
    process.exitCode = 1;
});
