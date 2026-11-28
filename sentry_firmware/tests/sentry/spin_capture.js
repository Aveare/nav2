/**
 * @file tests/sentry/spin_capture.js
 * @brief 哨兵小陀螺联合采集:一次采集同时拿两路数据(非侵入,不暂停核心)
 *
 *   ① 大yaw/小yaw 稳定性:variables.sample 批量采样(桥接非暂停,50Hz)
 *      world_rad/world_speed/torque/speed_target/small_yaw_joint/small_yaw_ref/
 *      fric_ff/carrier_inj/est_wz/mode/rearm/active
 *   ② 功率标定:同一窗口的 RTT 文本流([pwri]/[pwr]/[chcal])落盘,
 *      事后 tools/rtt_pwr_to_csv.py --power <W> --append steady_raw.csv 直接可用
 *
 * 为什么一个进程串行做两件事:
 *   variables.sample 与 rtt.read 都走 EmberProbe 桥,同进程串行调用天然互斥,
 *   不违反"采样期间不要同时开别的读变量工具"(tools/gimbal_trace.js 头注)。
 *   采样块 50 点×20ms=1s,每块后读一次 RTT,日志缓冲不溢出。
 *
 * 用法(仓库根目录):
 *   node tests/sentry/spin_capture.js --label spin300 --secs 20
 *   node tests/sentry/spin_capture.js --label spin500 --secs 20 --interval 20
 * 采集前先烧 sentry 新固件 + 起探针;ELF 身份不对会在预检拒绝。
 *
 * 输出:
 *   log/<label>_<secs>s.log       RTT 文本(功率标定行在内)
 *   log/<label>_<secs>s_yaw.csv   t_s, world_deg, world_spd_dps, torque_nm,
 *                                 spd_tgt_dps, small_joint_deg, small_ref_dps,
 *                                 fric_ff, carrier_inj, est_wz_dps, mode, rearm, active
 *   终端摘要:进入拖拽 / 稳态世界角速度 / 力矩均值(对照 DEV-20261128-041921 判据)
 *
 * @note 本文件属哨兵兵种路径;不依赖共享 tools/ 的修改。
 */
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const ROOT = path.resolve(__dirname, '..', '..');
let BRIDGE_WS = ROOT; // 桥接工作区(--workspace 覆盖),main() 里赋值
const { call } = require(path.join(ROOT, '.agents', 'skills', '_emberprobe', 'agent-client.js'));

const VARS = [
  'sentry_big_yaw_world_rad',
  'sentry_big_yaw_world_speed_rad_s',
  'sentry_big_yaw_torque_nm',
  'sentry_big_yaw_speed_target_rad_s',
  'sentry_small_yaw_joint_deg',
  'sentry_small_yaw_ref_deg_s',
  'sentry_rt_big_yaw_fric_ff_nm',
  'sentry_rt_small_yaw_carrier_inj',
  'est_wz',
  'chassis_cmd_recv.chassis_mode',
  'sentry_big_yaw_rearm_required',
  'sentry_big_yaw_control_active',
];
const CSV_HEADER = 't_s,world_deg,world_spd_dps,torque_nm,spd_tgt_dps,small_joint_deg,' +
  'small_ref_dps,fric_ff,carrier_inj,est_wz_dps,mode,rearm,active';
const RAD_2_DEG = 57.29577951308232;

function sha256File(file) {
  return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex').toUpperCase();
}

function readCurrentTxt(WS) {
  const file = path.join(WS, 'firmware', 'current.txt');
  if (!fs.existsSync(file)) return {};
  const out = {};
  for (const line of fs.readFileSync(file, 'utf8').split('\n')) {
    const m = /^(\w+)=(.*)$/.exec(line.trim());
    if (m) out[m[1]] = m[2].trim();
  }
  return out;
}

function preflight(WS) {
  const elf = path.join(WS, 'firmware', 'sentry.elf');
  if (!fs.existsSync(elf)) {
    console.error('[FAIL] <工作区>/firmware/sentry.elf 不存在:先 build.ps1 sentry -Current');
    return false;
  }
  const cur = readCurrentTxt(WS);
  if (cur.PROFILE !== 'sentry') {
    console.error(`[FAIL] current.txt PROFILE=${cur.PROFILE}(被别的兵种覆盖),重跑 build.ps1 sentry -Current`);
    return false;
  }
  if (String(cur.SHA256 || '').toUpperCase() !== sha256File(elf)) {
    console.error('[FAIL] current.txt SHA 与磁盘 sentry.elf 不一致(ELF 被重建),重烧或重新构建');
    return false;
  }
  return true;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/** 确保摩擦前馈在位:RAM 变量断电会复位到 0(config 默认),复位即补写。 */
async function ensureFeedforward(target) {
  const r = await call(BRIDGE_WS, 'variables.read',
    { variables: [{ name: 'sentry_rt_big_yaw_fric_ff_nm' }] }, 20000);
  const cur = (((r.values || {})['sentry_rt_big_yaw_fric_ff_nm'] || {}).value) || 0;
  if (Math.abs(cur - target) < 0.05) {
    console.log(`[前馈] fric_ff=${cur}(在位)`);
    return true;
  }
  const w = await call(BRIDGE_WS, 'variables.write',
    { values: [{ name: 'sentry_rt_big_yaw_fric_ff_nm', value: target }] }, 20000);
  const ok = !!((w.results || [])[0] || {}).verified;
  console.log(`[前馈] fric_ff=${cur}(复位/不一致)→ 已写 ${target}${ok ? ',verified' : ',回读失败!'}`);
  return ok;
}

async function sampleChunk(intervalMs, count) {
  for (let attempt = 0; attempt < 8; ++attempt) {
    try {
      const reply = await call(BRIDGE_WS, 'variables.sample', {
        variables: VARS.map((name) => ({ name })),
        count,
        intervalMs,
      }, 60000);
      const list = (reply && reply.samples) || [];
      if (list.length > 0) return list;
    } catch (err) {
      process.stderr.write(`[采样重试 ${attempt + 1}] ${err && err.message}\n`);
    }
    await sleep(500);
  }
  return [];
}

async function rttRead(gen, cur) {
  const params = { channel: 0, maxBytes: 16384 };
  if (gen !== null) { params.generation = gen; params.cursor = cur; }
  const reply = await call(BRIDGE_WS, 'rtt.read', params, 20000);
  return reply || {};
}

function num(v) { return typeof v === 'number' && isFinite(v) ? v : 0; }

/** 采样条目形状:{ timestamp, values: { <varname>: { value } } };扁平访问一律 undefined→0。 */
function sampleValue(entry, name) {
  const slot = entry && entry.values && entry.values[name];
  return slot ? num(slot.value) : 0;
}

/** CSV → 稳定性摘要;rows<11 返回 null(样本不足)。 */
function summarize(csvPath) {
  const lines = fs.readFileSync(csvPath, 'utf8').trim().split('\n');
  const rows = lines.slice(1).map((l) => l.split(','));
  if (rows.length <= 10) return null;
  const ang0 = parseFloat(rows[0][1]);
  const entry = rows.filter((r) => parseFloat(r[0]) <= 3);
  const entryDrag = Math.max(...entry.map((r) => Math.abs(parseFloat(r[1]) - ang0)));
  const tail = rows.slice(Math.floor(rows.length / 2));
  const spdAvg = tail.reduce((a, r) => a + Math.abs(parseFloat(r[2])), 0) / tail.length;
  const spdMax = Math.max(...tail.map((r) => Math.abs(parseFloat(r[2]))));
  const tAvg = tail.reduce((a, r) => a + Math.abs(parseFloat(r[3])), 0) / tail.length;
  const limp = tail.filter((r) => r[12] === '0').length;
  return { rows: rows.length, entryDrag, spdAvg, spdMax, tAvg, limpPct: 100 * limp / tail.length };
}

/** 离线自检:合成一段已知特征的 CSV,验证采集行格式与摘要算路。 */
function selftest() {
  const csvPath = path.join(ROOT, 'build', 'spin_selftest.csv');
  fs.mkdirSync(path.dirname(csvPath), { recursive: true });
  const rows = ['t_s,world_deg,world_spd_dps,torque_nm,spd_tgt_dps,small_joint_deg,' +
    'small_ref_dps,fric_ff,carrier_inj,est_wz_dps,mode,rearm,active'];
  // 0~3s 拖拽:世界角从 0 漂到 12°,速度 -120→0;3~10s 稳态:速度 ±0.5,力矩 3.3
  for (let i = 0; i < 500; ++i) {
    const t = i * 0.02;
    const inEntry = t <= 3;
    const ang = inEntry ? 12 * (t / 3) : 12;
    const spd = inEntry ? -120 * (1 - t / 3) : 0.5 * (i % 2 ? 1 : -1);
    const torque = inEntry ? 1.0 + t : 3.3;
    rows.push([t.toFixed(3), ang.toFixed(3), spd.toFixed(3), torque.toFixed(4),
      '0.000', '2.500', '0.000', '0.000', '0.000', '299.50', '4', '0', '1'].join(','));
  }
  // 混入卸力拍:摘要只统计后 250 行, Inject 后半每 10 行 1 拍 → limpPct=10%
  for (let i = 250; i < 500; i += 10) {
    const f = rows[i].split(',');
    f[12] = '0';
    rows[i] = f.join(',');
  }
  fs.writeFileSync(csvPath, rows.join('\n') + '\n');
  const s = summarize(csvPath);
  const ok = s !== null &&
    Math.abs(s.entryDrag - 12) < 0.5 &&
    s.spdAvg <= 1.0 && s.spdMax <= 1.0 &&
    Math.abs(s.tAvg - 3.3) < 0.15 &&
    Math.abs(s.limpPct - 10) < 2 &&
    s.rows === 500;
  console.log(`SELFTEST ${ok ? 'PASS' : 'FAIL'}: ` + JSON.stringify(s));
  fs.unlinkSync(csvPath);
  return ok;
}

async function main() {
  const argv = process.argv;
  if (argv.includes('--selftest')) process.exit(selftest() ? 0 : 1);
  const args = {};
  for (let i = 2; i < argv.length; ++i) {
    if (argv[i] === '--label') args.label = argv[++i];
    else if (argv[i] === '--secs') args.secs = parseInt(argv[++i], 10);
    else if (argv[i] === '--interval') args.interval = parseInt(argv[++i], 10);
    else if (argv[i] === '--workspace') args.workspace = argv[++i];
    else if (argv[i] === '--ff') args.ff = parseFloat(argv[++i]);
  }
  const label = args.label || 'spin';
  const secs = args.secs || 20;
  const intervalMs = args.interval || 20;
  const CHUNK = 25; // 25 点 × 20ms = 0.5s/块:采样期 RTT 停读,1024B 缓冲会丢行,
                    // 块越短丢越少(1s 块实测 20s 只抓到 4 对功率行)
  // 桥接按 VS Code 打开的工作区绑定;--workspace 指到 EmberProbe 实际启用的工作区
  const WS = args.workspace ? path.resolve(args.workspace) : ROOT;
  BRIDGE_WS = WS;
  const ffTarget = typeof args.ff === 'number' ? args.ff : 2.5;

  if (!preflight(WS)) process.exit(1);
  if (!(await ensureFeedforward(ffTarget))) {
    console.error('[FAIL] 前馈写不进去,检查桥接/探针');
    process.exit(1);
  }
  // 预探:去掉板上固件没有的变量(如旧固件无 sentry_rt_rotate_speed_dps)
  try {
    const probe = await call(WS, 'variables.read', { variables: VARS.map((n) => ({ name: n })) }, 20000);
    const errs = (probe && probe.variableErrors) || [];
    for (const e of errs) {
      const bad = (e && (e.name || e.path)) || '';
      if (bad && VARS.includes(bad)) {
        VARS.splice(VARS.indexOf(bad), 1);
        console.log(`[预探] 板上固件无 ${bad},剔除(固件较旧,建议之后重烧最新 ELF)`);
      }
    }
  } catch (err) {
    const hint = /采样连接|OpenOCD/.test(err.message)
      ? '(目标无响应:车断电/未上电/探针脱落?上电重启后再采)' : '';
    console.error(`[预探失败] ${err.message} ${hint}`);
    process.exit(1);
  }
  fs.mkdirSync(path.join(WS, 'log'), { recursive: true });
  const logPath = path.join(WS, 'log', `${label}_${secs}s.log`);
  const csvPath = path.join(WS, 'log', `${label}_${secs}s_yaw.csv`);
  const logFd = fs.openSync(logPath, 'w');
  const csvFd = fs.openSync(csvPath, 'w');
  fs.writeSync(csvFd, CSV_HEADER + '\n');

  console.log(`联合采集 <${label}> ${secs}s(每块 ${CHUNK} 点 @${intervalMs}ms,块间读 RTT)`);
  console.log('>>> 现在切入小陀螺并保持工况! <<<');

  let gen = null, cur = null, t0 = null, rows = 0, rttBytes = 0;
  const deadline = Date.now() + secs * 1000;
  while (Date.now() < deadline) {
    const samples = await sampleChunk(intervalMs, CHUNK);
    // 块后紧跟一次 RTT 读:variables.sample 会把 RTT 会话让渡出去(pending-host),
    // 每次读前重新 start 认领(实测 start→read 即恢复)。
    try {
      await call(BRIDGE_WS, 'rtt.start', {}, 15000);
      const j = await rttRead(gen, cur);
      if (typeof j.generation === 'number') { gen = j.generation; cur = j.nextCursor; }
      const text = j.text || '';
      rttBytes += Buffer.byteLength(text);
      if (text) fs.writeSync(logFd, text);
    } catch (err) {
      process.stderr.write(`[RTT 读失败] ${err && err.message}\n`);
    }
    for (const s of samples) {
      if (t0 === null) t0 = s.timestamp;
      const t = (s.timestamp - t0) / 1000;
      const line = [
        t.toFixed(3),
        (sampleValue(s, 'sentry_big_yaw_world_rad') * RAD_2_DEG).toFixed(3),
        (sampleValue(s, 'sentry_big_yaw_world_speed_rad_s') * RAD_2_DEG).toFixed(3),
        sampleValue(s, 'sentry_big_yaw_torque_nm').toFixed(4),
        (sampleValue(s, 'sentry_big_yaw_speed_target_rad_s') * RAD_2_DEG).toFixed(3),
        sampleValue(s, 'sentry_small_yaw_joint_deg').toFixed(3),
        sampleValue(s, 'sentry_small_yaw_ref_deg_s').toFixed(3),
        sampleValue(s, 'sentry_rt_big_yaw_fric_ff_nm').toFixed(3),
        sampleValue(s, 'sentry_rt_small_yaw_carrier_inj').toFixed(3),
        sampleValue(s, 'est_wz').toFixed(2),
        sampleValue(s, 'chassis_cmd_recv.chassis_mode').toFixed(0),
        sampleValue(s, 'sentry_big_yaw_rearm_required').toFixed(0),
        sampleValue(s, 'sentry_big_yaw_control_active').toFixed(0),
      ].join(',');
      fs.writeSync(csvFd, line + '\n');
      ++rows;
    }
    if (samples.length > 0) {
      const last = samples[samples.length - 1];
      console.log(`t=${((last.timestamp - t0) / 1000).toFixed(0)}s  world_spd=` +
        `${(sampleValue(last, 'sentry_big_yaw_world_speed_rad_s') * RAD_2_DEG).toFixed(1)}°/s  ` +
        `torque=${sampleValue(last, 'sentry_big_yaw_torque_nm').toFixed(2)}Nm  ` +
        `est_wz=${sampleValue(last, 'est_wz').toFixed(0)}°/s`);
    } else {
      console.log('(本块 0 点:变量名或桥接异常,看 stderr)');
    }
  }
  fs.closeSync(logFd);
  fs.closeSync(csvFd);

  // 摘要:进入拖拽(前 3s 最大角偏)+ 稳态(后 50%)世界角速度
  const s = summarize(csvPath);
  if (s) {
    console.log(`\n摘要(${s.rows} 点):进入3s拖拽=${s.entryDrag.toFixed(1)}°(判据<10°)`);
    console.log(`稳态 |世界角速度| avg=${s.spdAvg.toFixed(2)}°/s max=${s.spdMax.toFixed(2)}°/s(判据<2°/s)`);
    console.log(`稳态 |力矩| avg=${s.tAvg.toFixed(2)}Nm;卸力拍占比=${s.limpPct.toFixed(1)}%`);
    console.log(`\n功率侧:python tools/rtt_pwr_to_csv.py log/${label}_${secs}s.log --power <客户端W> --append steady_raw.csv`);
  } else {
    console.log(`\n样本不足(${rows} 点):桥接 stderr 若是"采样连接"类错误,多半是车断电/未上电`);
  }
  console.log(`saved: ${path.relative(ROOT, csvPath)}\nsaved: ${path.relative(ROOT, logPath)} (${rttBytes}B)`);
}

main().catch((err) => { console.error(err); process.exit(1); });
