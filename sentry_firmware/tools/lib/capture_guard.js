/**
 * @file tools/lib/capture_guard.js
 * @brief 采集前闸门:兵种 ELF 身份校验、固件一致性校验、采集互斥锁
 *
 * 背景(2026-11-27 实测定性):
 *   EmberProbe 的 elf 固定指向 firmware/current.elf,而该文件会被任意兵种的
 *   build.ps1 -Current 覆盖(哨兵台架采集时它已被步兵默认构建换成
 *   PROFILE=infantry)。结果: RTT 控制块按错地址搜索失败、变量按错地址解析,
 *   采集工具还把每次探针失败重试 24 遍 —— 表现为"采集又慢又总出问题"。
 *
 * 规则:
 *   1. 采集前把 ELF 钉到 firmware/<robot>.elf,不依赖 current.elf;
 *   2. 校验 current.txt 记录的 SHA 与磁盘 firmware/<robot>.elf 一致;
 *   3. 用一次真实读变量核对 flashedSha256 == elfSha256,不一致默认拒绝采集
 *      (车上固件与磁盘 ELF 不同源时,地址可能已漂移);
 *   4. 同一时刻只允许一个采集进程(锁文件 build/.capture.lock)。
 *
 * @note 本文件属共享工具层;改它要同步 tools/README.md 与根 RM_DEVLOG.md §6。
 */
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const LOCK_FILE = '.capture.lock';

function sha256File(file) {
  return crypto
    .createHash('sha256')
    .update(fs.readFileSync(file))
    .digest('hex')
    .toUpperCase();
}

/** 读 firmware/current.txt(构建产物身份卡);缺失或半写时返回 null 字段 */
function readCurrentTxt(workspace) {
  const file = path.join(workspace, 'firmware', 'current.txt');
  if (!fs.existsSync(file)) return null;
  const text = fs.readFileSync(file, 'utf8');
  const pick = (key) => {
    const m = new RegExp('^' + key + '=(.*)$', 'm').exec(text);
    return m ? m[1].trim() : null;
  };
  return {
    path: file,
    profile: pick('PROFILE'),
    sha256: String(pick('SHA256') || '').toUpperCase(),
    source: pick('SOURCE'),
    updated: pick('UPDATED'),
  };
}

/** current.elf 当前属于哪个兵种(只作默认值,不作为可信身份) */
function currentProfile(workspace) {
  const cur = readCurrentTxt(workspace);
  return (cur && cur.profile) || null;
}

function elfPathFor(workspace, robot) {
  return path.join(workspace, 'firmware', robot + '.elf');
}

/**
 * 解析本兵种可用的 ELF。
 *
 * build.ps1 -Current 只给 sentry 额外复制 firmware/sentry.elf;infantry/heavy 只有
 * 共享的 firmware/current.elf。所以优先级:
 *   1) firmware/<robot>.elf 存在 → 用它(哨兵不会被步兵构建覆盖,最稳);
 *   2) 否则要求 current.txt 的 PROFILE == robot → 用 current.elf;
 *   3) 都不满足 → 拒绝并给出 build.ps1 命令。
 * current.txt 指向别的兵种只算 warning(说明构建链最后跑的是别的兵种),
 * 真正的数据闸门是后面的 flashedSha256 核对。
 */
function checkRobotElf(workspace, robot) {
  const warnings = [];
  const problems = [];
  const remedy = 'powershell -ExecutionPolicy Bypass -File build.ps1 ' + robot + ' -Current';
  const cur = readCurrentTxt(workspace);
  const namedPath = elfPathFor(workspace, robot);
  const currentPath = path.join(workspace, 'firmware', 'current.elf');

  let elfPath = null;
  let source = null;

  if (fs.existsSync(namedPath)) {
    elfPath = namedPath;
    source = 'named';
    if (cur && cur.profile && cur.profile !== robot) {
      warnings.push(
        `current.txt 指向 ${cur.profile}(最后一次构建不是 ${robot});改用 firmware/${robot}.elf`
      );
    }
  } else if (cur && cur.profile === robot && fs.existsSync(currentPath)) {
    elfPath = currentPath;
    source = 'current';
  } else {
    problems.push(
      cur && cur.profile
        ? `current.txt 指向 ${cur.profile},不是 ${robot},且没有 firmware/${robot}.elf`
        : `没有可用的 ELF(current.txt 缺失且无 firmware/${robot}.elf)`
    );
  }

  let elfSha = null;
  if (elfPath) {
    elfSha = sha256File(elfPath);
    if (cur && cur.profile === robot && cur.sha256 && cur.sha256 !== elfSha) {
      warnings.push('current.txt 的 SHA 与 ' + path.basename(elfPath) + ' 不一致(构建后被覆盖?)');
    }
  }

  return {
    ok: problems.length === 0 && Boolean(elfSha),
    elfPath,
    elfSha,
    source,
    current: cur,
    warnings,
    problems,
    remedy,
  };
}

/** 把 EmberProbe 的 elf 钉到指定文件;已是目标值时不做任何写入 */
async function pinElf(call, workspace, elfPath) {
  const desired = String(elfPath).replace(/\\/g, '/');
  const cfg = await call(workspace, 'config.get', {}, 10000);
  const current = String((cfg && cfg.elf) || '').replace(/\\/g, '/');
  if (current === desired) return { changed: false, elf: desired, from: current };
  await call(workspace, 'config.set', { values: { elf: desired } }, 20000);
  return { changed: true, elf: desired, from: current };
}

/** 从任意 reads/sample 回包里取固件一致性;mismatch 表示车上固件与 ELF 不同源 */
function firmwareConsistency(reply) {
  const c = (reply && reply.firmwareConsistency) || {};
  return {
    ok: c.status !== 'mismatch',
    status: c.status || 'unknown',
    elfSha256: c.elfSha256 || '',
    flashedSha256: c.flashedSha256 || '',
    flashedAt: c.flashedAt || null,
  };
}

/**
 * 只把"可自愈的会话/内存读失败"当瞬时错误重试。
 * 2026-11-27 实测: config.set 之后或空闲后的第一条 RPC 常报
 * OPENOCD_START_FAILED, 紧接着重试即可成功; 而探针目标不匹配、变量解析失败
 * 这类硬错误重试无意义。
 */
function isTransient(err) {
  const code = String((err && (err.code || (err.error && err.error.code))) || '');
  if (/PROBE_TARGET_MISMATCH|PROBE_IDENTITY|VARIABLE_|NO_SAMPLES|CAPTURE_GUARD/i.test(code)) return false;
  if (/OPENOCD_START_FAILED|BRIDGE_TIMEOUT|BRIDGE_UNAVAILABLE/i.test(code)) return true;
  const message = String((err && err.message) || '');
  return /failed to read memory|timed out|timeout|超时/i.test(message);
}

function lockPath(workspace) {
  return path.join(workspace, 'build', LOCK_FILE);
}

function pidAlive(pid) {
  if (!pid || pid === process.pid) return false;
  try {
    process.kill(pid, 0);
    return true;
  } catch (err) {
    return false;
  }
}

/** 采集互斥:同一工作区同时只允许一个采集进程(RTT 与采样抢探针 = 今天的故障) */
function acquireLock(workspace, tool) {
  const file = lockPath(workspace);
  fs.mkdirSync(path.dirname(file), { recursive: true });
  let owner = null;
  try {
    owner = JSON.parse(fs.readFileSync(file, 'utf8'));
  } catch (err) {
    owner = null;
  }
  if (owner && pidAlive(owner.pid)) return { ok: false, owner };
  fs.writeFileSync(
    file,
    JSON.stringify({ pid: process.pid, tool, at: new Date().toISOString() }),
    'utf8'
  );
  return { ok: true, file };
}

function releaseLock(workspace) {
  const file = lockPath(workspace);
  try {
    const owner = JSON.parse(fs.readFileSync(file, 'utf8'));
    if (owner && owner.pid === process.pid) fs.unlinkSync(file);
  } catch (err) {
    /* 锁文件不存在或不是本进程持有:不动作 */
  }
}

module.exports = {
  sha256File,
  readCurrentTxt,
  currentProfile,
  elfPathFor,
  checkRobotElf,
  pinElf,
  firmwareConsistency,
  isTransient,
  acquireLock,
  releaseLock,
};
