<#
Read-only baseline audit for rm-electrical-dev.

Compares a working project against the skill's pinned YueLu snapshot via
assets/yuelu-basic-framework.manifest.json (per-file SHA-256), and reports:
  - Unchanged : byte-identical to the pinned commit
  - Changed   : content differs (first-party .c/.h also get a heuristic
                symbol delta: functions / #defines / typedefs added or removed)
  - Missing   : present in baseline, absent in the project
  - Added     : present in the project, absent in baseline

Self-verifies the snapshot against the manifest first; throws if the snapshot
itself is corrupted (all diff conclusions would be unreliable).

The script never modifies the project or the snapshot. It writes a file only
when -ReportPath is given. No build, no flash, no execution of project code.

!!! 孪生脚本: audit_baseline.py 与本文件逻辑必须保持一致, 修改任一端需同步另一端 !!!

Author: rm-electrical-dev (JIT-RM). Keep this file ASCII-only.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ProjectRoot,
    [string]$SkillRoot = '',
    [switch]$IgnoreEol,
    [string]$ReportPath = '',
    [ValidateSet('Text', 'Json')][string]$OutputFormat = 'Text'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrEmpty($SkillRoot)) {
    $SkillRoot = Split-Path -Parent $PSScriptRoot
}
$project = (Get-Item -LiteralPath $ProjectRoot -Force).FullName.TrimEnd('\', '/')
if (-not (Get-Item -LiteralPath $project).PSIsContainer) { throw 'ProjectRoot must be a directory.' }

$manifestPath = Join-Path $SkillRoot 'assets\yuelu-basic-framework.manifest.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "Manifest not found: $manifestPath" }
$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$snapshotRoot = Join-Path $SkillRoot 'assets\yuelu-basic-framework'

$textExtensions = @('.c', '.h', '.md', '.s', '.ld', '.ioc', '.txt', '.json', '.yml', '.yaml', '.clangd', '.cfg', '.gitignore')
$funcKeywords = @('if', 'for', 'while', 'switch', 'else', 'do', 'return', 'sizeof', 'struct', 'union', 'enum', 'typedef', '_')
$handlePattern = [regex]'\b(hcan\d+|htim\d+|hadc\d+|husart\d+|huart\d+|hspi\d+|hi2c\d+|hdma\d+)\b'
$whileOnePattern = [regex]'while\s*\(\s*1\s*\)'
$handleScopeDirs = @('application/', 'Src/', 'Inc/')

function Get-PatternTokens([string]$Text) {
    $toks = @()
    foreach ($m in $handlePattern.Matches($Text)) { $toks += $m.Value }
    if ($whileOnePattern.IsMatch($Text)) { $toks += 'while(1)' }
    return @($toks | Sort-Object -Unique)
}

function Get-NewRedFlags([string]$Rel, [string]$BaseText, [string]$ProjText) {
    $base = Get-PatternTokens $BaseText
    $flags = @()
    foreach ($tok in (Get-PatternTokens $ProjText)) {
        if ($base -contains $tok) { continue }
        $inHandleScope = ($handleScopeDirs | Where-Object { $Rel.StartsWith($_) }).Count -gt 0
        if ($tok -eq 'while(1)') {
            if (-not $Rel.StartsWith('tests/')) { $flags += "$Rel`: $tok" }  # tests循环while(1)合法
        }
        elseif ($inHandleScope) { $flags += "$Rel`: $tok" }
    }
    return @($flags)
}

function Read-FileText([string]$Path) {
    return [System.IO.File]::ReadAllText($Path)
}

function Get-Sha256Hex([byte[]]$Bytes) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([System.BitConverter]::ToString($sha.ComputeHash($Bytes)) -replace '-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

function Get-FileHash256([string]$Path, [bool]$Normalize) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($Normalize) {
        $out = New-Object System.Collections.Generic.List[byte]
        $n = $bytes.Length
        for ($i = 0; $i -lt $n; $i++) {
            $b = $bytes[$i]
            if ($b -eq 13) { continue }  # drop CR bytes: CRLF -> LF normalization
            $out.Add($b)
        }
        $bytes = $out.ToArray()
    }
    return Get-Sha256Hex $bytes
}

function Get-CodeSymbols([string]$Path) {
    $text = [System.IO.File]::ReadAllText($Path)
    $funcs = @()
    $defines = @()
    $types = @()

    foreach ($m in [regex]::Matches($text, '(?m)^[ \t]*#define[ \t]+([A-Za-z_][A-Za-z0-9_]*)')) { $defines += $m.Groups[1].Value }

    # Function definition heuristic: "type... name ( params ) {" with the brace
    # possibly on the next line; params must not contain ; { }. Call statements
    # end with ';' so they are excluded. Keywords filtered afterwards.
    $pattern = '(?ms)^[ \t]*(?:(?:static|inline|const|extern|[A-Za-z_][A-Za-z0-9_]*)[ \t\*]+)+([A-Za-z_][A-Za-z0-9_]*)[ \t]*\([^;{}]*\)[ \t\r\n]*\{'
    foreach ($m in [regex]::Matches($text, $pattern)) {
        $name = $m.Groups[1].Value
        if ($funcKeywords -notcontains $name) { $funcs += $name }
    }

    foreach ($m in [regex]::Matches($text, '(?s)\}\s*([A-Za-z_][A-Za-z0-9_]*)\s*;')) { $types += $m.Groups[1].Value }
    foreach ($m in [regex]::Matches($text, '(?m)^[ \t]*typedef[ \t]+[^;{}()]*?([A-Za-z_][A-Za-z0-9_]*)[ \t]*;')) { $types += $m.Groups[1].Value }

    return @{
        funcs   = @($funcs | Sort-Object -Unique)
        defines = @($defines | Sort-Object -Unique)
        types   = @($types | Sort-Object -Unique)
    }
}

$manifestFiles = @($manifest.files)

# 快照自检:基线必须与manifest一致,否则后续所有比对结论失真(P0盲区修复)
$snapBad = @()
foreach ($entry in $manifestFiles) {
    $sp = Join-Path $snapshotRoot (([string]$entry.path) -replace '/', '\')
    if (-not (Test-Path -LiteralPath $sp -PathType Leaf)) { $snapBad += "MISSING $($entry.path)"; continue }
    if ((Get-FileHash256 $sp $false) -ne [string]$entry.sha256) { $snapBad += "HASH $($entry.path)" }
}
if ($snapBad.Count -gt 0) {
    $snapBad | ForEach-Object { Write-Error "[snapshot] $_" }
    throw "Snapshot corrupted ($($snapBad.Count) files) - baseline audit unreliable."
}

$changed = New-Object System.Collections.Generic.List[object]
$missing = New-Object System.Collections.Generic.List[string]
$unchangedCount = 0

foreach ($entry in $manifestFiles) {
    $rel = [string]$entry.path
    $projFile = Join-Path $project ($rel -replace '/', '\')
    if (-not (Test-Path -LiteralPath $projFile -PathType Leaf)) { $null = $missing.Add($rel); continue }

    $ext = [System.IO.Path]::GetExtension($rel).ToLowerInvariant()
    $normalize = ($IgnoreEol -and ($textExtensions -contains $ext))
    $hash = Get-FileHash256 $projFile $normalize
    if ($hash -eq [string]$entry.sha256) { $unchangedCount++; continue }

    $item = [ordered]@{ path = $rel; baseline_sha256 = [string]$entry.sha256; project_sha256 = $hash }
    $snapFile = Join-Path $snapshotRoot ($rel -replace '/', '\')
    if (($ext -eq '.c' -or $ext -eq '.h') -and (Test-Path -LiteralPath $projFile -PathType Leaf)) {
        $baseText = ''
        if (Test-Path -LiteralPath $snapFile -PathType Leaf) {
            $baseText = Read-FileText $snapFile
            $base = Get-CodeSymbols $snapFile
            $proj = Get-CodeSymbols $projFile
            $addedF = @($proj.funcs | Where-Object { $base.funcs -notcontains $_ })
            $removedF = @($base.funcs | Where-Object { $proj.funcs -notcontains $_ })
            $addedD = @($proj.defines | Where-Object { $base.defines -notcontains $_ })
            $removedD = @($base.defines | Where-Object { $proj.defines -notcontains $_ })
            $addedT = @($proj.types | Where-Object { $base.types -notcontains $_ })
            $removedT = @($base.types | Where-Object { $proj.types -notcontains $_ })
            if (($addedF.Count + $removedF.Count + $addedD.Count + $removedD.Count + $addedT.Count + $removedT.Count) -gt 0) {
                $item.symbol_delta = [ordered]@{
                    added_functions   = $addedF
                    removed_functions = $removedF
                    added_defines     = $addedD
                    removed_defines   = $removedD
                    added_types       = @($addedT | Select-Object -First 20)
                    removed_types     = @($removedT | Select-Object -First 20)
                }
            }
        }
        $red = Get-NewRedFlags $rel $baseText (Read-FileText $projFile)
        if ($red.Count -gt 0) { $item.red_flags = $red }
    }
    $null = $changed.Add([pscustomobject]$item)
}

# Added-file scan restricted to first-party directories + root config patterns.
$manifestSet = @{}
foreach ($e in $manifestFiles) { $manifestSet[[string]$e.path] = $true }
$added = New-Object System.Collections.Generic.List[string]
$skipDirs = @('.git', 'build', 'CMakeFiles', '.cache', '__pycache__', 'node_modules')
foreach ($d in @('application', 'bsp', 'config', 'modules', 'Src', 'Inc', 'tests')) {
    $root = Join-Path $project $d
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { continue }
    $stack = New-Object System.Collections.Stack
    $null = $stack.Push($root)
    while ($stack.Count -gt 0) {
        $cur = [string]$stack.Pop()
        foreach ($fsi in (Get-ChildItem -LiteralPath $cur -Force)) {
            if ($fsi.PSIsContainer) {
                if ($skipDirs -contains $fsi.Name) { continue }
                if ($fsi.Name -like 'cmake-build-*') { continue }
                $null = $stack.Push($fsi.FullName)
            }
            else {
                $rel = $fsi.FullName.Substring($project.Length + 1) -replace '\\', '/'
                if (-not $manifestSet.ContainsKey($rel)) { $null = $added.Add($rel) }
            }
        }
    }
}
$rootPatterns = @('*.md', 'LICENSE*', 'Makefile*', 'CMakeLists.txt', '*.ioc', '*.ld', '*.s', '*.cfg', '*.svd', '*.jflash', '*.ps1', '.clangd', '.mxproject', '.gitignore')
foreach ($rf in (Get-ChildItem -LiteralPath $project -File -Force)) {
    foreach ($pat in $rootPatterns) {
        if ($rf.Name -like $pat) {
            if (-not $manifestSet.ContainsKey($rf.Name)) { $null = $added.Add($rf.Name) }
            break
        }
    }
}
$addedFinal = @($added | Sort-Object -Unique)

$redFlags = New-Object System.Collections.Generic.List[string]
foreach ($c in $changed) {
    if ($null -ne $c.PSObject.Properties['red_flags']) { foreach ($f in $c.red_flags) { $null = $redFlags.Add($f) } }
}
foreach ($a in $addedFinal) {
    if ($a.EndsWith('.c') -or $a.EndsWith('.h')) {
        $ap = Join-Path $project ($a -replace '/', '\')
        if (Test-Path -LiteralPath $ap -PathType Leaf) {
            foreach ($f in (Get-NewRedFlags $a '' (Read-FileText $ap))) { $null = $redFlags.Add($f) }
        }
    }
}

if ($OutputFormat -eq 'Json') {
    $result = [ordered]@{
        project               = $project
        baseline_repository   = $manifest.upstream_repository
        baseline_commit       = $manifest.source_revision
        manifest_collected_at = $manifest.collected_at
        eol_normalized        = [bool]$IgnoreEol
        unchanged_count       = $unchangedCount
        changed               = $changed
        missing               = $missing
        added                 = $addedFinal
    }
    $json = $result | ConvertTo-Json -Depth 6
    if ($ReportPath) { [System.IO.File]::WriteAllText($ReportPath, $json, (New-Object System.Text.UTF8Encoding($false))) }
    Write-Output $json
    return
}

$lines = New-Object System.Collections.Generic.List[string]
$null = $lines.Add('=== Baseline Audit (read-only) ===')
$null = $lines.Add("Project  : $project")
$null = $lines.Add("Baseline : $($manifest.upstream_repository) @ $($manifest.source_revision)")
$null = $lines.Add("Collected: $($manifest.collected_at)")
if ($IgnoreEol) { $null = $lines.Add('Mode     : EOL-normalized (CRLF -> LF) for text files') } else { $null = $lines.Add('Mode     : raw bytes') }
$null = $lines.Add('')
$null = $lines.Add("Unchanged: $unchangedCount / $($manifestFiles.Count)")
$null = $lines.Add("Changed  : $($changed.Count)")
$null = $lines.Add("Missing  : $($missing.Count)")
$null = $lines.Add("Added    : $($addedFinal.Count)")
$null = $lines.Add("RedFlags : $($redFlags.Count)")
if ($redFlags.Count -gt 0) {
    $null = $lines.Add('')
    $null = $lines.Add('--- Red flags (new vs baseline; see references/ai-constraints.md) ---')
    foreach ($f in $redFlags) { $null = $lines.Add("[!] $f") }
}

if ($changed.Count -gt 0) {
    $null = $lines.Add('')
    $null = $lines.Add('--- Changed files (+ heuristic symbol delta) ---')
    foreach ($c in $changed) {
        $null = $lines.Add("[M] $($c.path)")
        if ($null -ne $c.PSObject.Properties['symbol_delta']) {
            $d = $c.symbol_delta
            if ($d.added_functions.Count -gt 0) { $null = $lines.Add("    + funcs   : $($d.added_functions -join ', ')") }
            if ($d.removed_functions.Count -gt 0) { $null = $lines.Add("    - funcs   : $($d.removed_functions -join ', ')") }
            if ($d.added_defines.Count -gt 0) { $null = $lines.Add("    + defines : $($d.added_defines -join ', ')") }
            if ($d.removed_defines.Count -gt 0) { $null = $lines.Add("    - defines : $($d.removed_defines -join ', ')") }
            if ($d.added_types.Count -gt 0) { $null = $lines.Add("    + types   : $($d.added_types -join ', ')") }
            if ($d.removed_types.Count -gt 0) { $null = $lines.Add("    - types   : $($d.removed_types -join ', ')") }
        }
    }
}
if ($addedFinal.Count -gt 0) {
    $null = $lines.Add('')
    $null = $lines.Add('--- Added files (not in pinned baseline; classify per baseline-diff.md) ---')
    foreach ($a in $addedFinal) { $null = $lines.Add("[A] $a") }
}
if ($missing.Count -gt 0) {
    $null = $lines.Add('')
    $null = $lines.Add('--- Missing files (in baseline, absent in project) ---')
    foreach ($m in $missing) { $null = $lines.Add("[X] $m") }
}
$null = $lines.Add('')
$null = $lines.Add('Note: symbol delta is heuristic; confirm with git diff --no-index before acting.')
$null = $lines.Add('Note: Added is not automatically a violation; classify per references/baseline-diff.md.')
$output = ($lines -join [Environment]::NewLine)
if ($ReportPath) { [System.IO.File]::WriteAllText($ReportPath, $output + [Environment]::NewLine, (New-Object System.Text.UTF8Encoding($false))) }
Write-Output $output
