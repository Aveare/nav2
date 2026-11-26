"use strict";
/**
 * @file  tools/lib/preflight.js
 * @brief 读数前的身份预检:确认「板上跑的固件 == 工程里的 ELF」,并确认关键指针不是空。
 *
 * 为什么必须有这一层(2026-11-16 实测事故):
 *   21:15 那次烧录报成功、工具里 firmwareConsistency 也报 match,但片上 flash 其实是另一份
 *   镜像(或根本没写进去)。于是所有工具按 ELF 的符号表去读 RAM:
 *     motor_lf           -> 0x0        (真值是 0x20011858 这种堆/静态指针)
 *     gimbal_imu_data     -> 0x3
 *     chassis_last_ms    -> 恒定 1.12e-44(原始位是整数 8)
 *     gimbal_cmd_recv.pitch -> 1.09e-19
 *   这些数字长得像"故障数据",实际全是错地址上的垃圾,会把诊断引到完全错误的方向
 *   (当时去查了桥接、探针、供电、RTT,白花 15 分钟)。而判定只要读 8 个字节。
 *
 * 判据(为什么是这两个字):
 *   Cortex-M 复位从 0x08000000 取两个 32 位:第一个是初始 SP(SP_main),
 *   第二个是 Reset_Handler 入口。同一份 ELF 烧进 flash,这两字必然逐位相同;
 *   不同就说明板上不是这份固件 —— 与"烧录记录里写了成功"无关,以片上字节为准。
 *
 * 用法(读数工具里就一行):
 *   const preflight = require("./lib/preflight");
 *   await preflight.assertFirmware(call, WORKSPACE);   // 不一致直接抛错,拒绝出数
 *   const rep = await preflight.checkFirmware(call, WORKSPACE); // 或取结构化结果自己判
 */

const fs = require("fs");
const path = require("path");

const VECTOR_ADDR = 0x08000000; // F4 从主 flash 启动时的向量表地址
const VECTOR_BYTES = 8;         // SP + Reset_Handler
const DEFAULT_ELF = path.join("build", "basic_framework.elf");

/** 每个进程只做一次身份校验:交互式工具(jam 采 10 个点)不该每次都多暂停一次 */
const verified = new Set();

/**
 * 读 ELF 文件里 0x08000000 处的 8 个字节。
 * 不假设"向量表就在文件头",而是解析 program header 找覆盖该地址的 LOAD 段,
 * 这样链接脚本改了放置位置也不会误判。
 * @returns {{ok:boolean, hex?:string, sp?:number, reset?:number, message?:string}}
 */
function readElfVector(elfPath) {
    let buf;
    try {
        buf = fs.readFileSync(elfPath);
    } catch (e) {
        return { ok: false, message: `读不到 ELF:${elfPath}(${e.code || e.message})` };
    }

    const isElf = buf.length > 0x34 && buf.readUInt32BE(0) === 0x7f454c46;
    if (!isElf) return { ok: false, message: "不是 ELF 文件(魔数不对)" };
    if (buf[4] !== 1) return { ok: false, message: "不是 ELF32(本工程是 ARM 32 位)" };

    const phoff = buf.readUInt32LE(0x1c);
    const phentsize = buf.readUInt16LE(0x2a);
    const phnum = buf.readUInt16LE(0x2c);

    for (let i = 0; i < phnum; i++) {
        const off = phoff + i * phentsize;
        if (off + 0x14 > buf.length) break;
        const paddr = buf.readUInt32LE(off + 0x0c);
        const filesz = buf.readUInt32LE(off + 0x10);
        const poffset = buf.readUInt32LE(off + 0x04);
        if (paddr > VECTOR_ADDR || paddr + filesz < VECTOR_ADDR + VECTOR_BYTES) continue;

        const fileOff = poffset + (VECTOR_ADDR - paddr);
        if (fileOff + VECTOR_BYTES > buf.length) continue;
        const slice = buf.subarray(fileOff, fileOff + VECTOR_BYTES);
        return {
            ok: true,
            hex: slice.toString("hex"),
            sp: slice.readUInt32LE(0),
            reset: slice.readUInt32LE(4)
        };
    }
    return { ok: false, message: `ELF 里没有覆盖 0x${VECTOR_ADDR.toString(16)} 的 LOAD 段` };
}

/**
 * 暂停目标 → 在暂停窗口内执行 fn → 恢复(只有本次真的暂停过才恢复,
 * 避免把"别人暂停的现场"擅自跑起来)。
 */
async function withTargetPaused(call, workspace, fn) {
    const st = await call(workspace, "debug.status", null, 20000);
    if (!st.session) {
        const e = new Error(
            "没有活动的调试会话。先执行: node .agents/skills/mcu-debug-control/scripts/debug.js " +
            "--workspace <工程> --start --attach"
        );
        e.code = "NO_SESSION";
        throw e;
    }
    const wasPaused = !!st.paused;
    if (!wasPaused) await call(workspace, "debug.control", { action: "pause" }, 30000);
    try {
        return await fn();
    } finally {
        if (!wasPaused) {
            try {
                await call(workspace, "debug.control", { action: "continue" }, 30000);
            } catch (_) {
                /* 恢复失败让上层的 resumeAndVerify 去发现并报错,这里不吞成静默成功 */
            }
        }
    }
}

/** 读片上 flash 的向量表两个字(需要调试会话;内部自带暂停/恢复) */
async function readChipVector(call, workspace) {
    const hex = await withTargetPaused(call, workspace, async () => {
        const r = await call(workspace, "memory.read", { address: VECTOR_ADDR, length: VECTOR_BYTES }, 20000);
        if (r.unreadableBytes) throw new Error(`片上 flash 有 ${r.unreadableBytes} 字节读不出来`);
        return r.hex;
    });
    const buf = Buffer.from(hex, "hex");
    return { hex, sp: buf.readUInt32LE(0), reset: buf.readUInt32LE(4) };
}

/**
 * 比对片上固件与 ELF。
 * @param {object} opts { elf, force:true 跳过进程内缓存 }
 * @returns {{ok:boolean, reason?:string, chip?:object, elf?:object, elfPath?:string, message?:string}}
 */
async function checkFirmware(call, workspace, opts = {}) {
    const elfPath = opts.elf ? path.resolve(workspace, opts.elf) : path.join(workspace, DEFAULT_ELF);
    const cacheKey = `${workspace}|${elfPath}`;

    const elf = readElfVector(elfPath);
    if (!elf.ok) return { ok: false, reason: "elf-unreadable", elfPath, message: elf.message };

    // ELF 变了(重新编译过)就重新校验:用 mtime+size 做指纹
    let stamp = "";
    try {
        const st = fs.statSync(elfPath);
        stamp = `${st.mtimeMs}:${st.size}`;
    } catch (_) { /* ignore */ }

    const key = `${cacheKey}|${stamp}`;
    if (!opts.force && verified.has(key)) return { ok: true, cached: true, elfPath, elf };

    const chip = await readChipVector(call, workspace);
    const same = chip.hex === elf.hex;

    const result = {
        ok: same,
        elfPath,
        elf,
        chip,
        reason: same ? undefined : "firmware-mismatch",
        message: same
            ? `片上固件与 ELF 一致(向量表 ${chip.hex})`
            : "板上跑的固件不是这份 ELF:向量表不一致,下面读到的所有变量都是错地址上的垃圾值"
    };
    if (same) verified.add(key);
    return result;
}

/**
 * 校验不通过就抛错 —— 读数工具在暂停读变量**之前**调用它,
 * 保证永远不会把垃圾值当数据输出。
 */
async function assertFirmware(call, workspace, opts = {}) {
    let r;
    try {
        r = await checkFirmware(call, workspace, opts);
    } catch (e) {
        throw e; // NO_SESSION 等原样抛出,提示语已经够明确
    }
    if (r.ok) return r;

    const e = new Error(
        `${r.message}\n` +
        `  ELF  (${path.relative(workspace, r.elfPath)}) : 向量表 ${r.elf?.hex}  SP=0x${(r.elf?.sp ?? 0).toString(16)}  Reset=0x${(r.elf?.reset ?? 0).toString(16)}\n` +
        `  片上 flash @0x08000000              : 向量表 ${r.chip?.hex}  SP=0x${(r.chip?.sp ?? 0).toString(16)}  Reset=0x${(r.chip?.reset ?? 0).toString(16)}\n` +
        "  => 请先烧录工程里的这份 ELF,再读取。此时任何变量读数都不可信,不要拿去调参。"
    );
    e.code = "FIRMWARE_MISMATCH";
    e.detail = r;
    throw e;
}

/**
 * 关键指针体检:固件身份正确之后,再看"任务有没有真的初始化完"。
 * (指针为 0/野值 = 初始化没跑完,这时即使固件匹配也不能采数)
 */
async function checkGlobals(call, workspace, exprs = null) {
    const list = exprs || {
        motor_lf: "motor_lf",
        gimbal_imu_data: "gimbal_imu_data",
        chassis_last_ms: "chassis_last_ms"
    };
    const out = {};
    await withTargetPaused(call, workspace, async () => {
        for (const [key, expression] of Object.entries(list)) {
            try {
                const r = await call(workspace, "debug.evaluate", { expression }, 20000);
                out[key] = { text: String(r.result), type: r.type };
            } catch (e) {
                out[key] = { error: e.message };
            }
        }
    });
    return out;
}

module.exports = { readElfVector, readChipVector, checkFirmware, assertFirmware, checkGlobals, withTargetPaused, VECTOR_ADDR };
