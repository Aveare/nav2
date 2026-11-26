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
 * 为什么**不再**用暂停式读取(pitch_calib 曾有"暂停 → debug.evaluate → 恢复"的老实现):
 *   暂停内核会让 GimbalTask 停发 CAN 控制帧,C620 约 100 ms 无帧即触发信号丢失保护按 0 输出,
 *   枪管被重力砸下、恢复后又抬起来 —— "每读一个点云台抽一下",读数与被测现象互相污染
 *   (2026-11-16 实测:用户看到"一读探针 pitch 就往下砸",据此得出的"极限环"结论全部作废)。
 *   现改为 variables.read 直接读 DWARF 成员路径,由 EmberProbe 后台 DAP 读取,不打断控制环。
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
const MAX_OUT_REF = 8000;   // speed_PID.MaxOut(gimbal.c:206),峰值电流码上限(2026-11-15 判据实验 4000→8000)
const INT_LIMIT_REF = 3000;  // speed_PID.IntegralLimit(gimbal.c:204),静止保持的实际上限
                             // 必须与**板上固件**一致:2026-11-16 判据实验把它改成了 3000(源码暂留 3000),
                             // 若将来回退 1200 要同步改这里,否则 jam/add 的"顶积分"判据会误报
const TEMP_WARN = 55;        // M3508/C620 堵转测温提醒阈值,℃

// 标定要读的量(DWARF 成员路径,由 variables.read 非侵入读取)
const EXPR = {
    ang: "gimbal_imu_data->Pitch",                                  // θ,IMU 欧拉 pitch(度)
    ref: "gimbal_cmd_recv.pitch",                                  // 角度环目标(度),ref-ang 即角度误差
    out: "pitch_motor->motor_controller.speed_PID.Output",          // 稳态保持电流码
    iout: "pitch_motor->motor_controller.speed_PID.Iout",           // 其中积分项(判断是否贴 IntegralLimit)
    spd: "pitch_motor->measure.speed_aps",                          // 转速(判稳态)
    gyro: "gimbal_imu_data->Gyro[0]",                                // 速度环实际反馈(IMU X 轴角速度),查符号用
    cur: "pitch_motor->measure.real_current",                       // 实际电流
    temp: "pitch_motor->measure.temperature"                        // 温度
};

// "解锁回中"诊断要看的全景:IMU 姿态 + 两个轴的目标 + 闭环输出 + 状态机。
// 命名与 robot_cmd.c / gimbal.c / chassis.c 里的全局一致;静态变量(robot_state、
// authority_source)在 DWARF 里带名字,用 variables.read 按裸名即可读(非侵入)。
const STATE_EXPR = {
    pitch: "gimbal_imu_data->Pitch",         // IMU 解算 pitch(角度环反馈域,度)
    roll: "gimbal_imu_data->Roll",           // 侧倾,判车身是否歪(歪了 pitch 也在变)
    yaw: "gimbal_imu_data->Yaw",             // 世界系 yaw
    gx: "gimbal_imu_data->Gyro[0]",          // 芯片原始轴角速度,pitch 速度环用的就是它
    gy: "gimbal_imu_data->Gyro[1]",
    gz: "gimbal_imu_data->Gyro[2]",
    tgtPitch: "gimbal_cmd_recv.pitch",      // pitch 目标角(解锁时被置 0)
    tgtYaw: "gimbal_cmd_recv.yaw",          // yaw 目标角(解锁时锚定为当时总角)
    offset: "chassis_cmd_recv.offset_angle",// 云台相对车头的角偏差,yaw 回正/跟随都用它
    state: "robot_state",                   // ROBOT_STOP=0 / ROBOT_READY=1
    auth: "authority_source",               // 0 RC_LOST / 1 KILL_SWITCH / 2 STOP_LATCHED / 3 RUN
    out: "pitch_motor->motor_controller.speed_PID.Output",
    iout: "pitch_motor->motor_controller.speed_PID.Iout",
    cur: "pitch_motor->measure.real_current",
    temp: "pitch_motor->measure.temperature"
};

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/** 读一个标量全局变量 */
async function readVar(name) {
    const r = await call(WORKSPACE, "variables.read", { variables: [{ name }] }, 20000);
    return r.values?.[name]?.value ?? null;
}

/**
 * 枚举字面量 → 数值。某些后端对 enum 类型的量回的是字面量(`ROBOT_STOP`)而不是数字,
 * 光看数字会全部落空(2026-11-16 实测:state/auth 两列显示 null)。
 */
const ENUM_NUM = {
    ROBOT_STOP: 0, ROBOT_READY: 1,
    AUTH_RC_LOST: 0, AUTH_KILL_SWITCH: 1, AUTH_STOP_LATCHED: 2, AUTH_RUN: 3,
    GIMBAL_ZERO_FORCE: 0, GIMBAL_GYRO_MODE: 1
};

function pickValue(r) {
    if (r == null) return null;
    if (typeof r === "number") return r;
    const cands = [r.value, r.result?.value, r.value?.value, r.result?.result, r.result];
    for (const c of cands) {
        if (typeof c === "number") return c;
        if (c && typeof c.value === "number") return c.value;
        if (typeof c === "string") {
            const t = c.trim();
            if (Object.prototype.hasOwnProperty.call(ENUM_NUM, t)) return ENUM_NUM[t];
            const n = Number(t);
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

/**
 * 一次 variables.read 批量读全部字段。
 *
 * **绝不暂停内核**(2026-11-16 血的教训):暂停会让 GimbalTask 停发 CAN 控制帧 →
 * C620 约 100 ms 无帧即按 0 输出(信号丢失保护)→ 枪管被重力砸下 → 恢复核心后力矩回来又抬起。
 * 结果是"每读一个点云台抽一下",读数与现象互相污染,还差点把这笔账算到重力前馈头上
 * (当时看到的"极限环""自己上下动"全是这个伪影)。
 * variables.read 走 EmberProbe 后台 DAP 读取,不打断控制环,所以这里既不需要
 * debug.status / pause / continue,也不再需要 resumeAndVerify 那套兜底。
 *
 * 固件身份改为看本次读取自带的 firmwareConsistency:不一致只告警不中断
 * (通常只是重编译改了注释/行号,地址没变;真不一致时读数是垃圾,需要重烧)。
 */
async function readBatch(exprs) {
    const keys = Object.keys(exprs);
    const names = keys.map((k) => exprs[k]);
    const r = await call(WORKSPACE, "variables.read", { variables: names.map((name) => ({ name })) }, 30000);
    const vals = {};
    const raw = {};
    keys.forEach((key, i) => {
        const entry = r.values?.[names[i]];
        raw[key] = entry ?? null;
        // diagnostic 命中(LIVE_NULL_POINTER 等)说明这一项这次没拿到,记 null 而不是猜一个值
        vals[key] = entry && !entry.diagnostic ? pickValue(entry) : null;
    });
    const fc = r.firmwareConsistency;
    return { vals, raw, firmwareMismatch: Boolean(fc && fc.status === "mismatch"), firmware: fc };
}

/** 标定用:批量读 + boot 时间戳(识别中途重启 —— 重启后标记的 base 全作废) */
async function evaluateAll(exprs) {
    const { vals, raw, firmwareMismatch, firmware } = await readBatch(exprs);
    try {
        vals[BOOT_MS_VAR] = await readVar(BOOT_MS_VAR);
    } catch {
        vals[BOOT_MS_VAR] = null;
    }
    return { vals, raw, firmwareMismatch, firmware };
}

// ---------------------------------------------------------------- 状态全景

/**
 * 宽松版批量读:任何一项没拿到只把它记成 null,不影响其它字段。
 * (诊断用,不希望因为某个符号名对不上就整条命令失败)
 *
 * `stale` 的语义变了(2026-11-16):以前指"核心没能恢复,读数可能是冻结值";
 * 现在读取不再暂停核心,所以它表示"ELF 与板上固件哈希不一致,地址可能对不上"。
 */
async function evaluateLenient(exprs) {
    const { vals, raw, firmwareMismatch, firmware } = await readBatch(exprs);
    const errors = {};
    for (const key of Object.keys(exprs)) {
        if (vals[key] == null) {
            const entry = raw[key];
            errors[key] = entry?.diagnostic?.message || "未取到值";
        }
    }
    try {
        vals[BOOT_MS_VAR] = await readVar(BOOT_MS_VAR);
    } catch {
        vals[BOOT_MS_VAR] = null;
    }
    const stale = firmwareMismatch
        ? `ELF(${String(firmware?.elfSha256).slice(0, 8)}) 与板上固件(${String(firmware?.flashedSha256).slice(0, 8)}) 不一致,地址可能对不上,建议重烧`
        : null;
    return { vals, errors, stale };
}

const AUTH_NAME = { 0: "RC_LOST(遥控丢失)", 1: "KILL_SWITCH(上挡零力)", 2: "STOP_LATCHED(急停锁存)", 3: "RUN(已解锁)" };

async function stateCommand(times, intervalMs) {
    const samples = [];
    for (let i = 0; i < times; i++) {
        if (i > 0) await sleep(intervalMs);
        const { vals, errors, stale } = await evaluateLenient(STATE_EXPR);
        samples.push({ at: new Date().toISOString(), ...vals, errors, stale });
    }
    return samples;
}


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
        const e = new Error("没能从 variables.read 取到 ang/out 数值(变量名或 ELF 绑定有问题),原始返回见 raw");
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
    // 非侵入读取后每个样本约 0.1~0.3s(不再有暂停/恢复开销),按时间窗折算样本数,最少 5 点
    const targetSamples = Math.max(5, Math.round((secs * 1000) / 150));
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

/** 单基函数模型:y ≈ k·f(x) + b(两参数最小二乘) */
function ls1(rows, xf) {
    let s11 = 0;
    let s1 = 0;
    let s1y = 0;
    let sy = 0;
    let n = 0;
    for (const r of rows) {
        const x = xf(r);
        s11 += x * x;
        s1 += x;
        s1y += x * r.out;
        sy += r.out;
        n += 1;
    }
    const det = s11 * n - s1 * s1;
    if (Math.abs(det) < 1e-9) return null;
    const k = (s1y * n - s1 * sy) / det;
    const b = (s11 * sy - s1 * s1y) / det;
    let sse = 0;
    for (const r of rows) sse += (r.out - (k * xf(r) + b)) ** 2;
    return { k1: k, b, sse };
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

function fit(useCur) {
    const raw = readRows();
    if (raw.length < 3) return { ok: false, message: `只有 ${raw.length} 个点,至少 3 个(建议 8 个以上且覆盖行程两端)` };

    // 拟合对象用哪个量:
    //   默认 `cur`(measure.real_current,电机实际输出的总电流码)= 保持该角度真正需要的力矩。
    //   一旦固件里开了前馈,`out`(速度环自己算出来的部分)就只剩"前馈没补上的残差",拿它拟合会把
    //   已经补上的那半量又算一遍 —— 所以标定必须用 cur。前馈关闭时 cur ≈ out,两者等价。
    const key = useCur === false ? "out" : "cur";

    // 同一角度箱可能有多趟数据(比如一趟往下推、一趟往上推)。**必须先合并再拟合**:
    // 摩擦力总是反抗运动方向,下推时向上、上推时向下,两趟的偏差近似等大反号,
    // 逐点放进最小二乘会把 ±摩擦当成重力,拟合出一条假的单调趋势(甚至把相位拉到 ±100°)。
    // 按 2° 分箱取平均即可把这部分抵掉 —— 前提是上下两趟都采到了。
    const byBin = new Map();
    for (const r of raw) {
        const key2 = Math.round(r.ang / SWEEP_BIN_DEG) * SWEEP_BIN_DEG;
        if (!byBin.has(key2)) byBin.set(key2, []);
        byBin.get(key2).push(r);
    }
    const rows = [...byBin.entries()]
        .sort((a, b) => a[0] - b[0])
        .map(([binAng, g]) => ({
            ang: g.reduce((s, x) => s + x.ang, 0) / g.length,
            out: g.reduce((s, x) => s + (Number.isFinite(x[key]) ? x[key] : x.out), 0) / g.length,
            n: g.length
        }));
    const paired = [...byBin.values()].filter((g) => g.length >= 2).length;

    const angs = rows.map((r) => r.ang);
    const span = Math.max(...angs) - Math.min(...angs);

    // ① 常数模型
    const c0 = lsConst(rows);
    // ② 余弦模型(A·cosθ + B)
    const c1 = ls1(rows, (r) => Math.cos(r.ang * DEG2RAD));
    // ③ 带相位余弦:φ 网格搜索(±180°,1° 步长),每个 φ 用 ② 的两参数拟合
    let best = null;
    for (let phi = -180; phi <= 180; phi += 1) {
        const r = ls1(rows, (x) => Math.cos((x.ang - phi) * DEG2RAD));
        if (r && (!best || r.sse < best.sse)) best = { ...r, phi };
    }

    const n = rows.length;
    const out = {
        ok: true,
        points: n,
        fittedOn: key,
        rawPoints: raw.length,
        angleBins: byBin.size,
        pairedBins: paired,
        angRange: [Math.min(...angs), Math.max(...angs)],
        angleSpanDeg: Number(span.toFixed(1)),
        models: {
            constant: { OFFSET: Math.round(c0.b), rms: Number(rms(c0.sse, n).toFixed(1)) },
            cos: c1 ? { A: Math.round(c1.k1), OFFSET: Math.round(c1.b), rms: Number(rms(c1.sse, n).toFixed(1)) } : null,
            cosPhase: best ? { A: Math.round(best.k1), PHASE: best.phi, OFFSET: Math.round(best.b), rms: Number(rms(best.sse, n).toFixed(1)) } : null
        }
    };

    // 推荐:谁的残差明显更小就用谁;残差接近时优先参数少的(窄行程下不易过拟合)
    const rc0 = rms(c0.sse, n);
    const rc1 = c1 ? rms(c1.sse, n) : Infinity;
    const rcp = best ? rms(best.sse, n) : Infinity;
    const warn = [];
    if (paired === 0 && raw.length >= 6) {
        warn.push(
            "所有角度箱都只有一趟数据 —— 摩擦力偏差没有抵消,拟合出的斜率里可能混着摩擦。\n" +
                "  建议再扫一趟**相反方向**(下推过一趟就再来一趟上推),同角度箱会自动平均,相位才可信。"
        );
    }
    if (best && Math.abs(best.phi) > 30) {
        warn.push(
            `最佳相位 φ=${best.phi}°。若质心明显偏离俯仰轴(自瞄模块装在轴前/轴上方),φ≠0 是正常的;\n` +
                `  但**上下两趟都采到**才排得掉摩擦偏差 —— 只有一趟时这个 φ 很可能是摩擦造成的假相位。`
        );
    }

    let recommend;
    if (rcp < rc0 * 0.6 && rcp <= rc1 * 1.02) {
        recommend = { note: "带相位余弦拟合明显更好,推荐三参数", ...out.models.cosPhase };
    } else if (c1 && rc1 < rc0 * 0.6) {
        recommend = { note: "余弦项显著,推荐 A·cosθ + B", ...out.models.cos };
    } else {
        recommend = {
            note: `行程内保持电流变化不明显(RMS ${rc0.toFixed(0)} vs 常数模型的 ${rc0.toFixed(0)}),推荐只填 OFFSET`,
            ...out.models.constant,
            A: 0
        };
    }
    out.recommend = recommend;
    out.residuals = { constant: Number(rc0.toFixed(1)), cos: Number(rc1.toFixed(1)), cosPhase: Number(rcp.toFixed(1)) };
    out.warnings = warn;

    out.rows = rows.map((r) => ({ ang: Number(r.ang.toFixed(2)), out: Math.round(r.out), samples: r.n }));
    return out;
}

// ---------------------------------------------------------------- 连续扫掠采点

/** 扫掠采点的"准静态"门限:角度变化率 ≤3°/s 且电机转速 ≤100apS 才算稳态点 */
const SWEEP_STATIC_RATE = 3.0;
const SWEEP_STATIC_SPD = 100;
const SWEEP_BIN_DEG = 2.0;
const SWEEP_CSV = path.join(WORKSPACE, "build", "pitch_sweep.csv");

function median(list) {
    const a = list.slice().sort((x, y) => x - y);
    if (!a.length) return null;
    const m = a.length >> 1;
    return a.length % 2 ? a[m] : (a[m - 1] + a[m]) / 2;
}

/**
 * 连续扫掠采点:一次 variables.sample 拿整条曲线(非侵入,不暂停核心),再把"准静态"样本
 * 按角度分箱取中位数,每箱落成一个标定点写进 pitch_calib.csv。
 *
 * 为什么需要它:人手推杆只能"从最上面慢慢到最下面",没法精确停在每个整数角度,而 add 要求
 * "停住→说一声→采一点",既慢又强人所难。扫掠一次 30 秒就能覆盖全行程。
 * 准静态筛选(|dθ/dt| 小、|speed_aps| 小)保证采到的仍是"保持力矩",不含加减速惯性项 ——
 * 这是把扫掠数据当前馈标定用的关键前提。
 */
async function sweepTest(secs, intervalMs, label) {
    const count = Math.max(2, Math.min(1000, Math.round((secs * 1000) / intervalMs)));
    const keys = Object.keys(EXPR);
    const names = keys.map((k) => EXPR[k]);

    // 预热:standalone sampling 通道在冷启动时,第一条 RPC 常报 "failed to read memory"
    // (OpenOCD 还在起)。先用一次轻量 variables.read 把通道叫醒,失败就重试,不消耗采样窗口。
    for (let attempt = 0; attempt < 5; attempt++) {
        try {
            await call(WORKSPACE, "variables.read", { variables: names.slice(0, 2).map((name) => ({ name })) }, 20000);
            await sleep(150);
            break;
        } catch {
            if (attempt === 4) break;
            await sleep(700);
        }
    }
    // 分块采样:单个 variables.sample 请求太长容易被打断(实测 40s×1000 点会整条失败,且失败时
    // 一点数据都拿不到)。改成每块 100 点(约 4s),失败重试 3 次;已采到的部分保留,
    // 只要拿到 ≥40% 就照常出数据并在结果里标 partial —— 用户推一趟不容易,不能因为一次抖动全丢。
    const CHUNK = 100;
    const samples = [];
    let hardError = null;
    let failedChunks = 0;
    for (let got = 0; got < count; got += CHUNK) {
        const n = Math.min(CHUNK, count - got);
        let batch = null;
        for (let attempt = 0; attempt < 3 && batch === null; attempt++) {
            try {
                const r = await call(
                    WORKSPACE,
                    "variables.sample",
                    { variables: names.map((name) => ({ name })), count: n, intervalMs },
                    Math.min(2147483647, n * intervalMs + 30000)
                );
                batch = Array.isArray(r.samples) ? r.samples : [];
            } catch (e) {
                hardError = e.message || String(e);
                if (attempt < 2) await sleep(800);
            }
        }
        if (batch === null) {
            failedChunks++;
            continue;
        }
        samples.push(...batch);
    }
    if (!samples.length) {
        return {
            ok: false,
            message: `采样失败:${hardError || "无样本"} —— 拿到 0/${count} 点,${failedChunks} 块失败。先 node tools/doctor.js 看探针/ELF 绑定`
        };
    }
    // 只要拿到任何数据就落盘:用户推一趟不容易,哪怕只剩 4 秒也可能正好覆盖到要判定的方向。
    const partial = samples.length < count * 0.95 || failedChunks > 0;

    const rows = samples.map((s) => {
        const row = { t: s.timestamp };
        keys.forEach((k, i) => {
            const v = s.values?.[names[i]];
            row[k] = v && typeof v.value === "number" ? v.value : null;
        });
        return row;
    });

    // 用中心差分估每点的角速度,再标"准静态"。两端邻居不足时退化成单侧差分。
    for (let i = 0; i < rows.length; i++) {
        const a = rows[Math.max(0, i - 1)];
        const b = rows[Math.min(rows.length - 1, i + 1)];
        const dt = (b.t - a.t) / 1000;
        const ok = dt > 0 && [rows[i].ang, a.ang, b.ang].every((v) => typeof v === "number");
        rows[i].rate = ok ? (b.ang - a.ang) / dt : null;
        rows[i].static = ok && Math.abs(rows[i].rate) <= SWEEP_STATIC_RATE && Math.abs(rows[i].spd || 0) <= SWEEP_STATIC_SPD;
    }

    fs.mkdirSync(path.dirname(SWEEP_CSV), { recursive: true });
    fs.writeFileSync(
        SWEEP_CSV,
        "t_ms,ang_deg,ref_deg,err_deg,out,iout,spd_aps,gyro,cur,temp,rate_deg_s,static\n" +
            rows
                .map((x) =>
                    [
                        x.t,
                        x.ang,
                        x.ref,
                        x.err,
                        x.out,
                        x.iout,
                        x.spd,
                        x.gyro,
                        x.cur,
                        x.temp,
                        x.rate == null ? "" : x.rate.toFixed(2),
                        x.static ? 1 : 0
                    ].join(",")
                )
                .join("\n") +
            "\n"
    );

    // 按角度分箱:同一箱里若有 ≥3 个准静态样本,用中位数代表该角度的保持电流
    const bins = new Map();
    for (const x of rows) {
        if (!x.static || typeof x.ang !== "number" || typeof x.out !== "number") continue;
        const key = Math.round(x.ang / SWEEP_BIN_DEG) * SWEEP_BIN_DEG;
        if (!bins.has(key)) bins.set(key, []);
        bins.get(key).push(x);
    }

    let bootMs = null;
    try {
        bootMs = await readVar(BOOT_MS_VAR);
    } catch {
        bootMs = null;
    }

    const at = new Date().toISOString();
    fs.mkdirSync(path.dirname(CSV), { recursive: true });
    if (!fs.existsSync(CSV)) fs.writeFileSync(CSV, "at,label,ang_deg,ref_deg,err_deg,out,iout,spd_aps,cur,temp,boot_ms\n");

    const table = [];
    let appended = 0;
    for (const binAng of [...bins.keys()].sort((a, b) => a - b)) {
        const group = bins.get(binAng);
        if (group.length < 3) continue;
        const ang = median(group.map((g) => g.ang));
        const ref = median(group.map((g) => g.ref));
        const out = median(group.map((g) => g.out));
        const iout = median(group.map((g) => g.iout));
        const cur = median(group.map((g) => g.cur));
        const temp = median(group.map((g) => g.temp));
        const spd = median(group.map((g) => g.spd));
        const err = ref != null && ang != null ? ref - ang : null;
        fs.appendFileSync(
            CSV,
            `${at},${label}@${binAng},${ang.toFixed(2)},${ref == null ? "" : ref.toFixed(2)},${err == null ? "" : err.toFixed(2)},` +
                `${out.toFixed(1)},${iout == null ? "" : iout.toFixed(1)},${spd == null ? "" : spd.toFixed(1)},` +
                `${cur == null ? "" : Math.round(cur)},${temp == null ? "" : Math.round(temp)},${bootMs == null ? "" : bootMs}\n`
        );
        appended++;
        table.push({ binAng, n: group.length, ang, out, cur, iout, temp });
    }

    const angs = rows.map((x) => x.ang).filter((v) => typeof v === "number");
    return {
        ok: true,
        partial,
        failedChunks,
        samples: rows.length,
        staticSamples: rows.filter((x) => x.static).length,
        seconds: rows.length ? (rows[rows.length - 1].t - rows[0].t) / 1000 : 0,
        angRange: angs.length ? [Math.min(...angs), Math.max(...angs)] : null,
        bins: table,
        appended,
        bootMs
    };
}

// ---------------------------------------------------------------- 离线重算(不用再推一趟)

/**
 * 从已保存的原始扫掠波形重新分箱入库。
 *
 * 为什么要它:扫掠期间如果一直推着杆,云台会"追着目标跑"、稳态误差 1~3°,此时的速度环电流里
 * 混着跟不上的滞后分量,不能当"保持重力所需的力矩"。真正的保持点只出现在你松杆停顿的时刻
 * (|误差| 很小)。这个命令让你用不同的误差门限反复重算同一份波形,不用再麻烦你重推。
 */
function rebuild(maxErr, minSamples, labelPrefix) {
    if (!fs.existsSync(SWEEP_CSV)) return { ok: false, message: `没有原始波形 ${path.relative(WORKSPACE, SWEEP_CSV)},先跑一次 sweep` };
    const lines = fs.readFileSync(SWEEP_CSV, "utf8").trim().split(/\r?\n/);
    const head = lines[0].split(",");
    const idx = (k) => head.indexOf(k);
    const rows = lines.slice(1).map((l) => {
        const c = l.split(",");
        return {
            t: Number(c[idx("t_ms")]),
            ang: Number(c[idx("ang_deg")]),
            ref: Number(c[idx("ref_deg")]),
            err: Number(c[idx("err_deg")]),
            out: Number(c[idx("out")]),
            iout: Number(c[idx("iout")]),
            spd: Number(c[idx("spd_aps")]),
            cur: Number(c[idx("cur")]),
            temp: Number(c[idx("temp")]),
            static: c[idx("static")] === "1"
        };
    });
    const picked = rows.filter((x) => x.static && Number.isFinite(x.err) && Math.abs(x.err) <= maxErr);
    if (!picked.length) return { ok: false, message: `误差门限 ${maxErr}° 下一个点都没有 —— 放宽到 2.0° 再试` };

    const bins = new Map();
    for (const x of picked) {
        const key = Math.round(x.ang / SWEEP_BIN_DEG) * SWEEP_BIN_DEG;
        if (!bins.has(key)) bins.set(key, []);
        bins.get(key).push(x);
    }

    const at = new Date().toISOString();
    fs.mkdirSync(path.dirname(CSV), { recursive: true });
    if (!fs.existsSync(CSV)) fs.writeFileSync(CSV, "at,label,ang_deg,ref_deg,err_deg,out,iout,spd_aps,cur,temp,boot_ms\n");

    const table = [];
    let appended = 0;
    for (const binAng of [...bins.keys()].sort((a, b) => a - b)) {
        const group = bins.get(binAng);
        if (group.length < minSamples) continue;
        const ang = median(group.map((g) => g.ang));
        const ref = median(group.map((g) => g.ref));
        const out = median(group.map((g) => g.out));
        const iout = median(group.map((g) => g.iout));
        const cur = median(group.map((g) => g.cur));
        const temp = median(group.map((g) => g.temp));
        const spd = median(group.map((g) => g.spd));
        fs.appendFileSync(
            CSV,
            `${at},${labelPrefix}@${binAng},${ang.toFixed(2)},${ref.toFixed(2)},${(ref - ang).toFixed(2)},` +
                `${out.toFixed(1)},${iout.toFixed(1)},${spd.toFixed(1)},${Math.round(cur)},${Math.round(temp)},\n`
        );
        appended++;
        table.push({ binAng, n: group.length, ang, err: ref - ang, out, cur, iout, temp });
    }
    return { ok: true, scanned: rows.length, picked: picked.length, maxErr, bins: table, appended };
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
            "  node tools/pitch_calib.js sweep [标签] --secs 30 --interval 40\n" +
            "                                          连续扫掠采点:你从最上面慢慢推到最下面,\n" +
            "                                          我一次采整条曲线,自动按角度分箱入库(推荐)\n" +
            "  node tools/pitch_calib.js show          看已采的点\n" +
            "  node tools/pitch_calib.js fit           拟合并打印建议宏值\n" +
            "  node tools/pitch_calib.js clear         清空采集数据\n" +
            "  node tools/pitch_calib.js state [--times 3] [--interval 800]\n" +
            "                                          IMU 姿态 + 两轴目标 + 闭环输出 + 状态机全景\n" +
            "                                          (诊断\"解锁回中\"是否真的在闭环、是否被重力拖走)\n"
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

    if (cmd === "state") {
        const argTimes = argv.indexOf("--times");
        const argInterval = argv.indexOf("--interval");
        const times = argTimes >= 0 ? Math.max(1, Math.min(20, parseInt(argv[argTimes + 1], 10) || 1)) : 1;
        const intervalMs = argInterval >= 0 ? Math.max(200, parseInt(argv[argInterval + 1], 10) || 800) : 800;
        const samples = await stateCommand(times, intervalMs);
        if (wantJson) return void process.stdout.write(JSON.stringify({ ok: true, samples }, null, 2) + "\n");

        const f = (v, w, d = 2) => (typeof v === "number" ? v.toFixed(d).padStart(w) : "?".padStart(w));
        const lines = [];
        lines.push("  时刻      pitch    roll     yaw   目标pitch  误差   Output   Iout    电流  温度   状态机");
        for (const s of samples) {
            const err = typeof s.pitch === "number" && typeof s.tgtPitch === "number" ? s.tgtPitch - s.pitch : null;
            const t = new Date(s.at).toLocaleTimeString("zh-CN", { hour12: false });
            lines.push(
                `  ${t} ${f(s.pitch, 8)} ${f(s.roll, 7)} ${f(s.yaw, 7)} ${f(s.tgtPitch, 9)} ${f(err, 6)} ` +
                `${f(s.out, 8, 0)} ${f(s.iout, 6, 0)} ${f(s.cur, 7, 0)} ${f(s.temp, 5, 0)}   ` +
                `${AUTH_NAME[s.auth] || s.auth}${s.state === 1 ? " / READY" : s.state === 0 ? " / STOP" : ""}`
            );
        }
        process.stdout.write(lines.join("\n") + "\n");
        const errKeys = Object.keys(samples[0].errors || {});
        if (errKeys.length) process.stdout.write(`  ⚠ 取不到的字段:${errKeys.join(", ")}\n`);
        const stale = samples.find((s) => s.stale);
        if (stale) process.stdout.write(`  ⚠ ${stale.stale}\n`);
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
        const useOut = argv.includes("--use") && argv[argv.indexOf("--use") + 1] === "out";
        const r = fit(!useOut);
        if (wantJson) return void process.stdout.write(JSON.stringify(r, null, 2) + "\n");
        if (!r.ok) return void process.stdout.write(`拟合失败:${r.message}\n`);

        const lines = [];
        lines.push(
            `样本 ${r.rawPoints} 行 → 按 2° 分箱得 ${r.points} 个角度点(其中 ${r.pairedBins} 个箱有 ≥2 趟数据)\n` +
                `  行程 ${r.angRange[0].toFixed(1)}° ~ ${r.angRange[1].toFixed(1)}°  (跨度 ${r.angleSpanDeg}°)\n` +
                `  拟合目标:${r.fittedOn === "cur" ? "measure.real_current(实际总电流,含前馈)" : "速度环 Output(前馈关闭时才等于总电流)"}`
        );
        lines.push("");
        lines.push("  模型                                      残差RMS(码)");
        lines.push(`  常数     I = ${String(r.models.constant.OFFSET).padStart(6)}                    ${r.models.constant.rms}`);
        if (r.models.cos) lines.push(`  余弦     I = ${String(r.models.cos.A).padStart(6)}·cosθ + ${String(r.models.cos.OFFSET).padStart(6)}         ${r.models.cos.rms}`);
        if (r.models.cosPhase)
            lines.push(
                `  带相位   I = ${String(r.models.cosPhase.A).padStart(6)}·cos(θ−${r.models.cosPhase.PHASE}°) + ${String(r.models.cosPhase.OFFSET).padStart(6)}  ${r.models.cosPhase.rms}`
            );
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
        if (r.warnings && r.warnings.length) {
            lines.push("");
            r.warnings.forEach((w) => lines.push(`⚠ ${w}`));
        }
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

    if (cmd === "sweep") {
        const args = argv.slice(1).filter((a) => a !== "--json");
        let secs = 30;
        let interval = 40;
        let label = "sweep";
        for (let i = 0; i < args.length; i++) {
            if (args[i] === "--secs") secs = Math.max(3, Math.min(120, Number(args[++i]) || 30));
            else if (args[i] === "--interval") interval = Math.max(20, Math.min(500, Number(args[++i]) || 40));
            else if (!args[i].startsWith("--")) label = args[i];
        }
        const r = await sweepTest(secs, interval, label);
        if (wantJson) return void process.stdout.write(JSON.stringify(r, null, 2) + "\n");
        if (!r.ok) return void process.stdout.write(`扫掠采样失败:${r.message}\n`);

        const L = [];
        L.push(`扫掠 "${label}":${r.samples} 样本 / ${r.seconds.toFixed(1)}s,其中准静态 ${r.staticSamples} 点`);
        if (r.partial) L.push(`⚠ 有 ${r.failedChunks} 块采样失败(共重试 3 次),数据不完整 —— 缺的角度可以再扫一趟补上`);
        if (r.angRange) L.push(`角度覆盖 ${r.angRange[0].toFixed(1)}° ~ ${r.angRange[1].toFixed(1)}°`);
        L.push(`原始波形 ${path.relative(WORKSPACE, SWEEP_CSV)}   标定点 ${r.appended} 个已并入 ${path.relative(WORKSPACE, CSV)}`);
        L.push("");
        if (r.bins.length) {
            L.push("  角度箱    样本   保持电流Output   电流码  积分Iout  温度");
            for (const b of r.bins) {
                L.push(
                    `  ${String(b.binAng).padStart(6)}°  ${String(b.n).padStart(5)}  ${String(Math.round(b.out)).padStart(12)}  ` +
                        `${String(Math.round(b.cur)).padStart(8)}  ${String(Math.round(b.iout)).padStart(8)}  ${b.temp == null ? "" : Math.round(b.temp)}`
                );
            }
            L.push("");
            L.push("下一步:node tools/pitch_calib.js fit");
        } else {
            L.push("没有凑够准静态样本 —— 推得太快或一直在加减速。放慢重来(全程 15~20s,中途可停一下)。");
        }
        process.stdout.write(L.join("\n") + "\n");
        return;
    }

    if (cmd === "rebuild") {
        const args = argv.slice(1).filter((a) => a !== "--json");
        let maxErr = 1.0;
        let minSamples = 3;
        let label = "R";
        for (let i = 0; i < args.length; i++) {
            if (args[i] === "--max-err") maxErr = Math.max(0.1, Number(args[++i]) || 1.0);
            else if (args[i] === "--min") minSamples = Math.max(1, Number(args[++i]) || 3);
            else if (!args[i].startsWith("--")) label = args[i];
        }
        const r = rebuild(maxErr, minSamples, label);
        if (wantJson) return void process.stdout.write(JSON.stringify(r, null, 2) + "\n");
        if (!r.ok) return void process.stdout.write(`重算失败:${r.message}\n`);
        process.stdout.write(
            `从原始波形重算:扫 ${r.scanned} 行,|误差|≤${r.maxErr}° 的保持点 ${r.picked} 个,入库 ${r.appended} 箱\n\n`
        );
        if (r.bins.length) {
            process.stdout.write("  角度      样本   误差   保持电流   积分   温度\n");
            for (const b of r.bins)
                process.stdout.write(
                    `  ${String(b.binAng).padStart(6)}°  ${String(b.n).padStart(5)}  ${b.err.toFixed(2).padStart(5)}  ` +
                        `${String(Math.round(b.out)).padStart(8)}  ${String(Math.round(b.iout)).padStart(5)}  ${Math.round(b.temp)}\n`
                );
            process.stdout.write("\n下一步:node tools/pitch_calib.js fit\n");
        }
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
