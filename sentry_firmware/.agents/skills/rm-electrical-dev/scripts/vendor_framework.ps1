<#
Copies a verified, pinned YueLu GitHub checkout without running source scripts.
Author: 金陵科技学院CR战队  Colin
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceRoot,
    [Parameter(Mandatory = $true)][string]$SkillRoot,
    [string]$SourceRevision = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-CheckedDirectory([string]$Directory) {
    $item = Get-Item -LiteralPath $Directory -Force
    if (!$item.PSIsContainer -or $item.PSProvider.Name -ne 'FileSystem') {
        throw "Expected a filesystem directory: $Directory"
    }
    $full = $item.FullName.TrimEnd([char[]]'\/')
    if ($full -eq [IO.Path]::GetPathRoot($item.FullName).TrimEnd([char[]]'\/')) {
        throw "A volume root is not a valid source or skill directory: $Directory"
    }
    $cursor = $item
    while ($null -ne $cursor) {
        if (($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse-point directory is not supported: $($cursor.FullName)"
        }
        $cursor = $cursor.Parent
    }
    return $full
}

function Assert-NoReparsePoint([IO.FileSystemInfo]$Item) {
    if (($Item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Reparse-point entry is not supported: $($Item.FullName)"
    }
}

function Get-Sha256([string]$File) {
    return (Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash.ToLowerInvariant()
}

$source = Resolve-CheckedDirectory $SourceRoot
$skill = Resolve-CheckedDirectory $SkillRoot
$comparison = [StringComparison]::OrdinalIgnoreCase
if ($source.Equals($skill, $comparison) -or
    $source.StartsWith($skill + '\', $comparison) -or
    $skill.StartsWith($source + '\', $comparison)) {
    throw 'Source and skill directories must be disjoint.'
}
$skillFile = Join-Path $skill 'SKILL.md'
if (!(Test-Path -LiteralPath $skillFile -PathType Leaf)) { throw 'SKILL.md is missing.' }
Assert-NoReparsePoint (Get-Item -LiteralPath $skillFile -Force)
$skillText = Get-Content -LiteralPath $skillFile -Raw -Encoding UTF8
$skillMetadata = [ordered]@{}
foreach ($key in @('version', 'version_date', 'updated_at', 'author', 'upstream_repository', 'upstream_commit')) {
    $pattern = '(?m)^  ' + [regex]::Escape($key) + ': "([^"\r\n]+)"\s*$'
    $match = [regex]::Match($skillText, $pattern)
    if (!$match.Success) { throw "Missing quoted metadata field: $key" }
    $skillMetadata[$key] = $match.Groups[1].Value
}

function Get-GitOutput([string[]]$GitArguments) {
    $result = @(& git -C $source -c core.quotepath=false @GitArguments)
    if ($LASTEXITCODE -ne 0) { throw "Git verification failed: $($GitArguments -join ' ')" }
    return $result
}

$gitRoot = Resolve-CheckedDirectory ((Get-GitOutput @('rev-parse', '--show-toplevel')) -join '')
if (!$gitRoot.Equals($source, $comparison)) { throw 'SourceRoot must be the Git working-tree root.' }
$origin = ((Get-GitOutput @('remote', 'get-url', 'origin')) -join '').TrimEnd('/')
$expectedOrigin = $skillMetadata.upstream_repository.TrimEnd('/')
if ($origin -ne $expectedOrigin -and $origin -ne ($expectedOrigin + '.git')) {
    throw 'Git origin does not match the declared upstream GitHub repository.'
}
$head = ((Get-GitOutput @('rev-parse', 'HEAD')) -join '')
if ($head -notmatch '^[0-9a-f]{40}$' -or $head -ne $skillMetadata.upstream_commit) {
    throw 'Git HEAD does not match the pinned metadata commit.'
}
if ($SourceRevision -and $SourceRevision -ne $head) { throw 'Requested revision differs from Git HEAD.' }
$SourceRevision = $head
if (@(Get-GitOutput @('status', '--porcelain', '--untracked-files=all')).Count -ne 0) {
    throw 'The source working tree must be clean, including untracked source files.'
}
$branch = ((Get-GitOutput @('branch', '--show-current')) -join '')
$commitDate = ((Get-GitOutput @('show', '-s', '--format=%cI', 'HEAD')) -join '')
$trackedBlobs = @{}
foreach ($line in (Get-GitOutput @('ls-files', '--stage'))) {
    if ($line -notmatch '^(100644|100755) ([0-9a-f]{40}) 0\t(.+)$') {
        throw "Unsupported tracked entry (for example symlink or submodule): $line"
    }
    $trackedBlobs[$Matches[3]] = $Matches[2]
}

$assets = Join-Path $skill 'assets'
if (Test-Path -LiteralPath $assets) { $null = Resolve-CheckedDirectory $assets }
$target = Join-Path $assets 'yuelu-basic-framework'
$manifestPath = Join-Path $assets 'yuelu-basic-framework.manifest.json'
if ((Test-Path -LiteralPath $target) -or (Test-Path -LiteralPath $manifestPath)) {
    throw 'Snapshot or manifest already exists; use a new skill destination.'
}

$requiredDirectories = @('application', 'bsp', 'modules', 'Src', 'Inc', 'Drivers', 'Middlewares', '.Doc', '.assets')
$optionalDirectories = @('.vscode', '.github', 'cmake', 'tests')
$requiredFiles = @(
    'README.md', 'LICENSE', 'CMakeLists.txt', 'Makefile', 'basic_framework.ioc',
    'startup_stm32f407xx.s', 'STM32F407IGHx_FLASH.ld',
    '.Doc/架构介绍与开发指南.md', '.Doc/必须做&禁止做.md', '.Doc/VSCode+Ozone使用方法.md',
    'application/robot.c', 'application/robot_def.h', 'application/robot_task.h', 'Src/main.c',
    'modules/motor/DJImotor/dji_motor.h', 'modules/message_center/message_center.h', 'bsp/can/bsp_can.h',
    'Middlewares/ST/ARM/DSP/Lib/libCMSISDSP.a',
    'Middlewares/Third_Party/SEGGER/RTT/SEGGER_RTT_ASM_ARMv7M.s'
)
foreach ($relative in $requiredFiles) {
    $file = Join-Path $source $relative
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Required source file is missing: $relative" }
    Assert-NoReparsePoint (Get-Item -LiteralPath $file -Force)
}
foreach ($relative in $requiredDirectories) {
    $directory = Join-Path $source $relative
    if (!(Test-Path -LiteralPath $directory -PathType Container)) { throw "Required source directory is missing: $relative" }
}

$excludedDirectories = @('.git', 'build', 'CMakeFiles', '.cache', '__pycache__', 'node_modules')
$excludedExtensions = @('.o', '.obj', '.d', '.elf', '.hex', '.bin', '.map', '.log', '.su', '.exe', '.dll', '.pyc')
$rootPatterns = @('README*.md', 'LICENSE*', 'NOTICE*', 'COPYING*', 'Makefile*', 'CMakeLists.txt',
    '*.cmake', '*.ioc', '*.s', '*.ld', '*.cfg', '*.svd', '*.jflash', '*.ps1', '.gitignore', '.gitmodules', '.mxproject', '.clangd')
$files = [Collections.Generic.List[IO.FileInfo]]::new()
$pending = [Collections.Generic.Stack[string]]::new()
foreach ($name in ($requiredDirectories + $optionalDirectories)) {
    $directory = Join-Path $source $name
    if (Test-Path -LiteralPath $directory -PathType Container) { $pending.Push($directory) }
}
while ($pending.Count -gt 0) {
    $directory = $pending.Pop()
    Assert-NoReparsePoint (Get-Item -LiteralPath $directory -Force)
    foreach ($item in Get-ChildItem -LiteralPath $directory -Force) {
        Assert-NoReparsePoint $item
        if ($item.PSIsContainer) {
            if ($item.Name -notin $excludedDirectories -and $item.Name -notlike 'cmake-build-*') {
                $pending.Push($item.FullName)
            }
        } elseif ($item.Extension.ToLowerInvariant() -notin $excludedExtensions -and
            $item.Name -notin @('CMakeCache.txt', 'compile_commands.json') -and
            $item.Name -notlike '.cortex-debug.*.state.json') {
            $files.Add($item)
        }
    }
}
foreach ($item in Get-ChildItem -LiteralPath $source -Force -File) {
    Assert-NoReparsePoint $item
    foreach ($pattern in $rootPatterns) {
        if ($item.Name -like $pattern) { $files.Add($item); break }
    }
}

# Check the selected set before creating a destination or copying data.
$selected = @($files | ForEach-Object { $_.FullName.Substring($source.Length + 1).Replace('\', '/') })
foreach ($relative in $requiredFiles) {
    if ($relative -notin $selected) { throw "Required file excluded by selection: $relative" }
}
$orderedFiles = @($files | Sort-Object FullName)
$rawBlobIds = @($orderedFiles | ForEach-Object { $_.FullName } | & git -C $source hash-object --no-filters --stdin-paths)
if ($LASTEXITCODE -ne 0 -or $rawBlobIds.Count -ne $orderedFiles.Count) { throw 'Git blob comparison failed.' }
for ($index = 0; $index -lt $orderedFiles.Count; $index++) {
    $relative = $orderedFiles[$index].FullName.Substring($source.Length + 1).Replace('\', '/')
    if (!$trackedBlobs.ContainsKey($relative) -or $rawBlobIds[$index] -ne $trackedBlobs[$relative]) {
        throw "Checkout bytes differ from pinned Git blob: $relative. Check line-ending conversion or local changes."
    }
}
$null = New-Item -ItemType Directory -Path $target
$entries = [Collections.Generic.List[object]]::new()
foreach ($file in $orderedFiles) {
    $relative = $file.FullName.Substring($source.Length + 1)
    $destination = Join-Path $target $relative
    $null = New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($destination)) -Force
    $before = Get-Sha256 $file.FullName
    Copy-Item -LiteralPath $file.FullName -Destination $destination
    $after = Get-Sha256 $file.FullName
    $copied = Get-Sha256 $destination
    if ($before -ne $after -or $before -ne $copied) {
        throw "Source changed or copy mismatch: $relative. Partial snapshot remains for inspection."
    }
    $entries.Add([ordered]@{ path = $relative.Replace('\', '/'); size = (Get-Item -LiteralPath $destination).Length; sha256 = $copied })
}

# A final source comparison detects edits made during the whole collection.
foreach ($entry in $entries) {
    if ((Get-Sha256 (Join-Path $source $entry.path)) -ne $entry.sha256 -or
        (Get-Sha256 (Join-Path $target $entry.path)) -ne $entry.sha256) {
        throw "Final hash mismatch: $($entry.path). Manifest was not written."
    }
}
if (((Get-GitOutput @('rev-parse', 'HEAD')) -join '') -ne $SourceRevision -or
    @(Get-GitOutput @('status', '--porcelain', '--untracked-files=all')).Count -ne 0) {
    throw 'Git source changed during collection; manifest was not written.'
}
$manifest = [ordered]@{
    schema = 1
    skill_name = 'rm-electrical-dev'
    skill_metadata = $skillMetadata
    collected_at = [DateTimeOffset]::Now.ToString('o')
    source_kind = 'github_commit'
    source_directory_name = [IO.Path]::GetFileName($source)
    upstream_repository = $skillMetadata.upstream_repository
    source_origin = $origin
    source_branch = $branch
    source_commit_date = $commitDate
    source_revision = $SourceRevision
    source_revision_verified = $true
    source_working_tree_clean = $true
    selected_git_blob_bytes_verified = $true
    third_party_license_audit = 'not_performed'
    firmware_build_validation = 'not_performed'
    required_files = $requiredFiles
    excluded_directories = $excludedDirectories + @('cmake-build-*')
    excluded_extensions = $excludedExtensions
    license_files = @($entries | Where-Object { $_.path -match '(?i)(^|/)(license[^/]*|copying[^/]*|notice[^/]*)$' } | ForEach-Object { $_.path })
    file_count = $entries.Count
    files = @($entries.ToArray())
}
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))
Write-Output "Copied and hash-verified $($entries.Count) files. Manifest: $manifestPath"
