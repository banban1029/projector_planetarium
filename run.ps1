# ビルドして起動する（PowerShell 用）。MSYS2 Bash / Git Bash では run.sh を使う。
#   .\run.ps1                                  Debug ビルドで起動（config/projector.yaml）
#   .\run.ps1 --release                        Release ビルドで起動
#   .\run.ps1 --time "2026-12-15 22:00" --grid  それ以外の引数は planetarium.exe にそのまま渡す
# param() を使わず $args で受けるのは、--time などを PowerShell の引数として解釈させないため
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"

$preset = 'mingw-debug'
$appArgs = @($args)
if ($appArgs.Count -gt 0 -and $appArgs[0] -eq '--release') {
    $preset = 'mingw-release'
    $appArgs = @($appArgs | Select-Object -Skip 1)
}
$buildDir = "build\$($preset -replace '^mingw-', '')"

if (-not (Test-Path "$buildDir\build.ninja")) {
    cmake --preset $preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& ".\$buildDir\planetarium.exe" @appArgs
exit $LASTEXITCODE
