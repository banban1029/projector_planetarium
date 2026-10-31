#!/usr/bin/env bash
# 解説つき上映「大阪から阿智村へ ― 11 月の星空」（約 5 分、ナレーションと BGM つき）を流す。MSYS2 Bash / Git Bash 用。
#   ./show.sh                       ビルドし、音を用意して上映を始める（PC 画面のウィンドウ。F11 で全画面）
#   ./show.sh config/ceiling.yaml   プロジェクタ用などの設定ファイルを指定する
#   ./show.sh --seek 120            2 分の場面から始める（確認用）
# 上映中のキー: P 停止 / B 音 ON・OFF / F11 全画面 / Esc 終了
# ストーリーは shows/autumn_2026.md、台本（日時・場所・字幕）は shows/autumn_2026.yaml
set -euo pipefail
cd "$(dirname "$0")"
export PATH="/c/msys64/mingw64/bin:$PATH"
SHOW=shows/autumn_2026.yaml

# 上映は Release ビルド（速い。Windows のスマート アプリ コントロールに止められたときの切り分けにもなる）
[[ -f build/release/build.ninja ]] || cmake --preset mingw-release
cmake --build --preset mingw-release
# ナレーション（Windows の日本語音声）と BGM を作る。台本が変わった場面だけ作り直す
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/make_show_audio.ps1 "$SHOW"
exec ./run.sh --release --scenario "$SHOW" --play "$@"
