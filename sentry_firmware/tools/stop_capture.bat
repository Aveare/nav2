@echo off
REM ---------------------------------------------------------------
REM  Stop the RTT capture process (started by capture_rtt.bat or
REM  "python tools\rtt_log.py --loop").
REM  Also kills a leftover openocd.exe, otherwise the probe stays
REM  occupied and the next run reports "unable to find a matching
REM  CMSIS-DAP device".
REM ---------------------------------------------------------------
setlocal
cd /d "%~dp0.."

set "PID="
for /f "tokens=5" %%P in ('netstat -ano ^| findstr ":4456" ^| findstr "LISTENING"') do set "PID=%%P"

if defined PID (
    echo [stop] rtt_log.py PID=%PID%
    taskkill /F /PID %PID% >nul 2>&1
) else (
    echo [stop] no capture process on port 4456
)

tasklist /FI "IMAGENAME eq openocd.exe" 2>nul | findstr /I "openocd.exe" >nul
if not errorlevel 1 (
    echo [stop] leftover openocd.exe found, killing it to release the probe
    taskkill /F /IM openocd.exe >nul 2>&1
)

echo [stop] done
