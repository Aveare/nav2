"use strict";
/**
 * @file  tools/mem_read.js
 * @brief 通过 EmberProbe Agent Bridge 读取目标 RAM 原始字节(绕开 variables.read 的
 *        复合类型限制),用于读取"指针指向的堆实例"内部字段。
 *
 * 背景:本工程 ELF 经 LTO 后,变量的复合类型布局在 DWARF 中不可用,
 *       variables.read 只支持顶层标量符号 —— `motor_lf->measure.speed_aps`
 *       会被当成"指针变量自身 + 字段偏移",读出来是垃圾值(实测地址与
 *       motor_lf 变量地址完全相同即为证据)。因此改用本脚本按绝对地址读。
 *
 * 用法:
 *   node tools/mem_read.js 0x20011858 464            # 十六进制 + 长度
 *   node tools/mem_read.js 0x20011858 464 --words    # 附带按 u32/i16/f32 解出的视图
 *
 * 输出: 一行 JSON {ok, address, length, hex, bytes[], u32[], f32[]}
 * 备注: memory.read 需要调试会话;若目标正在自由运行,先由调用方决定是否挂起。
 */

const path = require("path");
const { call } = require(path.join(__dirname, "..", ".agents", "skills", "_emberprobe", "agent-client.js"));

function parseArgs(argv) {
    const out = { address: null, length: 64, words: false };
    for (const raw of argv) {
        if (raw === "--words") out.words = true;
        else if (out.address === null) out.address = raw;
        else out.length = Number.parseInt(raw, 10);
    }
    if (!out.address) throw new Error("用法: node tools/mem_read.js <地址> [长度] [--words]");
    if (!Number.isFinite(out.length) || out.length < 1 || out.length > 4096)
        throw new Error("长度必须是 1..4096 的整数");
    return out;
}

function toBytes(result) {
    // 兼容扩展返回的几种形态:{bytes:[..]} / {data:"base64"} / {hex:"..."}
    if (Array.isArray(result?.bytes)) return Uint8Array.from(result.bytes);
    if (typeof result?.hex === "string") {
        const clean = result.hex.replace(/[^0-9a-fA-F]/g, "");
        return Uint8Array.from(Buffer.from(clean, "hex"));
    }
    if (typeof result?.data === "string") return Uint8Array.from(Buffer.from(result.data, "base64"));
    return new Uint8Array(0);
}

(async () => {
    const opt = parseArgs(process.argv.slice(2));
    const workspace = path.resolve(__dirname, "..");
    const address = typeof opt.address === "string" && /^0x/i.test(opt.address) ? opt.address : Number(opt.address);
    const result = await call(workspace, "memory.read", { address, length: opt.length }, 30000);
    const bytes = toBytes(result);

    const out = {
        ok: bytes.length > 0,
        address: typeof address === "string" ? address.toLowerCase() : "0x" + address.toString(16),
        length: opt.length,
        got: bytes.length,
        hex: Buffer.from(bytes).toString("hex")
    };
    if (opt.words && bytes.length >= 4) {
        out.u32 = [];
        out.f32 = [];
        const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
        for (let i = 0; i + 4 <= bytes.length; i += 4) {
            out.u32.push(dv.getUint32(i, true));
            out.f32.push(Number(dv.getFloat32(i, true).toFixed(4)));
        }
    }
    if (bytes.length === 0) out.raw = result;
    process.stdout.write(JSON.stringify(out) + "\n");
})().catch((error) => {
    process.stdout.write(JSON.stringify({ ok: false, message: error.message, code: error.code || null }) + "\n");
    process.exitCode = 1;
});
