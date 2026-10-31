#!/usr/bin/env bash
# 解説つき上映「大阪から阿智村へ ― 11 月の星空」（約 5 分半、ナレーションと BGM つき）を流す。MSYS2 Bash / Git Bash 用。
# プロジェクタがつながっていればプロジェクタに全画面で、無ければ PC の画面にウィンドウで出す（run.sh と同じ）。
#   ./show.sh --release                       本番。Release ビルドで上映する
#   ./show.sh                                 Debug ビルドで上映する（開発中の確認）
#   ./show.sh --release config/ceiling.yaml   別の設定ファイルを指定する
#   ./show.sh --seek 120                      2 分の場面から始める（確認用）
# 上映中のキー: Space 一時停止・再開 / P 停止 / B 音 ON・OFF / F11 全画面 / Esc 終了
# ストーリーは shows/autumn_2026.md、台本（日時・場所・字幕）は shows/autumn_2026.yaml
set -euo pipefail
cd "$(dirname "$0")"
export PATH="/c/msys64/mingw64/bin:$PATH"
SHOW=shows/autumn_2026.yaml

preset=mingw-debug
release=()
if [[ "${1:-}" == "--release" ]]; then
    preset=mingw-release
    release=(--release)
    shift
fi
[[ -f "build/${preset#mingw-}/build.ninja" ]] || cmake --preset "$preset"
cmake --build --preset "$preset"
# ナレーション（Windows の日本語音声）と BGM を作る。台本が変わった場面だけ作り直す
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/make_show_audio.ps1 "$SHOW"
exec ./run.sh "${release[@]}" "$@" --scenario "$SHOW" --play
