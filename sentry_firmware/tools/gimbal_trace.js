/**
 * @file tools/gimbal_trace.js
 * @brief 云台 yaw 闭环波形采集(非侵入,不暂停核心)
 *
 * 用途:
 *   yaw 轴的基线检查与整定取证 —— 静态残差 / 静息抖动 / 到位过冲 / 松杆回弹 /
 *   恒速段的稳态电流。对应 `.Doc/detail/yaw轴深化改造计划.md` 的 B1/B2 判据与 P2 摩擦标定。
 *
 * 为什么不用 tools/sample_scalars.js:
 *   那条路每个点都要 variables.read 一次,而读变量必须暂停核心 → 云台任务停发 CAN,
 *   C620/GM6020 约 100ms 无帧即按 0 输出,枪管/云台会被重力拽一下再抬回来 ——
 *   采到的是"每采一点砸一次"的假波形(2026-11-16 踩过,一整天数据作废)。
 *   本工具走桥接 `variables.sample`,一次取整段,全程不暂停核心。
 *
 * 用法:
 *   node tools/gimbal_trace.js --secs 10 --interval 20          # 静态 10s(B1)
 *   node tools/gimbal_trace.js --secs 12 --label push-30        # 手推偏 30° 松手(B2)
 *   node tools/gimbal_trace.js --secs 6 --interval 20 --keep    # 只存 CSV
 *   node tools/gimbal_trace.js --out build/yaw_b1.csv
 *
 * 输出:
 *   build/gimbal_trace.csv —— t_ms,yaw_ang,yaw_ref,err,yaw_gyro,out,iout,spd,cur,temp,off
 *   终端摘要 —— 误差统计 / 静息抖动峰峰值 / 过冲 / 温度 / 稳态电流
 *
 * 注意:
 *   - 依赖 EmberProbe bridge;核心在跑、探针独占。
 *   - 采样期间不要同时开别的读变量工具。
 *   - 判据取自 yaw 计划 P1:稳态 |fb-ref| < 0.5°、静息 out 峰峰值 < 50 码。
 */

'use strict';

const fs = require('fs');
const path = require('path');

const WORKSPACE = path.resolve(__dirname, '..');
const { call } = require(path.join(__dirname, '..', '.agents', 'skills', '_emberprobe', 'agent-client.js'));

let CHUNK_POINTS = 50;        // 单次 RPC 点数;--fast 档提到 200,把 RPC 开销摊薄
const RETRY_PER_CHUNK = 8;    // 单块成功率约 60%,给足重试
const RETRY_DELAY_MS = 500;
const RAD_2_DEG = 57.29577951308232;

const VARS_FULL = {
  yaw_ang: 'gimbal_imu_data->YawTotalAngle',            // 世界系多圈角,deg(角度环反馈)
  yaw_ref: 'gimbal_cmd_recv.yaw',                      // 角度环目标,deg
  yaw_gyro: 'gimbal_imu_data->Gyro[2]',                 // rad/s(速度环反馈)
  out: 'yaw_motor->motor_controller.speed_PID.Output', // 电流码
  iout: 'yaw_motor->motor_controller.speed_PID.Iout',  // 积分项,码
  spd: 'yaw_motor->measure.speed_aps',                 // 关节速度,deg/s(摩擦前馈用这个)
  cur: 'yaw_motor->measure.real_current',              // 实测电流码
  temp: 'yaw_motor->measure.temperature',              // ℃
  off: 'chassis_cmd_recv.offset_angle',                // 云台-底盘夹角,deg(跟随律用)
  state: 'robot_state',                                // 状态机(判断是否解锁)
  auth: 'authority_source',                            // 权限源(1=上挡零力,3=RUN)
};

/* 快速档:变量个数直接决定采样率 —— 桥接每个变量是一次独立 SWD 读,
 * 2026-11-16 实测同 interval=20ms 下 9 变量 16.5Hz、5 变量 27.7Hz。
 * 测阶跃/回弹这种亚秒级过程要用 --fast,否则 38ms 分辨率会把拐点抹平。 */
const VARS_FAST = {
  yaw_ang: 'gimbal_imu_data->YawTotalAngle',
  yaw_ref: 'gimbal_cmd_recv.yaw',
  spd: 'yaw_motor->measure.speed_aps',
  out: 'yaw_motor->motor_controller.speed_PID.Output',
  cur: 'yaw_motor->measure.real_current',
};

let VARS = VARS_FULL;
let KEYS = Object.keys(VARS);

/** yaw 计划 P1 的判据(2026-11-16 按实测修订:用机械量与稳健分位,不用 p2p)
 *  旧判据"静息 Output 峰峰值 < 50 码"是电压域写的;电流域下单点尖峰(GM6020 速度反馈 glitch)
 *  会把 p2p 抬到几百码,而中位输出只有个位数 —— p2p 会把正常基线判成 FAIL。 */
const CRIT_ANGLE_P2P_DEG = 0.30;  // 静息角位移峰峰值
const CRIT_STEADY_DEG = 0.50;     // 静态残差(应落在角度死区内)
const CRIT_OUT_P90 = 60;          // 输出绝对值 p90,码
const CRIT_TEMP = 50;             // ℃

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

function parseArgs(argv) {
  const out = { secs: 10, interval: 20, out: path.join('build', 'gimbal_trace.csv'), label: '', keep: false, fast: false };
  for (let i = 2; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === '--secs') out.secs = Number(argv[++i]);
    else if (a === '--interval') out.interval = Number(argv[++i]);
    else if (a === '--out') out.out = argv[++i];
    else if (a === '--label') out.label = argv[++i];
    else if (a === '--fast') out.fast = true;
    else if (a === '--keep') out.keep = true;
    else if (a === '--help' || a === '-h') {
      console.log('用法: node tools/gimbal_trace.js --secs 10 [--interval 20] [--label 名称] [--out 路径] [--fast] [--keep]');
      console.log('  --fast  只采 5 个变量(角/目标/关节速度/输出/电流),~26Hz;默认 9 变量 ~16Hz');
      process.exit(0);
    }
  }
  if (!Number.isFinite(out.secs) || out.secs <= 0) out.secs = 10;
  if (!Number.isFinite(out.interval) || out.interval < 5) out.interval = 20;
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
        for (const entry of list) samples.push({ t: entry.timestamp, ...pickNumbers(entry) });
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

function stat(values) {
  const v = values.filter((x) => typeof x === 'number' && Number.isFinite(x));
  if (!v.length) return null;
  const mean = v.reduce((a, b) => a + b, 0) / v.length;
  const sorted = [...v].sort((a, b) => a - b);
  const std = Math.sqrt(v.reduce((a, b) => a + (b - mean) ** 2, 0) / v.length);
  const pct = (p) => sorted[Math.round((p / 100) * (sorted.length - 1))];
  return {
    mean, std, min: sorted[0], max: sorted[sorted.length - 1], n: v.length,
    p2p: sorted[sorted.length - 1] - sorted[0], p05: pct(5), p50: pct(50), p95: pct(95),
  };
}

async function main() {
  const args = parseArgs(process.argv);
  if (args.fast) { VARS = VARS_FAST; KEYS = Object.keys(VARS); CHUNK_POINTS = 200; }

  // 预热:冷启动首条 RPC 常报 failed to read memory,先探一次
  try {
    await call(WORKSPACE, 'variables.read', { variables: [{ name: 'chassis_last_ms' }] }, 20000);
  } catch (err) {
    process.stderr.write(`预热失败(继续):${err.message}\n`);
  }

  const totalPoints = Math.max(2, Math.min(1000, Math.round((args.secs * 1000) / args.interval)));
  process.stderr.write(`采 ${totalPoints} 点 @${args.interval}ms(分 ${Math.ceil(totalPoints / CHUNK_POINTS)} 块)\n`);
  const { samples, failedChunks } = await sampleSeries(KEYS.map((k) => VARS[k]), totalPoints, args.interval);
  if (samples.length === 0) {
    console.log(JSON.stringify({ ok: false, code: 'NO_SAMPLES', message: '一个点都没采到' }));
    process.exit(1);
  }

  const t0 = samples[0].t;
  const csv = ['t_ms,yaw_ang,yaw_ref,err,yaw_gyro,out,iout,spd,cur,temp,off'];
  for (const s of samples) {
    const err = s.yaw_ang !== null && s.yaw_ref !== null ? s.yaw_ref - s.yaw_ang : null;
    csv.push([
      Math.round(s.t - t0),
      s.yaw_ang === null ? '' : s.yaw_ang.toFixed(2),
      s.yaw_ref === null ? '' : s.yaw_ref.toFixed(2),
      err === null ? '' : err.toFixed(3),
      s.yaw_gyro === null ? '' : (s.yaw_gyro * RAD_2_DEG).toFixed(2),
      s.out === null ? '' : s.out.toFixed(0),
      s.iout === null ? '' : s.iout.toFixed(0),
      s.spd === null ? '' : s.spd.toFixed(1),
      s.cur === null ? '' : s.cur.toFixed(0),
      s.temp === null ? '' : s.temp.toFixed(0),
      s.off === null ? '' : s.off.toFixed(2),
    ].join(','));
  }
  const outPath = path.isAbsolute(args.out) ? args.out : path.join(WORKSPACE, args.out);
  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  fs.writeFileSync(outPath, csv.join('\n') + '\n', 'utf8');

  const errs = samples.map((s) => (s.yaw_ang !== null && s.yaw_ref !== null ? s.yaw_ref - s.yaw_ang : null));
  const errStat = stat(errs);
  const outStat = stat(samples.map((s) => s.out));
  const curStat = stat(samples.map((s) => s.cur));
  const spdStat = stat(samples.map((s) => s.spd));
  const spdAbs = stat(samples.map((s) => (s.spd === null ? null : Math.abs(s.spd))));
  const tempStat = stat(samples.map((s) => s.temp));
  const gyroStat = stat(samples.map((s) => (s.yaw_gyro === null ? null : s.yaw_gyro * RAD_2_DEG)));

  // 静息段:目标未变(前后 ref 差 <0.2°)且 |关节速度| < 2°/s 的样本
  const quiet = [];
  for (let i = 2; i < samples.length; i += 1) {
    const a = samples[i];
    const refSteady = a.yaw_ref !== null && samples[i - 2].yaw_ref !== null
      && Math.abs(a.yaw_ref - samples[i - 2].yaw_ref) < 0.2;
    const slow = a.spd !== null && Math.abs(a.spd) < 2.0;
    if (refSteady && slow) quiet.push(a);
  }
  const quietOut = stat(quiet.map((s) => s.out));

  const duration = samples[samples.length - 1].t - t0;
  console.log(`采样 ${samples.length}/${totalPoints} 点,时长 ${(duration / 1000).toFixed(1)}s,失败块 ${failedChunks}`);
  console.log(`误差 err=ref-ang : 均值 ${errStat ? errStat.mean.toFixed(3) : '-'}°, 峰峰 ${errStat ? errStat.p2p.toFixed(2) : '-'}°, max|.| ${errStat ? Math.max(Math.abs(errStat.min), Math.abs(errStat.max)).toFixed(2) : '-'}°`);
  console.log(`关节速度 spd    : 均值 ${spdStat ? spdStat.mean.toFixed(1) : '-'}°/s, 峰峰 ${spdStat ? spdStat.p2p.toFixed(1) : '-'}°/s, max|.| ${spdAbs ? spdAbs.max.toFixed(1) : '-'}°/s`);
  console.log(`IMU Gyro[2]     : 均值 ${gyroStat ? gyroStat.mean.toFixed(2) : '-'}°/s, 峰峰 ${gyroStat ? gyroStat.p2p.toFixed(2) : '-'}°/s`);
  console.log(`速度环 Output   : 均值 ${outStat ? outStat.mean.toFixed(0) : '-'}, 峰峰 ${outStat ? outStat.p2p.toFixed(0) : '-'} 码`);
  console.log(`静息 Output     : ${quietOut ? `${quietOut.n} 点, 均值 ${quietOut.mean.toFixed(0)}, 峰峰 ${quietOut.p2p.toFixed(0)} 码` : '无静息段'}`);
  console.log(`实测电流 cur    : 均值 ${curStat ? curStat.mean.toFixed(0) : '-'} 码, max|.| ${curStat ? Math.max(Math.abs(curStat.min), Math.abs(curStat.max)).toFixed(0) : '-'}`);
  console.log(`温度 temp       : ${tempStat ? `${tempStat.min.toFixed(0)}~${tempStat.max.toFixed(0)}℃` : '-'}`);

  if (!args.keep) {
    const verdicts = [];
    const angStat = stat(samples.map((s) => s.yaw_ang));
    const quietAbs = quiet.map((s) => (s.out === null ? null : Math.abs(s.out)));
    const quietAbsStat = stat(quietAbs);
    if (angStat) {
      const angleOk = angStat.p2p < CRIT_ANGLE_P2P_DEG;
      verdicts.push(`静息角位移 ${angleOk ? 'PASS' : `FAIL(${CRIT_ANGLE_P2P_DEG}°)`}(${angStat.p2p.toFixed(2)}°)`);
    }
    if (errStat) {
      const steadyOk = Math.max(Math.abs(errStat.min), Math.abs(errStat.max)) < CRIT_STEADY_DEG;
      verdicts.push(`静态残差 ${steadyOk ? 'PASS' : `FAIL(>${CRIT_STEADY_DEG}°)`}(${errStat.mean.toFixed(2)}°)`);
    }
    if (quietAbsStat) {
      const outOk = quietAbsStat.p95 < CRIT_OUT_P90;
      verdicts.push(`静息输出 p95 ${outOk ? 'PASS' : `FAIL(>${CRIT_OUT_P90}码)`}(${quietAbsStat.p95.toFixed(0)}码, 中位 ${quietAbsStat.p50.toFixed(0)})`);
    }
    if (tempStat) verdicts.push(`温度 ${tempStat.max < CRIT_TEMP ? 'PASS' : `FAIL(${CRIT_TEMP}℃)`}`);
    console.log(`判据(P1)       : ${verdicts.join(' / ')}`);
    if (Math.abs(outStat ? outStat.max : 0) >= 16000) console.log('告警:Output 触 ±16384(±3A 电流上限),增益或前馈可能过头');
    if (spdAbs && spdAbs.max > 400) console.log('告警:关节速度 > 400°/s,注意 `YAW_REF_FF_MAX_RATE 450` 的阈值区');
    const authStat = stat(samples.map((s) => s.auth));
    if (authStat && authStat.max !== 3) console.log(`提示:auth 未到 3(RUN) —— 本次是 ${authStat.max === 1 ? '上挡零力' : authStat.max === 2 ? '急停锁存' : '遥控丢失'},数据不能当基线`);
  }
  if (args.label) console.log(`标签            : ${args.label}`);
  console.log(`波形: ${outPath}`);
}

main().catch((err) => {
  console.log(JSON.stringify({ ok: false, code: 'ERROR', message: err.message }));
  process.exit(1);
});
