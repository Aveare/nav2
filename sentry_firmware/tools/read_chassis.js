"use strict";
/**
 * @file  tools/read_chassis.js
 * @brief 底盘状态一览:四轮反馈 + 轮速逆解 + 指令。走 EmberProbe skill,不依赖 MCP 客户端。
 *
 * 为什么不用 `variables.read --variables motor_lf->measure.speed_aps`:
 *   本工程 ELF 经 LTO 后复合类型布局在 DWARF 中不可用,该工具把 `->` 当成"指针变量自身 + 字段偏移",
 *   读出来是垃圾值(实测报的地址与 motor_lf 变量地址完全相同)。variables.read 只支持顶层标量。
 *   因此分两步:① variables.read 读四个电机**指针**(u32);② memory.read 按指针读实例内存再按偏移解码。
 *
 * 为什么必须先挂起:memory.read 仅在暂停的调试会话里可用(否则 PAUSE_REQUIRED)。
 *   本脚本会自动 pause -> 读 -> 恢复原状态。**恢复失败会让目标一直停着,此后的读数全是冻结值**
 *   (2026-11-15 实际踩过:车被推了 2 米,total_angle 一个数没动)。
 *   所以读完必须做新鲜度校验:chassis_last_ms(DWT 时间线,单调增)恢复后必须比暂停时大;
 *   不增长即判定"目标没在跑",输出 ok:false,别拿这份数据下结论。
 *
 * 在线判据:dt = 距上一帧 CAN 反馈的时间(DWT_GetDeltaT),<5ms 即在持续收帧。
 *   注意 motor->feed_cnt 实际是 DWT->CYCCNT(周期数),**不是帧计数也不是开机时间**;
 *   32 位 @168MHz 约 25.6s 回绕一次,既不能当在线判据也不能当 uptime(旧版这里打印的 uptimeS 是错的)。
 *
 * 结构体偏移(DJI_Motor_Measure_s / DJIMotorInstance,arm-none-eabi-gdb `ptype /o` 求得):
 *   last_ecd +0(u16) | ecd +2(u16) | angle_single_round +4(f32) | speed_aps +8(f32)
 *   real_current +12(i16) | temperature +14(u8) | total_angle +16(f32) | total_round +20(i32)
 *   motor_can_instance +440 | daemon +448 | feed_cnt +452(u32, DWT CYCCNT) | dt +456(f32)
 *
 * 用法: node tools/read_chassis.js [--raw]
 */

const path = require("path");
const { call } = require(path.join(__dirname, "..", ".agents", "skills", "_emberprobe", "agent-client.js"));

const WORKSPACE = path.resolve(__dirname, "..");
const OFFSET = { ecd: 2, angle: 4, speed: 8, current: 12, temp: 14, totalAngle: 16, totalRound: 20, feedCnt: 452, dt: 456 };
const MOTORS = ["motor_lf", "motor_rf", "motor_lb", "motor_rb"];
const LABEL = { motor_lf: "LF", motor_rf: "RF", motor_lb: "LB", motor_rb: "RB" };
const ONLINE_DT_MS = 5; // 反馈帧间隔阈值,>5ms 视为已掉线
const FRESHNESS_MS = 250; // 恢复后等待时长,足够 ChassisTask 跑几十个周期

function decode(buf) {
    return {
        ecd: buf.readUInt16LE(OFFSET.ecd),
        angle: buf.readFloatLE(OFFSET.angle),
        speed: buf.readFloatLE(OFFSET.speed),
        current: buf.readInt16LE(OFFSET.current),
        temp: buf[OFFSET.temp],
        totalAngle: buf.readFloatLE(OFFSET.totalAngle),
        totalRound: buf.readInt32LE(OFFSET.totalRound),
        feedCnt: buf.readUInt32LE(OFFSET.feedCnt),
        dtMs: buf.readFloatLE(OFFSET.dt) * 1000
    };
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
    const wantRaw = process.argv.includes("--raw");

    const status = await call(WORKSPACE, "debug.status", null, 20000);
    if (!status.session) throw new Error("没有活动的调试会话。先执行: node .agents/skills/mcu-debug-control/scripts/debug.js --workspace <工程> --start --attach");
    const wasPaused = Boolean(status.paused);
    if (!wasPaused) await call(WORKSPACE, "debug.control", { action: "pause" }, 30000);

    const result = { ok: true, rows: [], extra: null };
    try {
        const ptrRes = await call(WORKSPACE, "variables.read", { variables: MOTORS.map((n) => ({ name: n })) }, 30000);
        for (const name of MOTORS) {
            const ptr = ptrRes.values?.[name]?.value;
            if (typeof ptr !== "number") throw new Error(`${name} 指针读取失败: ${JSON.stringify(ptrRes.values?.[name])}`);
            const r = await call(WORKSPACE, "memory.read", { address: ptr, length: 464 }, 30000);
            const buf = Array.isArray(r?.bytes) ? Buffer.from(r.bytes) : Buffer.from(String(r?.hex || ""), "hex");
            if (buf.length < 464) throw new Error(`${name} 内存读取不完整 ${buf.length}/464`);
            const m = decode(buf);
            result.rows.push({
                wheel: LABEL[name],
                ptr: "0x" + ptr.toString(16),
                ecd: m.ecd,
                angle: Number(m.angle.toFixed(2)),
                totalAngle: Number(m.totalAngle.toFixed(1)),
                speed: Number(m.speed.toFixed(1)),
                current: m.current,
                temp: m.temp,
                dtMs: Number(m.dtMs.toFixed(3)),
                online: m.dtMs > 0 && m.dtMs < ONLINE_DT_MS
            });
        }

        const names = ["chassis_last_ms"];
        if (wantRaw) names.push("est_vx", "est_vy", "est_wz", "vt_lf", "vt_rf", "vt_lb", "vt_rb", "chassis_vx", "chassis_vy");
        const res = await call(WORKSPACE, "variables.read", { variables: names.map((n) => ({ name: n })) }, 30000);
        const picked = Object.fromEntries(names.map((n) => [n, res.values?.[n]?.value ?? null]));
        result.chassisLastMsAtRead = picked.chassis_last_ms;
        delete picked.chassis_last_ms;
        if (wantRaw) result.extra = picked;
    } finally {
        if (!wasPaused) {
            try {
                await call(WORKSPACE, "debug.control", { action: "continue" }, 30000);
            } catch (e) {
                result.resumeError = e.message;
            }
        }
    }

    // 新鲜度校验:目标必须真的在跑,否则这一份读数可能是冻结的旧值
    await sleep(FRESHNESS_MS);
    const after = await call(WORKSPACE, "variables.read", { variables: [{ name: "chassis_last_ms" }] }, 20000);
    const t1 = after.values?.chassis_last_ms?.value;
    result.chassisLastMs = { atRead: result.chassisLastMsAtRead, afterResume: t1 };
    delete result.chassisLastMsAtRead;
    const alive = typeof t1 === "number" && typeof result.chassisLastMs.atRead === "number" && t1 > result.chassisLastMs.atRead;
    result.targetAlive = alive;
    if (!alive || result.resumeError) {
        result.ok = false;
        result.message = result.resumeError
            ? `恢复目标失败(${result.resumeError}),读数可能是冻结值`
            : "目标没有在运行(chassis_last_ms 未增长),读数可能是冻结值;先 continue 再重读";
    }
    process.stdout.write(JSON.stringify(result, null, 2) + "\n");
    if (!result.ok) process.exitCode = 1;
})().catch((error) => {
    process.stdout.write(JSON.stringify({ ok: false, code: error.code || null, message: error.message, details: error.details || null }, null, 2) + "\n");
    process.exitCode = 1;
});
