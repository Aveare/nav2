# One-command profile build: config + compile, then print the ELF path.
# Usage: .\build.ps1 sentry            (or infantry / heavy; default infantry)
#        .\build.ps1 sentry -Current   (also create firmware/sentry.elf and current.elf)
# Flashing is intentionally NOT automated; flash the printed ELF manually.
#
# cmake / ninja / arm-none-eabi-gcc are resolved automatically: PATH first, then the ST bundle
# layout ($env:CUBE_BUNDLE_PATH, %LOCALAPPDATA%\stm32cube\bundles), then this lab's C:\Toolchain.
# That keeps Ctrl+Shift+B working on machines where the tools are installed but not on PATH.
param(
    [Parameter(Position = 0)]
    [ValidateSet('sentry')]
    [string]$Profile = 'sentry',
    [switch]$Current
)

$ErrorActionPreference = 'Stop'

function Get-CandidatePaths {
    param([string]$Pattern)
    if (-not $Pattern) { return @() }
    if ($Pattern -match '[*?]') {
        return @(Get-ChildItem -Path $Pattern -ErrorAction SilentlyContinue |
                 Sort-Object FullName -Descending |
                 ForEach-Object { $_.FullName })
    }
    if (Test-Path -LiteralPath $Pattern) { return @($Pattern) }
    return @()
}

function Find-ToolPath {
    param([string]$Name, [string[]]$FallbackPatterns, [switch]$PathLast)
    if (-not $PathLast) {
        $cmd = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cmd) { return $cmd.Source }
    }
    foreach ($pattern in $FallbackPatterns) {
        foreach ($hit in (Get-CandidatePaths -Pattern $pattern)) {
            if (Test-Path -LiteralPath $hit) { return $hit }
        }
    }
    if ($PathLast) {
        $cmd = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cmd) { return $cmd.Source }
    }
    return $null
}

$projectRoot = Split-Path -Parent $PSCommandPath

$bundleRoots = @()
if ($env:CUBE_BUNDLE_PATH) { $bundleRoots += $env:CUBE_BUNDLE_PATH }
if ($env:LOCALAPPDATA) { $bundleRoots += (Join-Path $env:LOCALAPPDATA 'stm32cube\bundles') }

$cmakeCandidates = @()
$ninjaCandidates = @()
$gccCandidates = @('C:\Toolchain\bin\arm-none-eabi-gcc.exe')
foreach ($bundle in $bundleRoots) {
    $cmakeCandidates += (Join-Path $bundle 'cmake\*\bin\cmake.exe')
    $ninjaCandidates += (Join-Path $bundle 'ninja\*\bin\ninja.exe')
    $gccCandidates += (Join-Path $bundle 'gnu-tools-for-stm32\*\bin\arm-none-eabi-gcc.exe')
}

Push-Location $projectRoot
try {
    $cmake = Find-ToolPath -Name 'cmake' -FallbackPatterns $cmakeCandidates
    if (-not $cmake) {
        throw 'cmake not found (PATH, ST bundles, CUBE_BUNDLE_PATH). Install CMake or STM32CubeCLT.'
    }
    $ninja = Find-ToolPath -Name 'ninja' -FallbackPatterns $ninjaCandidates
    # C:\Toolchain holds the 15.3.1 this repo's baselines were made with; an older gcc earlier
    # on PATH (14.2.x) must not win, the two differ by ~600 B of text (see AGENTS.md).
    $gcc = Find-ToolPath -Name 'arm-none-eabi-gcc' -FallbackPatterns $gccCandidates -PathLast

    Write-Host "[tools] cmake: $cmake"
    if ($ninja) { Write-Host "[tools] ninja: $ninja" }
    if ($gcc) {
        Write-Host "[tools] gcc  : $gcc"
        $gccDir = Split-Path -Parent $gcc
        if ($env:PATH -notlike "*$gccDir*") { $env:PATH = "$gccDir;$env:PATH" }
    }

    # Pin what the preset does not fix, so a fresh build dir does not pick up whatever is on PATH.
    $configureArgs = @('--preset', $Profile)
    if ($gcc) { $configureArgs += "-DCMAKE_C_COMPILER=$gcc"; $configureArgs += "-DCMAKE_ASM_COMPILER=$gcc" }
    if ($ninja) { $configureArgs += "-DCMAKE_MAKE_PROGRAM=$ninja" }

    Write-Host "== configure: $Profile =="
    & $cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed: $Profile" }

    Write-Host "== build: $Profile =="
    & $cmake --build --preset $Profile
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed: $Profile" }

    $buildDir = @{
        infantry = 'build'
        sentry   = 'build_sentry'
        heavy    = 'build_heavy'
    }[$Profile]
    $sourceElf = Join-Path (Join-Path $projectRoot $buildDir) 'basic_framework.elf'
    if (-not (Test-Path $sourceElf)) { throw "ELF not found: $sourceElf" }
    $sourceElf = (Resolve-Path $sourceElf).Path

    # Give the sentry artifact an explicit profile name without breaking the CMake target name.
    $elf = $sourceElf
    if ($Profile -eq 'sentry') {
        $elf = Join-Path (Split-Path -Parent $sourceElf) 'sentry.elf'
        Copy-Item -LiteralPath $sourceElf -Destination $elf -Force
        $elf = (Resolve-Path $elf).Path
    }

    $currentElf = $null
    $namedFirmwareElf = $null
    if ($Current) {
        $firmwareDir = Join-Path $projectRoot 'firmware'
        New-Item -ItemType Directory -Force -Path $firmwareDir | Out-Null
        if ($Profile -eq 'sentry') {
            $namedFirmwareElf = Join-Path $firmwareDir 'sentry.elf'
            try {
                Copy-Item -LiteralPath $elf -Destination $namedFirmwareElf -Force
            }
            catch {
                throw "cannot update firmware/sentry.elf (stop debug/flash and retry): $_"
            }
        }
        $currentElf = Join-Path $firmwareDir 'current.elf'
        try {
            Copy-Item -LiteralPath $elf -Destination $currentElf -Force
        }
        catch {
            throw "cannot update firmware/current.elf (stop debug/flash and retry): $_"
        }
        # ninja may say "no work to do", leaving the source ELF mtime old; force a fresh stamp.
        (Get-Item -LiteralPath $currentElf).LastWriteTime = Get-Date

        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $currentElf).Hash
        $infoPath = Join-Path $firmwareDir 'current.txt'
        $lines = @(
            "PROFILE=$Profile"
            "SOURCE=$elf"
            $(if ($namedFirmwareElf) { "NAMED=$namedFirmwareElf" })
            "UPDATED=$((Get-Date).ToString('yyyy-MM-dd HH:mm:ss'))"
            "SHA256=$hash"
        ) | Where-Object { $_ }
        $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
        [System.IO.File]::WriteAllLines($infoPath, $lines, $utf8NoBom)
    }

    Write-Host ''
    Write-Host "PROFILE : $Profile"
    Write-Host "ELF     : $elf"
    if ($Current) {
        if ($namedFirmwareElf) { Write-Host "SENTRY  : $namedFirmwareElf" }
        Write-Host "CURRENT : $currentElf"
        Write-Host "INFO    : firmware/current.txt"
        if ($namedFirmwareElf) {
            Write-Host 'Next    : flash firmware/sentry.elf (current.elf remains for EmberProbe compatibility).'
        }
        else {
            Write-Host 'Next    : flash firmware/current.elf (EmberProbe pins this path once).'
        }
    }
    else {
        Write-Host 'Next    : flash the ELF above (flashing is not automated by this script).'
    }
}
finally {
    Pop-Location
}
