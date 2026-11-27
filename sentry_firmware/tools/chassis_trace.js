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
 *   node tools/chassis_trace.js --robot sentry --secs 12 --interval 40
 *   node tools/chassis_trace.js --secs 12 --keep      # 不做平台段摘要,只存 CSV
 *   node tools/chassis_trace.js --out build/chassis_s1.csv
 *
 * 采集闸门(2026-11-27 起,见 tools/lib/capture_guard.js):
 *   - ELF 钉到 firmware/<robot>.elf,不再依赖会被别的兵种构建覆盖的 current.elf;
 *   - 读一次变量核对 flashedSha256 == elfSha256,不一致默认拒绝(车上固件不同源时
 *     变量地址可能漂移);确认风险可加 --allow-mismatch 强制采,身份写入终端输出;
 *   - build/.capture.lock 互斥:RTT/其它采集正在跑时直接退出,不抢探针。
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
const guard = require(path.join(__dirname, 'lib', 'capture_guard.js'));

/* 400 = 尽量一次取完整个窗口:50 点/块会在块间留 0.3~0.5s 空洞,
 * 而起步/松杆都发生在那 0.2s 里(2026-11-21 踩过)。通道多时偶尔整套失败,失败会重试。 */
const CHUNK_POINTS = 400;
const RETRY_PER_CHUNK = 3;     // 只重试偶发 failed to read memory;硬错误一次就退出
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

/** 闸门拒绝:给一行原因 + 一行补救,退出码 2(不要静默继续采错数据) */
function fail(message, hint) {
  const payload = { ok: false, code: 'CAPTURE_GUARD', message, hint: hint || null };
  console.log(JSON.stringify(payload));
  process.stderr.write(`采集拒绝:${message}\n`);
  if (hint) process.stderr.write(`补救:${hint}\n`);
  process.exit(2);
}

function parseArgs(argv) {
  const out = {
    secs: 12,
    interval: 40,
    out: path.join('build', 'chassis_trace.csv'),
    keep: false,
    json: false,
    robot: null,
    allowMismatch: false,
  };
  for (let i = 2; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === '--secs') out.secs = Number(argv[++i]);
    else if (a === '--interval') out.interval = Number(argv[++i]);
    else if (a === '--out') out.out = argv[++i];
    else if (a === '--robot') out.robot = argv[++i];
    else if (a === '--allow-mismatch') out.allowMismatch = true;
    else if (a === '--keep') out.keep = true;
    else if (a === '--json') out.json = true;
    else if (a === '--help' || a === '-h') {
      console.log(
        '用法: node tools/chassis_trace.js --robot <infantry|sentry|heavy> --secs 12 --interval 40 ' +
          '[--out 路径] [--keep] [--json] [--allow-mismatch]'
      );
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

/** 分块采样:每块 CHUNK_POINTS 点;只有偶发内存读失败才重试,硬错误立刻中止 */
async function sampleSeries(names, totalPoints, intervalMs) {
  const samples = [];
  let failedChunks = 0;
  let abortError = null;
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
        if (!guard.isTransient(err)) break; // 探针/桥/会话类硬错误:重试只会把失败拖成几分钟
        await sleep(RETRY_DELAY_MS);
      }
    }
    if (!ok) {
      failedChunks += 1;
      process.stderr.write(`[块 ${cursor}~${cursor + count - 1}] 失败:${lastErr && lastErr.message}\n`);
      if (!guard.isTransient(lastErr)) {
        abortError = lastErr;
        break;
      }
    }
    cursor += count;
  }
  return { samples, failedChunks, abortError };
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

  /* ---- 闸门 1:兵种 / ELF 身份(不依赖会被别的兵种构建覆盖的 current.elf) ---- */
  const robot = args.robot || guard.currentProfile(WORKSPACE);
  if (!robot) {
    fail('无法确定兵种:firmware/current.txt 缺失或没有 PROFILE', '用 --robot <infantry|sentry|heavy> 指定');
  }
  const elf = guard.checkRobotElf(WORKSPACE, robot);
  if (!elf.ok) fail(`ELF 身份不对:${elf.problems.join('; ')}`, elf.remedy);
  if (elf.warnings && elf.warnings.length) {
    for (const w of elf.warnings) process.stderr.write(`警告:${w}\n`);
  }

  /* ---- 闸门 2:采集互斥(同一探针不允许两个采集进程) ---- */
  const lock = guard.acquireLock(WORKSPACE, 'chassis_trace');
  if (!lock.ok) {
    fail(
      `已有采集进程在跑:${lock.owner.tool}(pid ${lock.owner.pid}, ${lock.owner.at})`,
      '等它结束或先关掉它;确认进程已死可删 build/.capture.lock'
    );
  }
  process.on('exit', () => guard.releaseLock(WORKSPACE));

  /* ---- 闸门 3:钉 ELF + 核对车上固件(不同源时地址可能漂移) ---- */
  try {
    const pin = await guard.pinElf(call, WORKSPACE, elf.elfPath);
    process.stderr.write(
      `采集身份:${robot} / firmware/${robot}.elf / SHA ${String(elf.elfSha).slice(0, 8)}` +
        (pin.changed ? `(ELF 原指向 ${pin.from || '?'})` : '') +
        '\n'
    );
    /* 冷启动第一条 RPC 常报 OPENOCD_START_FAILED(实测 2026-11-27),给 3 次重试;
     * 硬错误(目标不匹配等)立即放弃。 */
    let pre = null;
    let lastErr = null;
    for (let attempt = 1; attempt <= 3 && !pre; attempt += 1) {
      try {
        pre = await call(WORKSPACE, 'variables.read', { variables: [{ name: 'est_wz' }] }, 20000);
      } catch (err) {
        lastErr = err;
        if (!guard.isTransient(err) || attempt === 3) break;
        process.stderr.write(`握手失败,重试 ${attempt}/3:${err.message}\n`);
        await sleep(1000);
      }
    }
    if (!pre) throw lastErr || new Error('握手无返回');
    const cons = guard.firmwareConsistency(pre);
    if (!cons.ok) {
      const msg =
        `车上固件与磁盘 ELF 不同源: flashed ${String(cons.flashedSha256).slice(0, 8)}` +
        ` != elf ${String(cons.elfSha256).slice(0, 8)}`;
      if (!args.allowMismatch) {
        fail(msg + '(变量地址可能已漂移)', `先 ${elf.remedy} 并烧录;确认风险可加 --allow-mismatch`);
      }
      process.stderr.write(`警告:${msg};--allow-mismatch 强制采集,数据可信度自负\n`);
    }
  } catch (err) {
    fail(
      `采集前握手失败:${err.message}`,
      'OPENOCD_START_FAILED 时先关掉其它占用探针的会话(Ozone/RTT/采样),再重试'
    );
  }

  const totalPoints = Math.max(2, Math.min(1000, Math.round((args.secs * 1000) / args.interval)));
  process.stderr.write(`采样 ${totalPoints} 点 @${args.interval}ms ≈ ${(totalPoints * args.interval) / 1000}s(分 ${Math.ceil(totalPoints / CHUNK_POINTS)} 块)\n`);

  const { samples, failedChunks, abortError } = await sampleSeries(VARS, totalPoints, args.interval);
  if (samples.length === 0) {
    console.log(
      JSON.stringify({
        ok: false,
        code: 'NO_SAMPLES',
        message: '一个点都没采到',
        reason: abortError && abortError.message,
      })
    );
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
    aborted: abortError ? abortError.message : null,
    identity: { robot, elf: elf.elfPath, elfSha256: elf.elfSha },
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
  if (abortError) {
    process.stderr.write(`采集中止:${abortError.message}(已保存 ${samples.length} 点)\n`);
    process.exit(3);
  }
  process.exit(0);
}

main().catch((err) => {
  console.log(JSON.stringify({ ok: false, code: 'ERROR', message: err.message }));
  process.exit(1);
});
