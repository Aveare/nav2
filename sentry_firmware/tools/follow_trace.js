/**
 * @file tools/follow_trace.js
 * @brief 跟随律(F1)专用采样:推 yaw 杆 → 底盘跟随的瞬态
 *
 * 采 4 个量(个数少,采样率才够看 0.3~1.0s 的脉冲):
 *   cmd_wz   底盘角速度指令(chassis_cmd_recv.wz,即跟随律输出,deg/s)
 *   est_wz   底盘实测角速度(由四轮反解,deg/s)
 *   off      云台相对底盘的偏角(chassis_cmd_recv.offset_angle,deg)
 *   gz       底盘陀螺 Z 角速度(gimbal_imu_data->Gyro[2],rad/s → 打印时转 deg/s)
 *
 * 判据(自动给出):
 *   - 滞后: off 峰值出现在 cmd 峰值之后的毫秒数
 *   - 过冲: est_wz 峰值 / cmd 稳态(以 cmd 峰值近似)×100%
 *   - 跟随比: ≤cmd 的 est_wz 平台 / cmd 平台
 *   - 抖: est_wz 符号翻转次数
 *
 * 用法:
 *   node tools/follow_trace.js --secs 12                  # 采 12s,存 build/follow_trace.csv
 *   node tools/follow_trace.js --secs 12 --label pulse-0.5
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { call } = require(path.join(__dirname, '..', '.agents', 'skills', '_emberprobe', 'agent-client.js'));

const WORKSPACE = path.resolve(__dirname, '..');
/* 1000 = 一次 RPC 取完整个窗口。原先 50 点/块会在块间留下 ~0.37s 空洞,
 * 20 多块就有 20 多个洞 —— 而转向的峰值/松杆/冲过头恰好都发生在那 0.3s 里,
 * 导致溜车、回摆、收敛全部量不出来(2026-11-21 踩)。重试粒度变粗,可接受。 */
const CHUNK_POINTS = 1000;
const RETRY_PER_CHUNK = 8;
const RETRY_DELAY_MS = 500;

const VARS = {
  cmd_wz: 'chassis_cmd_recv.wz',
  est_wz: 'est_wz',
  off: 'chassis_cmd_recv.offset_angle',
  gz: 'gimbal_imu_data->Gyro[2]',
  ycmd: 'chassis_cmd_recv.yaw_rate_cmd',
};
const KEYS = Object.keys(VARS);

const RAD_2_DEG = 57.29577951308232;

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

function parseArgs(argv) {
  const out = { secs: 12, interval: 20, out: path.join('build', 'follow_trace.csv'), label: '', trigger: 0, wait: 300 };
  for (let i = 2; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === '--secs') out.secs = Number(argv[++i]);
    else if (a === '--interval') out.interval = Number(argv[++i]);
    else if (a === '--out') out.out = argv[++i];
    else if (a === '--label') out.label = argv[++i];
    else if (a === '--trigger') out.trigger = Number(argv[++i]);
    else if (a === '--wait') out.wait = Number(argv[++i]);
    else if (a === '--help' || a === '-h') {
      console.log('用法: node tools/follow_trace.js --secs 12 [--interval 20] [--label 名称] [--out 路径] [--trigger 30]');
      console.log('  --trigger N  等到 |cmd_wz| > N deg/s 才开始记(解决"窗口与操作对不上"),会带约 60 点前摇');
      process.exit(0);
    }
  }
  if (!Number.isFinite(out.secs) || out.secs <= 0) out.secs = 12;
  if (!Number.isFinite(out.interval) || out.interval < 5) out.interval = 20;
  if (!Number.isFinite(out.trigger) || out.trigger < 0) out.trigger = 0;
  if (!Number.isFinite(out.wait) || out.wait <= 0) out.wait = 300;
  return out;
}

function pickNumbers(entry) {
  const values = (entry && entry.values) || {};
  const row = {};
  for (const key of KEYS) {
    const slot = values[VARS[key]];
    const v = slot ? slot.value : null;
    row[key] = typeof v === 'number' && Number.isFinite(v) ? v : null;
  }
  return row;
}

async function sampleSeries(names, totalPoints, intervalMs, trigger = 0, prePoints = 60, waitSec = 300) {
  const samples = [];
  let failedChunks = 0;
  let cursor = 0;
  let armed = !(trigger > 0);
  const waitBuf = []; // 触发前的环形缓冲,触发后作为前摇一起写入
  let guard = 0;

  if (!armed) process.stdout.write(`[待触发] 等 |cmd_wz| > ${trigger} deg/s,车一动就开始记(最多等 ${Math.round(waitSec)} s)\n`);

  const deadline = Date.now() + waitSec * 1000;
  while (cursor < totalPoints && Date.now() < deadline && guard < 800) {
    guard += 1;
    // 未触发时按固定块长轮询;触发后按剩余点数取
    const count = armed ? Math.min(CHUNK_POINTS, totalPoints - cursor) : CHUNK_POINTS;
    let ok = false;
    let lastErr = null;
    let list = [];
    for (let attempt = 1; attempt <= RETRY_PER_CHUNK && !ok; attempt += 1) {
      try {
        const reply = await call(WORKSPACE, 'variables.sample', {
          variables: names.map((name) => ({ name })),
          count,
          intervalMs,
        }, 60000);
        list = (reply && reply.samples) || [];
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
      cursor += count;
      continue;
    }

    const rows = list.map((entry) => ({ t: entry.timestamp, ...pickNumbers(entry) }));

    if (!armed) {
      for (const r of rows) waitBuf.push(r);
      while (waitBuf.length > prePoints) waitBuf.shift();
      const hit = waitBuf.find((r) => r.cmd !== null && Math.abs(r.cmd) > trigger);
      if (hit) {
        armed = true;
        process.stdout.write(`[触发] cmd=${hit.cmd.toFixed(1)} deg/s,开始记录(含 ${waitBuf.length} 点前摇)\n`);
        for (const r of waitBuf) if (cursor < totalPoints) { samples.push(r); cursor += 1; }
      }
    } else {
      for (const r of rows) {
        if (cursor >= totalPoints) break;
        samples.push(r);
        cursor += 1;
      }
    }
  }
  return { samples, failedChunks };
}

/** 找绝对值最大点及其时刻 */
function peak(samples, key) {
  let best = null;
  for (const s of samples) {
    if (s[key] === null) continue;
    if (!best || Math.abs(s[key]) > Math.abs(best.v)) best = { v: s[key], t: s.t };
  }
  return best;
}

/** 平台:绝对值 ≥ 阈值的最长连续段均值 */
function plateau(samples, key, threshold) {
  let best = null;
  let cur = null;
  for (const s of samples) {
    const v = s[key];
    if (v === null) continue;
    if (Math.abs(v) >= threshold) {
      if (!cur) cur = { sum: 0, n: 0 };
      cur.sum += v;
      cur.n += 1;
    } else if (cur) {
      if (!best || cur.n > best.n) best = cur;
      cur = null;
    }
  }
  if (cur && (!best || cur.n > best.n)) best = cur;
  return best ? { mean: best.sum / best.n, n: best.n } : null;
}

function countReversals(samples, key, threshold) {
  let reversals = 0;
  let lastSign = 0;
  for (const s of samples) {
    const v = s[key];
    if (v === null || Math.abs(v) < threshold) continue;
    const sign = v > 0 ? 1 : -1;
    if (lastSign !== 0 && sign !== lastSign) reversals += 1;
    lastSign = sign;
  }
  return reversals;
}

async function main() {
  const args = parseArgs(process.argv);
  try {
    await call(WORKSPACE, 'variables.read', { variables: [{ name: 'chassis_last_ms' }] }, 20000);
  } catch (err) {
    process.stderr.write(`预热失败(继续):${err.message}\n`);
  }

  /* 上限 3000 点:原来卡 1000 点(约 23 s @16ms)不够用 —— 操作手从"我开始采"到真正推杆
   * 常有十几秒延迟,1000 点会把动作切在窗口外。3000 点约 48 s,足够覆盖起步 + 整圈 + 左右大角度。
   * 每块仍是 50 点一次 RPC(MCP 单次上限 1000 点指的是单次调用,不是总量)。 */
  const totalPoints = Math.max(2, Math.min(3000, Math.round((args.secs * 1000) / args.interval)));
  process.stderr.write(`采 ${totalPoints} 点 @${args.interval}ms(分 ${Math.ceil(totalPoints / CHUNK_POINTS)} 块)\n`);
  const { samples, failedChunks } = await sampleSeries(KEYS.map((k) => VARS[k]), totalPoints, args.interval, args.trigger, 60, args.wait);
  if (samples.length === 0) {
    console.log(JSON.stringify({ ok: false, code: 'NO_SAMPLES', message: '一个点都没采到' }));
    process.exit(1);
  }

  const t0 = samples[0].t;
  const csv = ['t_ms,cmd_wz,est_wz,off,gz_dps,ycmd'];
  for (const s of samples) {
    csv.push([
      Math.round(s.t - t0),
      s.cmd_wz === null ? '' : s.cmd_wz.toFixed(2),
      s.est_wz === null ? '' : s.est_wz.toFixed(2),
      s.off === null ? '' : s.off.toFixed(3),
      s.gz === null ? '' : (s.gz * RAD_2_DEG).toFixed(2),
      s.ycmd === null ? '' : s.ycmd.toFixed(1),
    ].join(','));
  }
  const outPath = path.isAbsolute(args.out) ? args.out : path.join(WORKSPACE, args.out);
  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  fs.writeFileSync(outPath, csv.join('\n') + '\n', 'utf8');

  const cmdPeak = peak(samples, 'cmd_wz');
  const estPeak = peak(samples, 'est_wz');
  const offPeak = peak(samples, 'off');
  const cmdPlateau = plateau(samples, 'cmd_wz', 20);
  const estPlateau = plateau(samples, 'est_wz', 20);
  const reversals = countReversals(samples, 'est_wz', 15);

  const lagMs = cmdPeak && offPeak ? Math.round(offPeak.t - cmdPeak.t) : null;
  const overshootPct = cmdPeak && estPeak && Math.abs(cmdPeak.v) > 1
    ? Number(((Math.abs(estPeak.v) / Math.abs(cmdPeak.v)) * 100).toFixed(1)) : null;
  const followRatio = cmdPlateau && estPlateau && Math.abs(cmdPlateau.mean) > 1
    ? Number(((estPlateau.mean / cmdPlateau.mean) * 100).toFixed(1)) : null;

  const duration = samples[samples.length - 1].t - t0;
  console.log(`采样 ${samples.length}/${totalPoints} 点,时长 ${(duration / 1000).toFixed(1)}s,失败块 ${failedChunks}`);
  console.log(`指令峰值 cmd_wz = ${cmdPeak ? cmdPeak.v.toFixed(1) : '-'} deg/s`);
  console.log(`实测峰值 est_wz = ${estPeak ? estPeak.v.toFixed(1) : '-'} deg/s`);
  console.log(`偏角峰值 off    = ${offPeak ? offPeak.v.toFixed(2) : '-'} deg`);
  console.log(`跟随比 est/cmd  = ${followRatio === null ? '-' : followRatio + '%'}  (平台段${estPlateau ? estPlateau.n : 0}点)`);
  console.log(`峰值得分        = ${overshootPct === null ? '-' : overshootPct + '%'}  (>120% 说明过冲,<90% 说明跟不上)`);
  console.log(`off 峰滞后      = ${lagMs === null ? '-' : lagMs + ' ms'}  (负=offset 先建,正=指令先动;|值|>200ms 注意)`);
  console.log(`est_wz 翻转次数 = ${reversals}`);
  console.log(`波形: ${outPath}`);
}

main().catch((err) => {
  console.log(JSON.stringify({ ok: false, code: 'ERROR', message: err.message }));
  process.exit(1);
});
