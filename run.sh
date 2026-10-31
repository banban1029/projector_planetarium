#!/usr/bin/env bash
# ビルドして起動する。Git Bash / MSYS2 から実行する。
#   ./run.sh                       Debug ビルドで起動（config/projector.yaml）
#   ./run.sh --release             Release ビルドで起動
#   ./run.sh config/other.yaml     別の設定ファイルで起動
set -euo pipefail

cd "$(dirname "$0")"
export PATH="/c/msys64/mingw64/bin:$PATH"

preset=mingw-debug
if [[ "${1:-}" == "--release" ]]; then
    preset=mingw-release
    shift
fi
build_dir="build/${preset#mingw-}"

if [[ ! -f "$build_dir/build.ninja" ]]; then
    cmake --preset "$preset"
fi
cmake --build --preset "$preset"

exec "./$build_dir/planetarium.exe" "$@"
