# 上映の音を作る: 台本の字幕（または narration）を Windows の日本語音声で読み上げて WAV にし、BGM を作曲する。
#   .\scripts\make_show_audio.ps1                            shows/autumn_2026.yaml の音を作る（変わった場面だけ）
#   .\scripts\make_show_audio.ps1 shows/xxx.yaml -Force      すべて作り直す
# MSYS2 Bash からは: powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/make_show_audio.ps1
# 出力先と声は台本の audio:（narration_dir, voice, rate, bgm）。出力は git 管理外（build/ の下）
param([string]$Show = 'shows/autumn_2026.yaml', [switch]$Force)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
Set-Location $root
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"

# 台本の読み上げ文はアプリに出してもらう（YAML の読み方をアプリと揃えるため）
$exe = Join-Path $root 'build\release\planetarium.exe'
if (-not (Test-Path $exe)) {
    cmake --preset mingw-release | Out-Null
    cmake --build --preset mingw-release | Out-Null
}
$previous = [Console]::OutputEncoding
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$lines = & $exe --scenario $Show --dump-narration
[Console]::OutputEncoding = $previous
if ($LASTEXITCODE -ne 0) { throw "cannot read $Show" }

$settings = @{}
$steps = @()
foreach ($line in $lines) {
    $f = $line -split "`t", 3
    if ($line.StartsWith('#')) { $settings[$f[0].Substring(1)] = $f[1] }
    elseif ($f.Count -eq 3) { $steps += [pscustomobject]@{ Index = $f[0]; Duration = [double]$f[1]; Text = $f[2] } }
}
if (-not $settings['dir']) { throw "$Show has no audio.narration_dir" }
$dir = Join-Path $root $settings['dir']
New-Item -ItemType Directory -Force $dir | Out-Null

Add-Type -AssemblyName System.Speech
$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer
if ($settings['voice']) {
    try { $synth.SelectVoice($settings['voice']) }
    catch {
        $ja = $synth.GetInstalledVoices() | Where-Object { $_.VoiceInfo.Culture.Name -eq 'ja-JP' } | Select-Object -First 1
        if (-not $ja) { throw "no Japanese voice is installed (Settings > Time & language > Speech)" }
        Write-Host "voice '$($settings['voice'])' not found, using $($ja.VoiceInfo.Name)"
        $synth.SelectVoice($ja.VoiceInfo.Name)
    }
}
$synth.Rate = [int]$settings['rate']
$format = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(22050, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)

$made = 0
$tooLong = 0
foreach ($s in $steps) {
    $wav = Join-Path $dir "step_$($s.Index).wav"
    $txt = Join-Path $dir "step_$($s.Index).txt"
    if (-not $s.Text) {
        Remove-Item $wav, $txt -ErrorAction SilentlyContinue
        continue
    }
    # 読み上げ文が前回と同じなら作り直さない
    $same = (Test-Path $wav) -and (Test-Path $txt) -and ((Get-Content $txt -Raw -Encoding UTF8).TrimEnd() -eq $s.Text)
    if ($Force -or -not $same) {
        $synth.SetOutputToWaveFile($wav, $format)
        $synth.Speak($s.Text)
        $synth.SetOutputToNull()
        [System.IO.File]::WriteAllText($txt, $s.Text, (New-Object System.Text.UTF8Encoding($false)))
        $made++
    }
    # 長さ（44 バイトのヘッダ以降が 16bit モノラル）を場面の長さと比べる
    $seconds = ((Get-Item $wav).Length - 44) / (22050 * 2)
    $mark = if ($seconds -gt $s.Duration - 0.5) { $tooLong++; '  <-- longer than the step' } else { '' }
    '{0}: {1,5:N1} s / {2,4} s  {3}{4}' -f $s.Index, $seconds, $s.Duration, $s.Text, $mark
}
$synth.Dispose()
"narration: $made made, $($steps.Count) steps -> $dir"

if ($settings['bgm']) {
    $bgm = Join-Path $root $settings['bgm']
    if ($Force -or -not (Test-Path $bgm)) {
        python -I scripts/make_bgm.py $bgm
        if ($LASTEXITCODE -ne 0) { throw 'make_bgm.py failed' }
    }
    "bgm: $bgm"
}
if ($tooLong) { Write-Warning "$tooLong narrations are longer than their steps. Shorten the text or lengthen duration_sec." }
