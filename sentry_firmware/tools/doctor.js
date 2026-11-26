"use strict";
/**
 * @file  tools/doctor.js
 * @brief 一条命令体检「读不到数」的整条链路:桥接 → 探针 → 会话 → 固件身份 → 供电 → 关键指针。
 *
 * 为什么要有它:
 *   2026-11-16 的 15 分钟,全花在手工一条条试上面 —— 先怀疑桥接(查描述文件、查 VS Code 进程、
 *   重新开工程),再怀疑探针供电(降速重试),再怀疑 RTT 控制块,最后才发现"板上固件不是这份 ELF"。
 *   这些检查彼此独立、都能自动做,却因为要一项项敲命令而串成了长串。
 *   现在一条 `node tools/doctor.js` 按依赖顺序全跑一遍,并直接给结论。
 *
 * 输出顺序刻意如此(自下而上,先排除外部依赖):
 *   1. 工程文件      ELF 是否存在、sha256(后续所有判断的基准)
 *   2. 桥接          描述文件端口/进程是否还活着,能不能 TCP 连上
 *   3. 进程          VS Code 窗口数、残留 openocd(残留会独占探针,报错却像"没电")
 *   4. 调试会话      attach 当前状态(没有就自己起一个,结束时还原)
 *   5. 固件身份      片上向量表 vs ELF 向量表  ← 最容易被"烧录成功"的假象骗过的一项
 *   6. 供电/时钟     RCC.CR(HSE/PLL 是否起来)、RCC.CSR(复位原因)
 *   7. 关键指针      motor_lf / gimbal_imu_data / yaw_motor / pitch_motor + chassis_last_ms 是否在增长
 *
 * 用法:
 *   node tools/doctor.js            # 人读
 *   node tools/doctor.js --json     # 机器读
 *   node tools/doctor.js --no-attach  # 不自己起调试会话(只做离线+桥接检查)
 *   node tools/doctor.js --keep     # 自己起的会话保留不关(方便紧接着读数)
 *
 * 退出码:0 = 全部通过;1 = 有 FAIL(会打印最该先修的那一条)。
 */

const fs = require("fs");
const path = require("path");
const net = require("net");
const crypto = require("crypto");
const { execSync } = require("child_process");

const WORKSPACE = path.resolve(__dirname, "..");
const ELF = path.join(WORKSPACE, "build", "basic_framework.elf");
const DESCRIPTOR_DIR = path.join(process.env.APPDATA || "", "Code", "User", "globalStorage", "bakesheep.emberprobe");
const WORKSPACE_POINTER = path.join(WORKSPACE, ".agents", "skills", "_emberprobe", "agent-bridge.json");

const { call } = require(path.join(WORKSPACE, ".agents", "skills", "_emberprobe", "agent-client.js"));
const preflight = require("./lib/preflight");

const argv = process.argv.slice(2);
const wantJson = argv.includes("--json");
const noAttach = argv.includes("--no-attach");
const keepSession = argv.includes("--keep");

const checks = [];
function record(section, name, status, detail) {
    checks.push({ section, name, status, detail: detail === undefined ? "" : String(detail) });
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/** TCP 探活:桥接端口能不能真的连上(描述文件在 ≠ 服务活着) */
function tcpAlive(port, timeoutMs = 800) {
    return new Promise((resolve) => {
        const sock = net.connect({ host: "127.0.0.1", port }, () => {
            sock.destroy();
            resolve(true);
        });
        sock.setTimeout(timeoutMs, () => {
            sock.destroy();
            resolve(false);
        });
        sock.on("error", () => resolve(false));
    });
}

/** 列出所有 emberprobe 的 bridge 描述文件,按 mtime 取最新 */
function latestDescriptor() {
    let files = [];
    try {
        files = fs.readdirSync(DESCRIPTOR_DIR)
            .filter((f) => /^agent-bridge-.*\.json$/.test(f))
            .map((f) => {
                const full = path.join(DESCRIPTOR_DIR, f);
                const st = fs.statSync(full);
                return { full, name: f, mtime: st.mtimeMs };
            })
            .sort((a, b) => b.mtime - a.mtime);
    } catch (_) {
        return null;
    }
    if (!files.length) return null;
    const newest = files[0];
    try {
        const j = JSON.parse(fs.readFileSync(newest.full, "utf8"));
        return { ...newest, port: j.port, pid: j.pid };
    } catch (_) {
        return { ...newest, parseError: true };
    }
}

function pidAlive(pid) {
    if (!pid) return false;
    try {
        process.kill(pid, 0);
        return true;
    } catch (_) {
        return false;
    }
}

/** 用 tasklist 查残留 openocd(它会独占探针,报错却长得像"板子没电") */
function staleOpenocd() {
    try {
        const out = execSync('tasklist /FI "IMAGENAME eq openocd.exe" /FO CSV /NH', {
            encoding: "utf8",
            stdio: ["ignore", "pipe", "ignore"],
            windowsHide: true
        });
        const lines = out.split(/\r?\n/).filter((l) => /openocd\.exe/i.test(l));
        return lines.map((l) => {
            const cols = l.split('","').map((s) => s.replace(/^"|"$/g, ""));
            return { pid: Number(cols[1]), mem: cols[4] };
        }).filter((p) => Number.isFinite(p.pid));
    } catch (_) {
        return [];
    }
}

function vscodeWindows() {
    try {
        const out = execSync(
            'powershell -NoProfile -Command "Get-Process -Name Code -ErrorAction SilentlyContinue | ' +
            'Where-Object { $_.MainWindowTitle } | ForEach-Object { \\"$($_.Id)|$($_.MainWindowTitle)\\" }"',
            { encoding: "utf8", stdio: ["ignore", "pipe", "ignore"], windowsHide: true, timeout: 20000 }
        );
        const rows = out.split(/\r?\n/).map((s) => s.trim()).filter(Boolean);
        return rows.map((r) => {
            const i = r.indexOf("|");
            return { pid: Number(r.slice(0, i)), title: r.slice(i + 1) };
        });
    } catch (_) {
        return [];
    }
}

(async () => {
    // ---------------- 1. 工程文件
    let elfInfo = null;
    try {
        const st = fs.statSync(ELF);
        const sha = crypto.createHash("sha256").update(fs.readFileSync(ELF)).digest("hex");
        elfInfo = { path: ELF, size: st.size, mtime: st.mtime, sha256: sha };
        record("1 工程文件", "ELF 存在", "OK", `${path.relative(WORKSPACE, ELF)}  ${(st.size / 1024).toFixed(0)} KB  sha256=${sha.slice(0, 16)}…  ${st.mtime.toLocaleString("zh-CN")}`);
    } catch (e) {
        record("1 工程文件", "ELF 存在", "FAIL", `找不到 ${ELF} —— 先编译`);
    }

    // ---------------- 2. 桥接
    const desc = latestDescriptor();
    let bridgeUp = false;
    if (!desc) {
        record("2 桥接", "描述文件", "FAIL", "没有任何 agent-bridge-*.json —— VS Code 没打开本工程,或 EmberProbe 扩展未激活");
    } else {
        bridgeUp = await tcpAlive(desc.port);
        const ageS = Math.round((Date.now() - desc.mtime) / 1000);
        const age = ageS < 120 ? `${ageS}s 前` : ageS < 7200 ? `${Math.round(ageS / 60)}min 前` : `${(ageS / 3600).toFixed(1)}h 前`;
        record("2 桥接", "描述文件", "OK", `${desc.name}  port=${desc.port} pid=${desc.pid}  更新于 ${age}`);
        record("2 桥接", "端口连通", bridgeUp ? "OK" : "FAIL",
            bridgeUp ? `127.0.0.1:${desc.port} 可连`
                : `127.0.0.1:${desc.port} 连不上(ECONNREFUSED);pid=${desc.pid} ${pidAlive(desc.pid) ? "仍存活" : "已不存在"}`);

        // 工作区指针是否指向这份描述文件
        try {
            const p = JSON.parse(fs.readFileSync(WORKSPACE_POINTER, "utf8"));
            const same = path.basename(p.descriptorPath || "") === desc.name;
            record("2 桥接", "工作区指针", "OK", `${path.relative(WORKSPACE, WORKSPACE_POINTER)} → ${path.basename(p.descriptorPath || "?")}${same ? "" : "(不是最新那份,可能过期)"}`);
        } catch (_) {
            record("2 桥接", "工作区指针", "WARN", `读不到 ${path.relative(WORKSPACE, WORKSPACE_POINTER)}`);
        }
    }

    // ---------------- 3. 进程
    const wins = vscodeWindows();
    // 桥接连通时窗口探测只是参考(VS Code 可从别的入口/远程窗口提供桥接),不当成问题
    record("3 进程", "VS Code 窗口", wins.length || bridgeUp ? "OK" : "WARN",
        wins.length ? wins.map((w) => `pid=${w.pid} “${w.title}”`).join("; ")
            : bridgeUp ? "窗口列表为空,但桥接端口已连通,不影响读数"
                : "没有带窗口的 VS Code(扩展不会注册桥接)");
    const stale = staleOpenocd();
    record("3 进程", "残留 openocd", stale.length ? "FAIL" : "OK",
        stale.length ? stale.map((p) => `pid=${p.pid}`).join(",") + " —— 独占探针;先 Stop-Process 再连"
            : "无残留");

    // ---------------- 4. 调试会话
    let sessionStarted = false;
    let status = null;
    if (!bridgeUp) {
        record("4 会话", "debug.status", "SKIP", "桥接不通,跳过");
    } else {
        try {
            status = await call(WORKSPACE, "debug.status", null, 20000);
            record("4 会话", "现有会话", "OK", status.session ? `id=${status.session.id} state=${status.state}` : "无");
        } catch (e) {
            record("4 会话", "debug.status", "FAIL", e.message);
        }

        if ((!status || !status.session) && !noAttach) {
            try {
                const r = await call(WORKSPACE, "debug.start", { attach: true }, 60000);
                sessionStarted = true;
                status = r.status;
                const cons = r.firmwareConsistency;
                record("4 会话", "attach", "OK",
                    `state=${status.state}` + (cons ? `  烧录记录 ${cons.status}(ELF ${String(cons.elfSha256).slice(0, 12)}… / flashedAt ${cons.flashedAt})` : ""));
                if (cons && cons.status === "match") {
                    record("4 会话", "烧录记录比对", "WARN",
                        "记录说 match,但那只比对工具自己记的哈希,不代表片上内容 —— 以下面第 5 项为准");
                }
            } catch (e) {
                record("4 会话", "attach", "FAIL", `${e.code || ""} ${e.message}`);
            }
        }
    }

    // ---------------- 5. 固件身份(核心)
    let fw = null;
    if (status && status.session) {
        try {
            fw = await preflight.checkFirmware(call, WORKSPACE, { force: true });
            if (fw.reason === "elf-unreadable") {
                record("5 固件身份", "向量表比对", "SKIP", fw.message);
            } else {
                record("5 固件身份", "向量表比对", fw.ok ? "OK" : "FAIL",
                    `ELF ${fw.elf.hex} (Reset=0x${fw.elf.reset.toString(16)})  vs  片上 ${fw.chip.hex} (Reset=0x${fw.chip.reset.toString(16)})` +
                    (fw.ok ? "" : "  ← 不是同一份镜像,变量读数全是垃圾"));
            }
        } catch (e) {
            record("5 固件身份", "向量表比对", "FAIL", `${e.code || ""} ${e.message}`);
        }
    } else {
        record("5 固件身份", "向量表比对", "SKIP", "没有会话");
    }

    // ---------------- 6. 供电 / 时钟 / 复位原因
    if (status && status.session && fw && fw.ok) {
        try {
            const r = await preflight.withTargetPaused(call, WORKSPACE, () =>
                call(WORKSPACE, "peripherals.read", { targets: ["RCC.CR", "RCC.CSR", "RCC.CFGR"] }, 20000));
            const regs = {};
            for (const reg of r.registers || []) regs[reg.path] = Number(reg.value);
            const cr = regs["RCC.CR"];
            const csr = regs["RCC.CSR"];
            if (Number.isFinite(cr)) {
                const hse = (cr & 0x00020000) !== 0;
                const pll = (cr & 0x02000000) !== 0;
                record("6 供电时钟", "HSE/PLL", hse && pll ? "OK" : "FAIL",
                    `RCC.CR=0x${cr.toString(16).padStart(8, "0")}  HSE ${hse ? "已起振" : "未起振!"}  PLL ${pll ? "已锁定" : "未锁定!"}`);
            }
            if (Number.isFinite(csr)) {
                const flags = [];
                if (csr & 0x04000000) flags.push("POR/PWR");
                if (csr & 0x08000000) flags.push("PIN");
                if (csr & 0x02000000) flags.push("BOR");
                if (csr & 0x20000000) flags.push("IWDG看门狗");
                if (csr & 0x10000000) flags.push("WWDG");
                if (csr & 0x40000000) flags.push("软件复位");
                record("6 供电时钟", "复位标志", flags.includes("IWDG看门狗") ? "WARN" : "OK",
                    `RCC.CSR=0x${csr.toString(16).padStart(8, "0")}  上次复位来源:${flags.join("+") || "无记录"}`);
            }
        } catch (e) {
            record("6 供电时钟", "RCC", "WARN", `读不到 RCC:${e.code || e.message}`);
        }
    } else {
        record("6 供电时钟", "RCC", "SKIP", "需要「会话 + 固件身份一致」");
    }

    // ---------------- 7. 关键指针 + 活性
    if (status && status.session && fw && fw.ok) {
        try {
            const g = await preflight.checkGlobals(call, WORKSPACE, {
                motor_lf: "motor_lf",
                gimbal_imu_data: "gimbal_imu_data",
                yaw_motor: "yaw_motor",
                pitch_motor: "pitch_motor"
            });
            const ptrs = {
                motor_lf: g.motor_lf?.text,
                gimbal_imu_data: g.gimbal_imu_data?.text,
                yaw_motor: g.yaw_motor?.text,
                pitch_motor: g.pitch_motor?.text
            };
            const bad = Object.entries(ptrs).filter(([, v]) => !v || v === "0x0" || v === "0x3");
            record("7 关键指针", "指针非空", bad.length ? "FAIL" : "OK",
                Object.entries(ptrs).map(([k, v]) => `${k}=${v ?? "?"}`).join("  ") +
                (bad.length ? "  ← 有指针是空/野值:任务没初始化完,或固件不匹配" : ""));

            // 活性:chassis_last_ms 两次读数必须增长。
            // 注意:本工程的 variables.read 是「侵入式」的(会短暂暂停核心,而且偶尔不恢复),
            // 所以每次读之前都要把核心放回 running;否则两次读到同一个冻结值,会把
            // "核心其实在跑"误判成"停着"(2026-11-16 那天就被这件事坑过)。
            async function readLastMs() {
                const s = await call(WORKSPACE, "debug.status", null, 20000);
                if (s.paused) await call(WORKSPACE, "debug.control", { action: "continue" }, 30000);
                const r = await call(WORKSPACE, "variables.read", { variables: [{ name: "chassis_last_ms" }] }, 20000);
                return r.values?.chassis_last_ms?.value;
            }
            const a = await readLastMs();
            await sleep(400);
            const b = await readLastMs();
            const grew = typeof a === "number" && typeof b === "number" && b > a;
            record("7 关键指针", "核心在跑", grew ? "OK" : "FAIL",
                `chassis_last_ms ${a} → ${b}${grew ? "" : "  ← 不增长:核心停着,或者读到的是错地址"}`);
        } catch (e) {
            record("7 关键指针", "体检", "FAIL", `${e.code || ""} ${e.message}`);
        }
    } else {
        record("7 关键指针", "体检", "SKIP", "需要「会话 + 固件身份一致」");
    }

    // ---------------- 收尾:关掉自己起的会话(目标保持运行)
    if (sessionStarted && !keepSession) {
        try {
            const st = await call(WORKSPACE, "debug.status", null, 20000);
            if (st.session) {
                if (st.paused) await call(WORKSPACE, "debug.control", { action: "continue" }, 30000);
                await call(WORKSPACE, "debug.control", { action: "stop" }, 30000);
                record("8 收尾", "释放探针", "OK", "已 stop 会话(目标保持运行)");
            }
        } catch (e) {
            record("8 收尾", "释放探针", "WARN", e.message);
        }
    } else if (sessionStarted && keepSession) {
        record("8 收尾", "释放探针", "SKIP", "--keep:会话保留,探针仍被占用");
    }

    // ---------------- 汇总
    const fails = checks.filter((c) => c.status === "FAIL");
    const warns = checks.filter((c) => c.status === "WARN");
    const verdict = fails.length ? "FAIL" : warns.length ? "WARN" : "OK";

    if (wantJson) {
        process.stdout.write(JSON.stringify({ ok: verdict !== "FAIL", verdict, worksheet: WORKSPACE, elf: elfInfo, checks }, null, 2) + "\n");
    } else {
        const lines = [];
        let section = "";
        for (const c of checks) {
            if (c.section !== section) {
                lines.push("");
                lines.push(`── ${c.section} ──`);
                section = c.section;
            }
            const tag = c.status === "OK" ? "✓" : c.status === "WARN" ? "!" : c.status === "SKIP" ? "·" : "✗";
            lines.push(` ${tag} ${c.name}:${c.detail}`);
        }
        lines.push("");
        lines.push(fails.length ? `结论:${fails.length} 项失败 —— 先修:「${fails[0].section} / ${fails[0].name}」${fails[0].detail}`
            : warns.length ? `结论:可用,但有 ${warns.length} 项提醒` : "结论:全链路正常,可以读数");

        // 把最容易踩的三个坑做成一句话提示
        if (fails.some((c) => c.section === "2 桥接")) {
            lines.push("  修法:在 VS Code 里 File → Open Folder 打开 C:\\RoboMaster\\RM-JIT-CR-Infantry,点开 EmberProbe 侧边栏");
        }
        if (fails.some((c) => c.name === "残留 openocd")) {
            lines.push("  修法:Stop-Process -Id <pid> -Force 之后再连");
        }
        if (fails.some((c) => c.section === "5 固件身份")) {
            lines.push("  修法:重新烧录 build/basic_framework.elf;在烧进去之前,任何读数都不作数");
        }
        process.stdout.write(lines.join("\n") + "\n");
    }

    process.exit(verdict === "FAIL" ? 1 : 0);
})().catch((e) => {
    if (wantJson) {
        process.stdout.write(JSON.stringify({ ok: false, verdict: "FAIL", error: e.message, checks }, null, 2) + "\n");
    } else {
        process.stdout.write(`体检脚本自身出错:${e.message}\n`);
    }
    process.exit(1);
});
