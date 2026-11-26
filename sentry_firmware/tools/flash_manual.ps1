# 手动烧录脚本(绕过 EmberProbe 的两个已知问题)
#
# 为什么需要这个脚本:
#   1. (历史)早期 EmberProbe 请求 10 MHz 时会 CMSIS-DAP 失步;根因已解决(2026-11-23 确认),
#      现在 10 MHz 正常。本脚本仍固定 1000 kHz:烧录保守、成功率优先。
#   2. objcopy 生成的 hex 不记录段间空洞,而 bin 会把空洞填 0x00。烧 hex 时空洞保持擦除态 0xFF,
#      verify 会报 "checksum mismatch -> Verify Failed"(实际固件是好的,只差 8 字节填充)。
#      烧 bin 后片上内容与 bin 逐字节一致,verify 能真正通过。
#
# 用法:
#   powershell -ExecutionPolicy Bypass -File .\tools\flash_manual.ps1
#   powershell -ExecutionPolicy Bypass -File .\tools\flash_manual.ps1 -Profile sentry
#   powershell -ExecutionPolicy Bypass -File .\tools\flash_manual.ps1 -DryRun   # 只验证解析,不碰硬件
#
# 兵种:显式 -Profile 优先;否则读 firmware/current.txt(build.ps1 -Current 写入);
#       都没有时按 infantry。避免"编译了 sentry、却烧了步兵 bin"。
# 前置:先编译对应兵种(build.ps1);烧录时只接控制板供电,电机电源断开更稳。
param(
    [ValidateSet('infantry', 'sentry', 'heavy')]
    [string]$Profile,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

if (-not $Profile) {
    $infoPath = Join-Path $root "firmware\current.txt"
    if (Test-Path -LiteralPath $infoPath) {
        $hit = Select-String -LiteralPath $infoPath -Pattern '^PROFILE=(infantry|sentry|heavy)$' | Select-Object -First 1
        if ($hit) { $Profile = $hit.Matches[0].Groups[1].Value }
    }
}
if (-not $Profile) { $Profile = 'infantry' }

$buildDirs = @{
    infantry = 'build'
    sentry   = 'build_sentry'
    heavy    = 'build_heavy'
}
$bin = Join-Path $root (Join-Path $buildDirs[$Profile] "basic_framework.bin")
$binPosix = $bin -replace '\\', '/'

# openocd: PATH 优先(settings.json 已改走 PATH),退回本机 xpack 安装。
$openocd = $null
$openocdCmd = Get-Command openocd -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if ($openocdCmd) { $openocd = $openocdCmd.Source }
if (-not $openocd) { $openocd = "C:\Toolchain\xpack-openocd-0.12.0-7\bin\openocd.exe" }

if (-not (Test-Path $bin)) {
    Write-Host "[错误] 找不到 $bin,请先编译 $Profile" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $openocd)) {
    Write-Host "[错误] 找不到 openocd: $openocd" -ForegroundColor Red
    Write-Host "        请修改本脚本顶部的 `$openocd 路径" -ForegroundColor Yellow
    exit 1
}

if ($DryRun) {
    Write-Host "[dry-run] profile : $Profile"
    Write-Host "[dry-run] bin     : $binPosix"
    Write-Host "[dry-run] openocd : $openocd"
    Write-Host "[dry-run] 未接触硬件,只验证解析结果" -ForegroundColor Cyan
    exit 0
}

Write-Host "[1/3] 连接探针并复位目标..." -ForegroundColor Cyan
Write-Host "[2/3] 烧录 $binPosix (1000 kHz)" -ForegroundColor Cyan
Write-Host "[3/3] 校验并复位运行" -ForegroundColor Cyan

& $openocd `
    -f interface/cmsis-dap.cfg `
    -f target/stm32f4x.cfg `
    -c "transport select swd" `
    -c "adapter speed 1000" `
    -c "init" `
    -c "reset halt" `
    -c "program `"$binPosix`" 0x08000000 verify" `
    -c "reset run" `
    -c "shutdown"

$flashExit = $LASTEXITCODE
if ($flashExit -eq 0) {
    Write-Host ""
    Write-Host "烧录完成。若上方出现 'Verified OK' 即固件完整。" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "烧录失败(退出码 $flashExit)。排查顺序:" -ForegroundColor Red
    Write-Host "  1. 拔插探针 USB(失步后必须重新枚举),再跑一次本脚本" -ForegroundColor Yellow
    Write-Host "  2. 确认目标板由电池供电、电源开关已开、3.3V 轨正常" -ForegroundColor Yellow
    Write-Host "  3. 断开电机电源,只留控制板" -ForegroundColor Yellow
    Write-Host "  4. 换 USB 线/换主板后置 USB 口" -ForegroundColor Yellow
    exit $flashExit
}
