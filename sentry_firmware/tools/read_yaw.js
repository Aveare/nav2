"use strict";
/**
 * @file  tools/read_yaw.js
 * @brief 读云台 yaw 电机单圈角,并给出 `YAW_CHASSIS_ALIGN_ECD` 的标定建议值。
 *
 * 用途(为什么需要它):
 *   `CalcOffsetAngle()`(application/cmd/robot_cmd.c:283-292)算
 *       offset = yaw_motor_single_round_angle − YAW_ALIGN_ANGLE
 *   其中 YAW_ALIGN_ANGLE = YAW_CHASSIS_ALIGN_ECD × ECD_ANGLE_COEF_DJI(robot_cmd.c:27),
 *   ECD_ANGLE_COEF_DJI = 360/8192 = 0.043945。
 *   这个 offset 同时驱动两件事:
 *     ① 解锁回正(robot_cmd.c:492-514,GIMBAL_RECENTER_SPEED 90°/s,超时 600×5ms=3s);
 *     ② 中挡底盘跟随-CHASSIS_FOLLOW_GIMBAL_YAW(chassis.c:258-288)。
 *   **所以云台装好/拆装后只要把 YAW_CHASSIS_ALIGN_ECD 重标成"云台正对车头时读到的 ecd",两者一起修好。**
 *   宏值失真时的典型现象:拨中档 yaw 电机持续自转(回正追不上 270°/3s 的上限)、底盘同时自转。
 *
 * 标定步骤(需要人配合,约 1 分钟):
 *   1. 云台上电、遥控右上挡(kill switch,全零力,电机不抗力);
 *   2. 手动把云台转到**与车头正前方对齐**并保持不动(以车体纵轴为准,别以云台自己为准);
 *   3. `node tools/read_yaw.js` → 看输出里的 `suggest.alignEcd`,写进
 *      `config/robot_infantry_2026.h` 的 `YAW_CHASSIS_ALIGN_ECD`;
 *   4. 重新编译烧录后,拨中档验证:云台应在 ≤3s 内回正到车头方向并停住,底盘不自转。
 *   5. 若回正方向反了(离目标越转越远),改 `GIMBAL_RECENTER_DIR` 的符号,不要改这个 ecd。
 *
 * 为什么不用 `variables.read yaw_motor->measure.ecd`:
 *   本工程 ELF 经 LTO 后复合类型布局在 DWARF 中不可用,`->` 会被当成"指针变量自身+偏移",
 *   静默返回垃圾。variables.read 只支持顶层标量 → 先读指针,再 memory.read 按偏移解码
 *   (与 tools/read_chassis.js 同一套偏移,取自 arm-none-eabi-gdb `ptype /o`)。
 *
 * 在线判据:dt = 距上一帧 CAN 反馈的时间(DWT_GetDeltaT),<5ms 才认为 yaw 电机在持续发帧;
 *   离线时 ecd 是冻结的旧值,标定出来的宏一定是错的 → 本工具直接 ok:false。
 *
 * 用法: node tools/read_yaw.js [--json]
 *   默认打印人类可读摘要;--json 打印原始 JSON(便于落盘/对照)。
 */

const fs = require("fs");
const path = require("path");
const { call } = require(path.join(__dirname, "..", ".agents", "skills", "_emberprobe", "agent-client.js"));

const WORKSPACE = path.resolve(__dirname, "..");
const CONFIG_HEADER = path.join(WORKSPACE, "config", "robot_infantry_2026.h");

// DJIMotorInstance 内的偏移(与 tools/read_chassis.js 一致,gdb ptype /o 求得)
const OFFSET = { lastEcd: 0, ecd: 2, angle: 4, speed: 8, current: 12, temp: 14, totalAngle: 16, totalRound: 20, feedCnt: 452, dt: 456 };
const INSTANCE_BYTES = 464;

const YAW_PTR_VAR = "yaw_motor"; // application/gimbal/gimbal.c:224 yaw_motor = DJIMotorInit(&yaw_config)
const ONLINE_DT_MS = 5; // 反馈帧间隔阈值,>5ms 视为掉线
const ECD_TO_DEG = 360 / 8192; // ECD_ANGLE_COEF_DJI,application/cmd/robot_cmd.c
const DEG_TO_ECD = 8192 / 360;

const BOOT_MS_VAR = "chassis_last_ms"; // DWT 毫秒时间线,单调增:核心在跑才会增长

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const wrap180 = (deg) => ((((deg + 180) % 360) + 360) % 360) - 180;

/** 读一个标量全局变量(不自己暂停:调用方负责在暂停窗口内或运行态下调用) */
async function readVar(name) {
    const r = await call(WORKSPACE, "variables.read", { variables: [{ name }] }, 20000);
    return r.values?.[name]?.value ?? null;
}

/**
 * 恢复目标并**确认它真的在跑**(与 tools/odom.js 的 resumeAndVerify 同款)。
 * 必须循环校验:本工程 variables.read/memory.read 是侵入式读取,偶发的读取失败或残留暂停
 * 会让 continue 看起来生效、实际核心仍停着 —— 实测留下过 paused 的目标,
 * 以至于下一轮读数全是冻结值(2026-11-15 推 2 米读数没动那次)。
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

/** 从 config 头文件读当前宏值,用来算"现在的 offset 残差有多大"。 */
function readCurrentAlignEcd() {
    try {
        const text = fs.readFileSync(CONFIG_HEADER, "utf8");
        const m = text.match(/^[ \t]*#define[ \t]+YAW_CHASSIS_ALIGN_ECD[ \t]+\(?[ \t]*([-+]?[0-9]*\.?[0-9]+)/m);
        return m ? Number(m[1]) : null;
    } catch {
        return null;
    }
}

(async () => {
    const wantJson = process.argv.includes("--json");

    const status = await call(WORKSPACE, "debug.status", null, 20000);
    if (!status.session) {
        throw new Error("没有活动的调试会话。先执行: node .agents/skills/mcu-debug-control/scripts/debug.js --workspace <工程> --start --attach");
    }
    const wasPaused = Boolean(status.paused);
    if (!wasPaused) await call(WORKSPACE, "debug.control", { action: "pause" }, 30000);

    const out = { ok: true, yaw: null, config: null, suggest: null, chassisLastMs: null, targetAlive: null };
    try {
        const ptrRes = await call(WORKSPACE, "variables.read", { variables: [{ name: YAW_PTR_VAR }] }, 30000);
        const ptr = ptrRes.values?.[YAW_PTR_VAR]?.value;
        if (typeof ptr !== "number" || ptr === 0) {
            throw new Error(`${YAW_PTR_VAR} 指针读取失败(${JSON.stringify(ptrRes.values?.[YAW_PTR_VAR])});云台未上电或云台任务未初始化`);
        }
        const r = await call(WORKSPACE, "memory.read", { address: ptr, length: INSTANCE_BYTES }, 30000);
        const buf = Array.isArray(r?.bytes) ? Buffer.from(r.bytes) : Buffer.from(String(r?.hex || ""), "hex");
        if (buf.length < INSTANCE_BYTES) throw new Error(`yaw 电机内存读取不完整 ${buf.length}/${INSTANCE_BYTES}`);

        const ecd = buf.readUInt16LE(OFFSET.ecd);
        const angleDeg = buf.readFloatLE(OFFSET.angle); // measure.angle_single_round,度
        const dtMs = buf.readFloatLE(OFFSET.dt) * 1000;
        const online = dtMs > 0 && dtMs < ONLINE_DT_MS;

        out.yaw = {
            ptr: "0x" + ptr.toString(16),
            lastEcd: buf.readUInt16LE(OFFSET.lastEcd),
            ecd,
            angleDeg: Number(angleDeg.toFixed(3)),
            angleFromEcd: Number((ecd * ECD_TO_DEG).toFixed(3)), // 交叉校验:两者应一致
            totalAngle: Number(buf.readFloatLE(OFFSET.totalAngle).toFixed(2)),
            totalRound: buf.readInt32LE(OFFSET.totalRound),
            speed: Number(buf.readFloatLE(OFFSET.speed).toFixed(1)),
            current: buf.readInt16LE(OFFSET.current),
            temp: buf[OFFSET.temp],
            dtMs: Number(dtMs.toFixed(3)),
            online
        };

        const alignEcd = readCurrentAlignEcd();
        const alignDeg = alignEcd == null ? null : alignEcd * ECD_TO_DEG;
        out.config = {
            macro: "YAW_CHASSIS_ALIGN_ECD",
            alignEcd,
            alignDeg: alignDeg == null ? null : Number(alignDeg.toFixed(3)),
            offsetNowDeg: alignDeg == null ? null : Number(wrap180(angleDeg - alignDeg).toFixed(2)),
            recenterSpeedDps: 90,
            recenterTimeoutS: 3
        };

        // 建议值:对齐时读到的 ecd 就是答案;再用度数反算一遍做交叉校验
        const fromEcd = ecd;
        const fromDeg = Math.round(angleDeg * DEG_TO_ECD);
        const suggested = fromDeg; // 用度数(与固件 CalcOffsetAngle 的输入同源),fromEcd 仅作校验
        out.suggest = {
            alignEcd: suggested,
            alignEcdFromRawEcd: fromEcd,
            agree: Math.abs(suggested - fromEcd) <= 1,
            alignDeg: Number((suggested * ECD_TO_DEG).toFixed(3)),
            offsetWithNewDeg: Number(wrap180(angleDeg - suggested * ECD_TO_DEG).toFixed(3)),
            deltaEcd: alignEcd == null ? null : suggested - alignEcd,
            note: "把 alignEcd 写进 config/robot_infantry_2026.h 的 YAW_CHASSIS_ALIGN_ECD;重新编译烧录后拨中档验证"
        };

        // 在线判据不满足时 ecd 可能是冻结值,标定结果不可信
        if (!online) {
            out.ok = false;
            out.message = `yaw 电机反馈掉线(dt=${out.yaw.dtMs}ms > ${ONLINE_DT_MS}ms),ecd 可能是冻结的旧值:检查云台 CAN、24V 供电与 gimbal 任务是否在跑`;
        }

        out.chassisLastMs = await readVar(BOOT_MS_VAR);
    } finally {
        // 恢复统一交给下面的 resumeAndVerify():必须循环校验"核心真的在跑"。
        // 只发一次 continue 会出现"看着恢复、实际仍停着"的残留暂停 —— 本工程实测踩过,
        // 上一次读 yaw 就把目标留在 paused,下一次读数全是冻结值。
    }

    const resumed = await resumeAndVerify();
    if (resumed && resumed.failed) {
        out.ok = false;
        out.targetAlive = false;
        out.resumeError = resumed.lastErr || "未知";
        out.message = `恢复目标失败(${out.resumeError}),读数可能是冻结值`;
    } else {
        out.targetAlive = true;
        out.chassisLastMsAfter = resumed.bootMs;
    }

    if (wantJson) {
        process.stdout.write(JSON.stringify(out, null, 2) + "\n");
    } else {
        const y = out.yaw || {};
        const c = out.config || {};
        const s = out.suggest || {};
        const lines = [
            `yaw 电机:${y.online ? "在线" : "掉线"}  ecd=${y.ecd}  单圈角=${y.angleDeg}°  (由 ecd 推=${y.angleFromEcd}°)  累计=${y.totalAngle}°  温度=${y.temp}℃  电流=${y.current}`,
            `当前宏:YAW_CHASSIS_ALIGN_ECD=${c.alignEcd} → ${c.alignDeg}°  此刻 offset=${c.offsetNowDeg}°`,
            `建议宏:YAW_CHASSIS_ALIGN_ECD=${s.alignEcd} → ${s.alignDeg}°  (校准后 offset=${s.offsetWithNewDeg}°,相差 ${s.deltaEcd} 计数)`,
            s.agree ? "交叉校验:用 ecd 与用度数的结果一致" : "交叉校验:ecd 与角度反算不一致(差>1 计数),先确认没在转动中读数"
        ];
        if (!out.ok) lines.push(`⚠ ${out.message}`);
        process.stdout.write(lines.join("\n") + "\n");
    }
    if (!out.ok) process.exitCode = 1;
})().catch((error) => {
    process.stdout.write(JSON.stringify({ ok: false, code: error.code || null, message: error.message, details: error.details || null }, null, 2) + "\n");
    process.exitCode = 1;
});
