"use strict";
/**
 * @file  tools/odom.js
 * @brief 零力推车的轮端里程计:推车前 mark、推车后 delta,用电机 total_angle 算实际位移
 *
 * 为什么不用 ∫est_vx dt(采样法):
 *   采样法有两个无解的空洞。① PC 侧每次读变量都要临时连一次探针(~330ms),t 间隔不可控;
 *   ② 一旦中间丢一个点(实测 build/sample_straight.csv 第 68→69 行间隔 3475ms),
 *   梯形积分要么截断(少算),要么跨越空洞补 (v_last+0)/2*dt(多算 842mm)——真值不可知。
 *   total_angle 在车端由 CAN 反馈逐帧累加(dji_motor.c:168-172),零力下照常更新,
 *   只在推车前/后各读一次快照,中间没有采样空窗,因此是这一测的唯一可靠量。
 *
 * 换算链(与 chassis.c 正/逆解同一条,改一处必须三处同步):
 *   转子 deg → 四轮等效线速度 u_i = total_angle / (WHEEL_RATIO * 0.70710678),单位 mm
 *   车身位移 = ((Δ_rf + Δ_rb) - (Δ_lf + Δ_lb)) / 4 / (WHEEL_RATIO * 0.70710678)
 *   WHEEL_RATIO = 360 * REDUCTION_RATIO_WHEEL / (2π*RADIUS_WHEEL) = 14.516 deg/mm
 *   → 每转子度 0.097417mm 车身位移;2000mm 对应 |Δ| = 20523 deg/轮
 *
 * 为什么不能"四轮取平均":本车左右电机安装镜像,**正向前进时 LF/LB 转子角递减、RF/RB 递增**。
 *   符号约定来自 chassis.c:186-189 正解(vx 项对 lf/lb 取负、对 rf/rb 取正;该整体取反是实车标定结果),
 *   与 EstimateSpeed() chassis.c:231 的组合式严格对应。所以下面按 (lf,rf,lb,rb)=(-1,+1,-1,+1) 加权,
 *   再取四轮等价值的中位数——直接对四个原始 Δ 取平均会得到 0。
 *
 * 三个会让读数失效的坑:
 *   ① 板子重启会把 total_angle 清零 → delta 比对 chassis_last_ms 回退并报警;
 *   ③ **核心被调试器停住时 CAN 中断不跑,total_angle 一个数都不会变**——车真推了 2 米,delta 也会算出 0
 *      (2026-11-15 实车踩过:read_chassis 的 continue 静默失败,目标一直 paused)。
 *      所以读完后必须验新鲜度:恢复后 250ms 再读 chassis_last_ms,不增长就报错,绝不返回可疑的 0;
 *   ② total_angle 是 float32,total_round 溢出前长时间驾驶会让分辨率变粗(>16M deg 时 1deg≈0.1mm),可接受。
 *
 * 常量来源:运行时解析 config/robot_infantry_2026.h,不复制数值,免得改了参数这里悄悄过期。
 *
 * 用法:
 *   node tools/odom.js mark B1-slow                     # 起点打标(车静止、四轮着地)
 *   node tools/odom.js delta B1-slow --expect 2000      # 前后直线,按 2000mm 真值给误差
 *   node tools/odom.js delta B4-vy --axis vy --expect 2000  # 横移:换 vy 权重(前后对反向)
 *   node tools/odom.js delta B5-turn --axis wz --expect 360 # 原地转:输出偏航角 deg(不是 mm)
 *   node tools/odom.js show                             # 查看已存的起点标记
 *
 * 三个轴的区别只在权重:直线 (rf+rb)-(lf+lb)、横移 (lb+rb)-(lf+rf)、旋转四轮同号取负。
 * 手推横移时车会往 +y 或 -y 走,读数的符号直接告诉你本车 +vy 指向哪一侧(已实测:**+vy = 车头左侧**,RM 标准)。
 * 旋转轴的换算多一个力臂:θ = Σ(-Δ_i) /(4·WHEEL_RATIO·CENTER·0.7071),CENTER = √((WHEEL_BASE/2)²+(TRACK_WIDTH/2)²)。
 */

const fs = require("fs");
const path = require("path");
const { call } = require(path.join(__dirname, "..", ".agents", "skills", "_emberprobe", "agent-client.js"));

const WORKSPACE = path.resolve(__dirname, "..");
const STATE_FILE = path.join(WORKSPACE, "build", "odom_marks.json");
const CONFIG_H = path.join(WORKSPACE, "config", "robot_infantry_2026.h");
const MOTORS = ["motor_lf", "motor_rf", "motor_lb", "motor_rb"];
const LABEL = { motor_lf: "LF", motor_rf: "RF", motor_lb: "LB", motor_rb: "RB" };
// 各轴的正解权重:距离 = Σ(sign_i * Δ_i)/4 * mmPerDeg,与下方注释的固件公式严格同构
//   vx: ((Δ_rf+Δ_rb) - (Δ_lf+Δ_lb))/4  ← chassis.c:186-189 正解 vx 项
//   vy: ((Δ_lb+Δ_rb) - (Δ_lf+Δ_rf))/4  ← 正解 vy 项(前对一方向、后对反方向,麦轮横移特征)
const AXES = {
    vx: { sign: { LF: -1, RF: 1, LB: -1, RB: 1 }, name: "直线(前后)" },
    vy: { sign: { LF: -1, RF: -1, LB: 1, RB: 1 }, name: "横移(左右)" },
    // wz:正解里四轮恒为 -wz*CENTER(无正负号分化),所以四个权重都是 -1,输出量是**偏航角(deg)**不是位移 mm
    wz: { sign: { LF: -1, RF: -1, LB: -1, RB: -1 }, name: "旋转(偏航)", kind: "yaw" }
};
const TOTAL_ANGLE_OFFSET = 16; // DJIMotorInstance.measure.total_angle,float32,度
const HALF_SQRT2 = 0.70710678;
// 重启判据用 chassis_last_ms(DWT 时间线,开机上电后单调增),不能用 motor->feed_cnt:
// 后者是 DWT->CYCCNT 周期数,25.6s 就回绕一次(实测读数 91 秒处反复出现同一个值),拿它判重启会随机误报。
const BOOT_MS_VAR = "chassis_last_ms";

/** 从 config 头文件取两个宏,保证与固件同源;取不到就直接报错,不猜默认值 */
function readGeometry() {
    const text = fs.readFileSync(CONFIG_H, "utf8");
    const pick = (name) => {
        const m = text.match(new RegExp(`#define\\s+${name}\\s+([0-9.]+)`));
        if (!m) throw new Error(`${path.basename(CONFIG_H)} 里找不到 ${name}`);
        return parseFloat(m[1]);
    };
    const radius = pick("RADIUS_WHEEL");
    const reduction = pick("REDUCTION_RATIO_WHEEL");
    const wheelBase = pick("WHEEL_BASE");
    const trackWidth = pick("TRACK_WIDTH");
    const perimeter = radius * 2 * Math.PI;
    const wheelRatio = (360 * reduction) / perimeter; // deg 转子 / mm 轮面切向
    // 轮心到车体中心的距离(chassis.c 的 LF_CENTER;四轮同值)
    const centerDist = Math.hypot(wheelBase / 2, trackWidth / 2);
    return {
        radius,
        reduction,
        perimeter,
        wheelBase,
        trackWidth,
        centerDist,
        wheelRatio,
        mmPerDeg: 1 / (wheelRatio * HALF_SQRT2),
        // 每转子度对应的偏航角:θ_deg = Σ(-Δ_i) * yawDegPerRotorDeg
        yawDegPerRotorDeg: (180 / Math.PI) / (4 * wheelRatio * centerDist * HALF_SQRT2)
    };
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/** 读一个标量全局变量(不自己暂停:调用方负责在暂停窗口内或运行态下调用) */
async function readVar(name) {
    const r = await call(WORKSPACE, "variables.read", { variables: [{ name }] }, 20000);
    return r.values?.[name]?.value ?? null;
}

/**
 * 恢复目标并**确认它真的在跑**:连发 continue,直到 debug.status 报 running 且 chassis_last_ms 两次读数在增长。
 * 这里必须循环重试,因为本工程的 variables.read/memory.read 是侵入式读取,
 * 偶发的读取失败或残留暂停会让 continue 看起来生效、实际核心仍停着(实测踩过多次)。
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
            if (typeof a === "number" && typeof b === "number" && b > a) return b;
            lastErr = `${BOOT_MS_VAR} 两次读数不增长(${a} → ${b}),核心疑似仍停着`;
        } catch (e) {
            lastErr = e.message;
        }
    }
    return { failed: true, lastErr };
}

/** 读四轮 total_angle(deg)。会临时暂停目标,读完恢复;调用前车必须静止,否则快照带抖动 */
async function readWheels() {
    const status = await call(WORKSPACE, "debug.status", null, 20000);
    if (!status.session) throw new Error("没有活动的调试会话。先执行: node .agents/skills/mcu-debug-control/scripts/debug.js --workspace <工程> --start --attach");
    if (status.paused) {
        const r = await resumeAndVerify();
        if (r && r.failed) throw new Error(`目标处于暂停状态且无法恢复(${r.lastErr || "未知"}):核心停着时 total_angle 不累加,先让板子跑起来再重跑`);
    }
    await call(WORKSPACE, "debug.control", { action: "pause" }, 30000);
    let snap;
    try {
        const ptrRes = await call(WORKSPACE, "variables.read", { variables: MOTORS.map((n) => ({ name: n })) }, 30000);
        const wheels = {};
        for (const name of MOTORS) {
            const ptr = ptrRes.values?.[name]?.value;
            if (typeof ptr !== "number") throw new Error(`${name} 指针读取失败: ${JSON.stringify(ptrRes.values?.[name])}`);
            const r = await call(WORKSPACE, "memory.read", { address: ptr, length: 464 }, 30000);
            const buf = Array.isArray(r?.bytes) ? Buffer.from(r.bytes) : Buffer.from(String(r?.hex || ""), "hex");
            if (buf.length < 464) throw new Error(`${name} 内存读取不完整 ${buf.length}/464`);
            wheels[LABEL[name]] = buf.readFloatLE(TOTAL_ANGLE_OFFSET);
        }
        snap = { wheels, bootMs: await readVar(BOOT_MS_VAR) };
    } finally {
        // 恢复在下面的 resumeAndVerify 里统一处理(需要校验,不能只发一次 continue)
    }

    // 新鲜度校验:板子真的在跑,chassis_last_ms 才会增长;不增长说明这份快照是冻住的
    const afterMs = await resumeAndVerify();
    if (afterMs && afterMs.failed) {
        throw new Error(`恢复目标失败(${afterMs.lastErr || "chassis_last_ms 未增长"}):核心停着时 CAN 中断不跑,total_angle 不会累加,这次读数不可用`);
    }
    return { ...snap, bootMsAfter: afterMs };
}

function loadState() {
    if (!fs.existsSync(STATE_FILE)) return {};
    try {
        return JSON.parse(fs.readFileSync(STATE_FILE, "utf8"));
    } catch {
        return {};
    }
}

function saveState(state) {
    fs.mkdirSync(path.dirname(STATE_FILE), { recursive: true });
    fs.writeFileSync(STATE_FILE, JSON.stringify(state, null, 2));
}

(async () => {
    const [cmd, arg] = process.argv.slice(2);
    const expectIdx = process.argv.indexOf("--expect");
    const expect = expectIdx > 0 ? parseFloat(process.argv[expectIdx + 1]) : null;
    const axisIdx = process.argv.indexOf("--axis");
    const axisKey = axisIdx > 0 ? process.argv[axisIdx + 1] : "vx";
    if (!AXES[axisKey]) throw new Error(`--axis 只支持 ${Object.keys(AXES).join(" / ")},收到 ${axisKey}`);
    const axis = AXES[axisKey];
    const geo = readGeometry();

    if (cmd === "mark") {
        if (!arg) throw new Error("用法: node tools/odom.js mark <标签>,例如 mark B1-slow");
        const snap = await readWheels();
        const state = loadState();
        state[arg] = { ts: new Date().toISOString(), bootMs: snap.bootMs, totalAngle: snap.wheels };
        saveState(state);
        process.stdout.write(JSON.stringify({ ok: true, label: arg, bootMs: snap.bootMs, totalAngle: snap.wheels }, null, 2) + "\n");
        return;
    }

    if (cmd === "delta") {
        if (!arg) throw new Error("用法: node tools/odom.js delta <标签> [--axis vx|vy] [--expect 2000]");
        const state = loadState();
        const base = state[arg];
        if (!base) throw new Error(`没有标签 ${arg} 的起点标记,先跑 mark ${arg}`);
        const now = await readWheels();

        const d = {};
        for (const key of Object.keys(base.totalAngle)) d[key] = now.wheels[key] - base.totalAngle[key];

        // 与 EstimateSpeed() 完全同构:按轴取权重求和
        const sumSigned = Object.keys(d).reduce((acc, k) => acc + axis.sign[k] * d[k], 0);
        const isYaw = axis.kind === "yaw";
        const value = isYaw ? sumSigned * geo.yawDegPerRotorDeg : sumSigned * 0.25 * geo.mmPerDeg;
        // 四轮各自的等价值(位移 mm 或偏航角 deg),一致才说明没有拖轮;不一致时这四个数会明显分开
        const perWheel = Object.fromEntries(
            Object.keys(d).map((k) => [k, Number((isYaw ? -d[k] * geo.yawDegPerRotorDeg : axis.sign[k] * d[k] * geo.mmPerDeg).toFixed(1))])
        );
        const vals = Object.values(perWheel);
        const spread = (Math.max(...vals) - Math.min(...vals)) / Math.abs(value || 1);
        const rebooted = base.bootMs != null && now.bootMs != null && now.bootMs < base.bootMs - 500;

        const out = {
            ok: !rebooted,
            label: arg,
            axis: axisKey,
            axisName: axis.name,
            geometry: {
                radiusMm: geo.radius,
                reductionRatio: geo.reduction,
                wheelRatioDegPerMm: Number(geo.wheelRatio.toFixed(4)),
                mmPerDeg: Number(geo.mmPerDeg.toFixed(6)),
                centerDistMm: Number(geo.centerDist.toFixed(1)),
                yawDegPerRotorDeg: Number(geo.yawDegPerRotorDeg.toFixed(8))
            },
            dAngleDeg: Object.fromEntries(Object.entries(d).map(([k, v]) => [k, Number(v.toFixed(1))])),
            perWheelMm: isYaw ? undefined : perWheel,
            perWheelDeg: isYaw ? perWheel : undefined,
            distanceMm: isYaw ? undefined : Number(value.toFixed(1)),
            yawDeg: isYaw ? Number(value.toFixed(1)) : undefined,
            wheelSpreadPct: Number((spread * 100).toFixed(2)),
            spreadVerdict: spread > 0.05 ? "四轮不一致>5%,查拖轮/打滑/未着地" : "四轮一致,无拖轮迹象",
            uptimeS: { mark: base.bootMs, now: now.bootMs, rebooted },
            expectMm: expect,
            errorPct: expect ? Number((((value - expect) / expect) * 100).toFixed(2)) : null
        };
        if (rebooted) out.message = "板子在标记之后重启过,total_angle 已清零,这次读数作废,重新 mark";
        process.stdout.write(JSON.stringify(out, null, 2) + "\n");
        if (rebooted) process.exitCode = 1;
        return;
    }

    if (cmd === "show") {
        process.stdout.write(JSON.stringify(loadState(), null, 2) + "\n");
        return;
    }

    throw new Error("用法: node tools/odom.js mark <标签> | delta <标签> [--expect mm] | show");
})().catch((error) => {
    process.stdout.write(JSON.stringify({ ok: false, code: error.code || null, message: error.message }, null, 2) + "\n");
    process.exitCode = 1;
});
