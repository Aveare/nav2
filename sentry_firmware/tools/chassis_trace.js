/**
 * @file tools/chassis_trace.js
 * @brief 底盘速度环波形采集(非侵入,不暂停核心)
 *
 * 用途:
 *   S1「指令→速度」、S2 小陀螺、T2 落地短冲——把 est_vx/est_vy/est_wz 与四轮实测转速
 *   同时采下来,判断"杆量指令有没有真的变成轮子转速"。
 *
 * 为什么不用 tools/sample_scalars.js:
 *   那条路每个采样点都要 variables.read 一次,而读变量必须暂停核心 → 底盘任务停发 CAN,
 *   驱动被中断,采到的是"车一顿一顿"的波形。本工具走桥接的 variables.sample 一次取整段,
 *   全程不暂停核心(与 tools/pitch_calib.js 的 sweep 同一机制)。
 *
 * 用法:
 *   node tools/chassis_trace.js --secs 12 --interval 40
 *   node tools/chassis_trace.js --secs 12 --keep      # 不做平台段摘要,只存 CSV
 *   node tools/chassis_trace.js --out build/chassis_s1.csv
 *
 * 输出:
 *   build/chassis_trace.csv(默认) ——
 *     t_ms, est_vx/vy/wz, cmd_vx/vy/wz, ref_lf..rb, spd_lf..rb, cur_lf..rb, state, auth,
 *     off(云台-底盘夹角), mode(底盘模式,跟随=3), gyaw(云台世界角), gz(云台世界角速度)
 *   终端摘要 —— 每轴的最大值/平台均值/平台时长(平台=|值|≥阈值且连续)
 *
 * 注意:
 *   - 依赖 EmberProbe bridge;核心在跑、探针独占。
 *   - 采样期间不要同时开别的读变量工具。
 */
'use strict';

const fs = require('fs');
const path = require('path');

const WORKSPACE = path.resolve(__dirname, '..');
// 与 tools/pitch_calib.js 同一条桥接通道
const { call } = require(path.join(__dirname, '..', '.agents', 'skills', '_emberprobe', 'agent-client.js'));

/* 400 = 尽量一次取完整个窗口:50 点/块会在块间留 0.3~0.5s 空洞,
 * 而起步/松杆都发生在那 0.2s 里(2026-11-21 踩过)。通道多时偶尔整套失败,失败会重试。 */
const CHUNK_POINTS = 400;
const RETRY_PER_CHUNK = 8;     // 实测单块成功率约 60%(失败为 failed to read memory),故重试给足
const RETRY_DELAY_MS = 500;

const VARS = [
  'est_vx',
  'est_vy',
  'est_wz',
  'chassis_cmd_recv.vx',   // 指令:判断"推杆有没有真的下达"
  'chassis_cmd_recv.vy',
  'chassis_cmd_recv.wz',
  'vt_lf',                 // 轮速参考(限幅后):速度环的输入
  'vt_rf',
  'vt_lb',
  'vt_rb',
  'motor_lf->measure.speed_aps',
  'motor_rf->measure.speed_aps',
  'motor_lb->measure.speed_aps',
  'motor_rb->measure.speed_aps',
  'motor_lf->measure.real_current',
  'motor_rf->measure.real_current',
  'motor_lb->measure.real_current',
  'motor_rb->measure.real_current',
  'robot_state',
  'authority_source',
  'chassis_cmd_recv.offset_angle',      // 跟随环被控量:云台相对底盘夹角
  'chassis_cmd_recv.chassis_mode',      // 确认真的在跟随态(mode=3),否则采到的是零力段
  'gimbal_imu_data->YawTotalAngle',      // 云台实际世界角
  'gimbal_imu_data->Gyro[2]',            // 云台世界系 yaw 角速度 = 跟随律的前馈输入
];

const COLS = [
  'est_vx',
  'est_vy',
  'est_wz',
  'cmd_vx',
  'cmd_vy',
  'cmd_wz',
  'ref_lf',
  'ref_rf',
  'ref_lb',
  'ref_rb',
  'spd_lf',
  'spd_rf',
  'spd_lb',
  'spd_rb',
  'cur_lf',
  'cur_rf',
  'cur_lb',
  'cur_rb',
  'state',
  'auth',
  'off',
  'mode',
  'gyaw',
  'gz',
];

// CSV 列名 → 变量名(两者不同名时容易读错,集中在这里映射)
const COL_TO_VAR = {
  est_vx: 'est_vx',
  est_vy: 'est_vy',
  est_wz: 'est_wz',
  cmd_vx: 'chassis_cmd_recv.vx',
  cmd_vy: 'chassis_cmd_recv.vy',
  cmd_wz: 'chassis_cmd_recv.wz',
  ref_lf: 'vt_lf',
  ref_rf: 'vt_rf',
  ref_lb: 'vt_lb',
  ref_rb: 'vt_rb',
  spd_lf: 'motor_lf->measure.speed_aps',
  spd_rf: 'motor_rf->measure.speed_aps',
  spd_lb: 'motor_lb->measure.speed_aps',
  spd_rb: 'motor_rb->measure.speed_aps',
  cur_lf: 'motor_lf->measure.real_current',
  cur_rf: 'motor_rf->measure.real_current',
  cur_lb: 'motor_lb->measure.real_current',
  cur_rb: 'motor_rb->measure.real_current',
  state: 'robot_state',
  auth: 'authority_source',
  off: 'chassis_cmd_recv.offset_angle',
  mode: 'chassis_cmd_recv.chassis_mode',
  gyaw: 'gimbal_imu_data->YawTotalAngle',
  gz: 'gimbal_imu_data->Gyro[2]',
};
// 平台判据:mm/s 与 deg/s 分开给阈值
const PLATEAU = {
  est_vx: 300,
  est_vy: 300,
  est_wz: 100,
};

const ENUM_NUM = {
  ROBOT_STOP: 0,
  ROBOT_READY: 1,
  ROBOT_RUNNING: 2,
  AUTH_RC_LOST: 0,
  AUTH_KILL_SWITCH: 1,
  AUTH_STOP_LATCHED: 2,
  AUTH_RUN: 3,
  /* chassis_mode_e(app_types.h):跟随态必须是 3,否则录到的是零力/不跟随段 */
  CHASSIS_ZERO_FORCE: 0,
  CHASSIS_ROTATE: 1,
  CHASSIS_NO_FOLLOW: 2,
  CHASSIS_FOLLOW_GIMBAL_YAW: 3,
  CHASSIS_ALIGN: 4,
};

const AUTH_NAME = {
  0: 'RC_LOST',
  1: 'KILL_SWITCH',
  2: 'STOP_LATCHED',
  3: 'RUN',
};

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

function parseArgs(argv) {
  const out = {
    secs: 12,
    interval: 40,
    out: path.join('build', 'chassis_trace.csv'),
    keep: false,
    json: false,
  };
  for (let i = 2; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === '--secs') out.secs = Number(argv[++i]);
    else if (a === '--interval') out.interval = Number(argv[++i]);
    else if (a === '--out') out.out = argv[++i];
    else if (a === '--keep') out.keep = true;
    else if (a === '--json') out.json = true;
    else if (a === '--help' || a === '-h') {
      console.log('用法: node tools/chassis_trace.js --secs 12 --interval 40 [--out 路径] [--keep] [--json]');
      process.exit(0);
    }
  }
  if (!Number.isFinite(out.secs) || out.secs <= 0) out.secs = 12;
  if (!Number.isFinite(out.interval) || out.interval < 5) out.interval = 40;
  return out;
}

/** 从 variables.sample 的返回里取 {name: value},字符串枚举转数字 */
function pickValues(entry, names) {
  const values = (entry && entry.values) || {};
  const result = {};
  for (const name of names) {
    const slot = values[name];
    if (!slot) {
      result[name] = null;
      continue;
    }
    let v = slot.value;
    if (typeof v === 'string') {
      if (Object.prototype.hasOwnProperty.call(ENUM_NUM, v)) v = ENUM_NUM[v];
      else {
        const m = /^[-+]?\d+/.exec(v);
        v = m ? Number(m[0]) : null;
      }
    }
    result[name] = typeof v === 'number' && Number.isFinite(v) ? v : null;
  }
  return result;
}

/** 分块采样:每块 CHUNK_POINTS 点,块内失败重试,块间失败不中止 */
async function sampleSeries(names, totalPoints, intervalMs) {
  const samples = [];
  let failedChunks = 0;
  let cursor = 0;
  while (cursor < totalPoints) {
    const count = Math.min(CHUNK_POINTS, totalPoints - cursor);
    let ok = false;
    let lastErr = null;
    for (let attempt = 1; attempt <= RETRY_PER_CHUNK && !ok; attempt += 1) {
      try {
        const reply = await call(WORKSPACE, 'variables.sample', {
          variables: names.map((name) => ({ name })),
          count,
          intervalMs,
        }, 60000);
        const list = (reply && reply.samples) || [];
        for (const entry of list) {
          samples.push({ timestamp: entry.timestamp, values: pickValues(entry, names) });
        }
        ok = list.length > 0;
        if (!ok) lastErr = new Error('该块返回 0 点');
      } catch (err) {
        lastErr = err;
        await sleep(RETRY_DELAY_MS);
      }
    }
    if (!ok) {
      failedChunks += 1;
      process.stderr.write(`[块 ${cursor}~${cursor + count - 1}] 失败:${lastErr && lastErr.message}\n`);
    }
    cursor += count;
  }
  return { samples, failedChunks };
}

/** 找连续平台段:返回 {max, mean, ms, t0, t1} */
function plateau(samples, key, threshold) {
  let best = null;
  let cur = null;
  for (const s of samples) {
    const v = s.values[key];
    if (v === null || v === undefined) continue;
    if (Math.abs(v) >= threshold) {
      if (!cur) cur = { sum: 0, n: 0, t0: s.timestamp, t1: s.timestamp, peak: 0 };
      cur.sum += v;
      cur.n += 1;
      cur.t1 = s.timestamp;
      if (Math.abs(v) > Math.abs(cur.peak)) cur.peak = v;
    } else if (cur) {
      if (!best || cur.n > best.n) best = cur;
      cur = null;
    }
  }
  if (cur && (!best || cur.n > best.n)) best = cur;
  if (!best) return null;
  return {
    max: best.peak,
    mean: best.sum / best.n,
    points: best.n,
    ms: best.t1 - best.t0,
  };
}

async function main() {
  const args = parseArgs(process.argv);

  // 预热:冷启动第一条 RPC 常报 failed to read memory
  try {
    await call(WORKSPACE, 'variables.read', { variables: [{ name: 'chassis_last_ms' }] }, 20000);
  } catch (err) {
    process.stderr.write(`预热失败(继续尝试采样):${err.message}\n`);
  }

  const totalPoints = Math.max(2, Math.min(1000, Math.round((args.secs * 1000) / args.interval)));
  process.stderr.write(`采样 ${totalPoints} 点 @${args.interval}ms ≈ ${(totalPoints * args.interval) / 1000}s(分 ${Math.ceil(totalPoints / CHUNK_POINTS)} 块)\n`);

  const { samples, failedChunks } = await sampleSeries(VARS, totalPoints, args.interval);
  if (samples.length === 0) {
    console.log(JSON.stringify({ ok: false, code: 'NO_SAMPLES', message: '一个点都没采到' }));
    process.exit(1);
  }

  const t0 = samples[0].timestamp;
  const lines = ['t_ms,' + COLS.join(',')];
  for (const s of samples) {
    const row = [Math.round(s.timestamp - t0)];
    for (const col of COLS) {
      const key = COL_TO_VAR[col];
      const v = s.values[key];
      row.push(v === null || v === undefined ? '' : Number(v).toFixed(2));
    }
    lines.push(row.join(','));
  }
  const outPath = path.isAbsolute(args.out) ? args.out : path.join(WORKSPACE, args.out);
  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  fs.writeFileSync(outPath, lines.join('\n') + '\n', 'utf8');

  const summary = {};
  for (const key of Object.keys(PLATEAU)) {
    const p = plateau(samples, key, PLATEAU[key]);
    if (p) summary[key] = { max: Number(p.max.toFixed(1)), mean: Number(p.mean.toFixed(1)), ms: p.ms, points: p.points };
  }

  const authValues = [...new Set(samples.map((s) => s.values.authority_source).filter((v) => v !== null))];
  const result = {
    ok: true,
    points: samples.length,
    requested: totalPoints,
    failedChunks,
    durationMs: samples[samples.length - 1].timestamp - t0,
    out: outPath,
    plateau: args.keep ? undefined : summary,
    auth: authValues.map((v) => `${v}:${AUTH_NAME[v] || '?'}`),
  };

  if (args.json) {
    console.log(JSON.stringify(result, null, 2));
  } else {
    console.log(`采样 ${result.points}/${result.requested} 点,时长 ${(result.durationMs / 1000).toFixed(1)}s,失败块 ${failedChunks}`);
    console.log(`状态: ${result.auth.join(' / ') || '无'}`);
    if (!args.keep) {
      const rows = Object.entries(summary);
      if (rows.length === 0) {
        console.log('平台段:无(|值| 未超过阈值,车没动或没出力)');
      } else {
        console.log('平台段(取最长连续段):');
        for (const [k, v] of rows) console.log(`  ${k}: 峰值 ${v.max}, 平台均值 ${v.mean}, 持续 ${v.ms}ms (${v.points} 点)`);
      }
    }
    console.log(`波形: ${outPath}`);
  }
  process.exit(0);
}

main().catch((err) => {
  console.log(JSON.stringify({ ok: false, code: 'ERROR', message: err.message }));
  process.exit(1);
});
