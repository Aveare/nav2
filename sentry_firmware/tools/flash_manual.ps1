# 手动烧录脚本(绕过 EmberProbe 的两个已知问题)
#
# 为什么需要这个脚本:
#   1. EmberProbe 的 adapterSpeedKhz 默认 0 时会请求 10 MHz 烧录时钟,本板实测会 USB 管道失步
#      (CMSIS-DAP command mismatch, Sent 0x12 received 0x6),Programming 直接失败。
#      .vscode/settings.json 已锁 1000 kHz,但换机器/重装扩展后会丢。
#   2. objcopy 生成的 hex 不记录段间空洞,而 bin 会把空洞填 0x00。烧 hex 时空洞保持擦除态 0xFF,
#      verify 会报 "checksum mismatch -> Verify Failed"(实际固件是好的,只差 8 字节填充)。
#      烧 bin 后片上内容与 bin 逐字节一致,verify 能真正通过。
#
# 用法:
#   powershell -ExecutionPolicy Bypass -File .\tools\flash_manual.ps1
#
# 前置:先编译出 build/basic_framework.bin;烧录时只接控制板供电,电机电源断开更稳。

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$openocd = "C:\Toolchain\xpack-openocd-0.12.0-7\bin\openocd.exe"
$bin = Join-Path $root "build\basic_framework.bin"
$binPosix = $bin -replace '\\', '/'

if (-not (Test-Path $bin)) {
    Write-Host "[错误] 找不到 $bin,请先编译" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $openocd)) {
    Write-Host "[错误] 找不到 openocd: $openocd" -ForegroundColor Red
    Write-Host "        请修改本脚本顶部的 `$openocd 路径" -ForegroundColor Yellow
    exit 1
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

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "烧录完成。若上方出现 'Verified OK' 即固件完整。" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "烧录失败(退出码 $LASTEXITCODE)。排查顺序:" -ForegroundColor Red
    Write-Host "  1. 拔插探针 USB(失步后必须重新枚举),再跑一次本脚本" -ForegroundColor Yellow
    Write-Host "  2. 确认目标板由电池供电、电源开关已开、3.3V 轨正常" -ForegroundColor Yellow
    Write-Host "  3. 断开电机电源,只留控制板" -ForegroundColor Yellow
    Write-Host "  4. 换 USB 线/换主板后置 USB 口" -ForegroundColor Yellow
}
