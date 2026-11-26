"use strict";
/**
 * @file  tools/sample_scalars.js
 * @brief 以固定间隔连续采样**顶层标量**变量,输出 CSV(时间戳 + 各变量值)。
 *
 * 用途:小陀螺自转标定时记录 `est_wz` 等标量的时间序列,事后积分得转过的总角度,
 *      与"人工数的圈数×360°"比对,从而验证力臂 360mm 换算是否正确。
 *
 * 限制(与 variables.read 相同):
 *   - 只支持**顶层标量符号**。ELF 经 LTO 后复合类型布局缺失,`motor_lf->...` 会静默读错,禁用。
 *   - `memory.read` 需暂停会话;本脚本走 `variables.read` 的临时探针,目标可保持运行。
 *   - 每次调用要建一次临时连接,实测约 200~500ms 一次,不要指望 kHz。
 *
 * 用法:
 *   node tools/sample_scalars.js --seconds 90 --interval 200 \
 *        --vars est_wz,est_vx,est_vy,vt_lf,vt_rf,vt_lb,vt_rb --out build/sample_wz.csv
 *
 * 输出:CSV 首列 t_ms(本机单调时钟,ms),其后每个变量一列;结束时打印一行汇总 JSON 到 stderr。
 */

const path = require("path");
const fs = require("fs");
const { call } = require(path.join(__dirname, "..", ".agents", "skills", "_emberprobe", "agent-client.js"));

const WORKSPACE = path.resolve(__dirname, "..");

function argOf(argv, name, dflt) {
    const i = argv.indexOf(name);
    return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : dflt;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
    const argv = process.argv.slice(2);
    const seconds = Number(argOf(argv, "--seconds", "30"));
    const interval = Number(argOf(argv, "--interval", "200"));
    const vars = argOf(argv, "--vars", "est_wz").split(",").map((s) => s.trim()).filter(Boolean);
    const outPath = path.resolve(WORKSPACE, argOf(argv, "--out", "build/sample_scalars.csv"));

    fs.mkdirSync(path.dirname(outPath), { recursive: true });
    // 逐点落盘:进程被中途结束(或想边采边看)时,已采到的点不丢。
    fs.writeFileSync(outPath, "t_ms," + vars.join(",") + "\r\n", "utf8");
    const t0 = Date.now();
    const endAt = t0 + seconds * 1000;
    let ok = 0;
    let fail = 0;
    let lastError = null;

    while (Date.now() < endAt) {
        const tickStart = Date.now();
        try {
            const res = await call(WORKSPACE, "variables.read", { variables: vars.map((n) => ({ name: n })) }, 20000);
            const cols = vars.map((n) => {
                const v = res.values?.[n];
                return v && typeof v.value === "number" ? v.value : "";
            });
            fs.appendFileSync(outPath, `${Date.now() - t0},${cols.join(",")}\r\n`, "utf8");
            ok++;
        } catch (error) {
            fail++;
            lastError = error.message;
        }
        const spent = Date.now() - tickStart;
        const wait = interval - spent;
        if (wait > 0) await sleep(wait);
    }

    process.stdout.write(`采样完成:${ok} 点 / 失败 ${fail} 次 -> ${outPath}\n`);
    if (fail > 0) process.stderr.write(`最后一次错误:${lastError}\n`);
})().catch((error) => {
    process.stderr.write(`采样脚本异常:${error.message}\n`);
    process.exitCode = 1;
});
