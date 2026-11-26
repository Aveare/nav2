# 日志格式串校验:禁止 LOGxxx 中出现浮点占位符(%f/%e/%g/%a)
#
# 背景(DEV-20261121-10):SEGGER RTT 的 printf 不支持浮点占位符。写了 %f 时该占位符
# 既不打印也不消费参数,随后的 %s 会把浮点值当 char* 解引用 —— 实车表现为 HardFault
# (BFAR=0x40240000 = heat_q 2.5625 的位模式),整车僵死并由看门狗复位,已连续复现四次。
#
# 用法(提交前执行):
#   powershell -ExecutionPolicy Bypass -File tools\check_log_format.ps1
# 退出码:0 = 通过;1 = 发现违规
#
# 允许的写法:
#   LOGINFO("[heat] q=%d/%d", (int)heat_q, (int)heat_limit);
#   char s[16]; Float2Str(s, v); LOGINFO("[x] v=%s", s);

$ErrorActionPreference = 'Stop'

$roots = @('application', 'modules', 'bsp')
$floatSpec = '%[-+ #0-9.]*[fFeEgGaA]'
$violations = @()

foreach ($root in $roots) {
    if (-not (Test-Path $root)) { continue }
    Get-ChildItem -Path $root -Recurse -Filter *.c | ForEach-Object {
        $path = $_.FullName
        $text = Get-Content -Raw -LiteralPath $path
        if ([string]::IsNullOrEmpty($text)) { return }
        # LOG / LOGINFO / LOGWARNING / LOGERROR 调用(允许跨行,贪婪到右括号+分号)
        $pattern = '\bLOG(?:INFO|WARNING|ERROR)?\s*\((?:[^;])*?\)\s*;'
        foreach ($m in [regex]::Matches($text, $pattern, 'Singleline')) {
            if ($m.Value -match $floatSpec) {
                $lineNo = ($text.Substring(0, $m.Index) -split "`n").Count
                $snippet = ($m.Value -replace '\s+', ' ')
                if ($snippet.Length -gt 120) { $snippet = $snippet.Substring(0, 120) + '...' }
                $violations += [pscustomobject]@{ File = $path; Line = $lineNo; Code = $snippet }
            }
        }
    }
}

if ($violations.Count -eq 0) {
    Write-Host '[OK] 未发现日志浮点占位符'
    exit 0
}

Write-Host "[FAIL] 发现 $($violations.Count) 处日志浮点占位符(会错位崩溃):"
foreach ($v in $violations) {
    Write-Host ("  {0}:{1}  {2}" -f $v.File, $v.Line, $v.Code)
}
Write-Host ''
Write-Host '改法:浮点用 Float2Str(buf, v) + %s,或 (int)(v) + %d'
exit 1
