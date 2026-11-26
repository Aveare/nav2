@echo off
REM ---------------------------------------------------------------
REM  RTT log capture, keep-alive mode.
REM  Double-click to start; or put a shortcut in shell:startup
REM  for auto-start at logon (see .Doc/guide/RTT日志与调试通道.md §8).
REM  Console output is appended to log\_console.log.
REM  Requires: probe idle (no Ozone / EmberProbe / RTT Viewer).
REM ---------------------------------------------------------------
setlocal
cd /d "%~dp0.."

set "PY="
if exist "%LOCALAPPDATA%\Programs\Python\Python313\python.exe" set "PY=%LOCALAPPDATA%\Programs\Python\Python313\python.exe"
if not defined PY if exist "C:\Users\ZhuanZ\.workbuddy\binaries\python\versions\3.13.12\python.exe" set "PY=C:\Users\ZhuanZ\.workbuddy\binaries\python\versions\3.13.12\python.exe"
if not defined PY set "PY=python"

if not exist "log" mkdir "log"

echo [%DATE% %TIME%] start %PY% >> "log\_console.log"
"%PY%" "tools\rtt_log.py" --loop >> "log\_console.log" 2>&1
echo [%DATE% %TIME%] exit %ERRORLEVEL% >> "log\_console.log"
