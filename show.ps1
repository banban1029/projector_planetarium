# 解説つき上映「大阪から阿智村へ ― 11 月の星空」（約 5 分半、ナレーションと BGM つき）を流す。PowerShell 用。
# プロジェクタがつながっていればプロジェクタに全画面で、無ければ PC の画面にウィンドウで出す（run.ps1 と同じ）。
#   .\show.ps1 --release                       本番。Release ビルドで上映する
#   .\show.ps1                                 Debug ビルドで上映する（開発中の確認）
#   .\show.ps1 --release config\ceiling.yaml   別の設定ファイルを指定する
#   .\show.ps1 --seek 120                      2 分の場面から始める（確認用）
# 上映中のキー: Space 一時停止・再開 / P 停止 / B 音 ON・OFF / F11 全画面 / Esc 終了
# ストーリーは shows/autumn_2026.md、台本（日時・場所・字幕）は shows/autumn_2026.yaml
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
$show = 'shows/autumn_2026.yaml'

$preset = 'mingw-debug'
$runArgs = @($args)
if ($runArgs.Count -gt 0 -and $runArgs[0] -eq '--release') {
    $preset = 'mingw-release'
    $runArgs = @($runArgs | Select-Object -Skip 1)
}
$buildDir = "build\$($preset -replace '^mingw-', '')"
if (-not (Test-Path "$buildDir\build.ninja")) { cmake --preset $preset; if ($LASTEXITCODE) { exit $LASTEXITCODE } }
cmake --build --preset $preset
if ($LASTEXITCODE) { exit $LASTEXITCODE }
# ナレーション（Windows の日本語音声）と BGM を作る。台本が変わった場面だけ作り直す
& "$PSScriptRoot\scripts\make_show_audio.ps1" $show
if ($preset -eq 'mingw-release') { $runArgs = @('--release') + $runArgs }
& "$PSScriptRoot\run.ps1" @runArgs --scenario $show --play
exit $LASTEXITCODE
