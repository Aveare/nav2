$ErrorActionPreference = 'Stop'
$taskProject = Split-Path -Parent $PSScriptRoot
Push-Location $taskProject
try {
    New-Item -ItemType Directory -Force -Path 'tests/build' | Out-Null
    $taskIncludes = @(
        '-Itests/sentry/usb_protocol/stubs',
        '-Iapps/sentry/autonomy',
        '-Icomponents/ax650',
        '-Icomponents/navigation'
    )
    $taskSources = @(
        'tests/sentry/usb_protocol/sentry_usb_protocol_test.c',
        'apps/sentry/autonomy/autonomy.c',
        'apps/sentry/autonomy/vision_link.c',
        'components/ax650/ax650_pose.c',
        'components/navigation/nav_protocol.c'
    )
    foreach ($taskOptimization in @('-O0', '-Ofast')) {
        $taskTag = $taskOptimization.TrimStart('-')
        & gcc -std=c11 -Wall -Wextra -Werror $taskOptimization @taskIncludes @taskSources -lm `
            -o "tests/build/sentry_usb_protocol_$taskTag.exe"
        if ($LASTEXITCODE -ne 0) { throw "sentry USB compilation failed ($taskTag)." }
        & "./tests/build/sentry_usb_protocol_$taskTag.exe"
        if ($LASTEXITCODE -ne 0) { throw "sentry USB regression failed ($taskTag)." }
    }
} finally {
    Pop-Location
}
