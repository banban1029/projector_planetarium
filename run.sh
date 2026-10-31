#!/usr/bin/env bash
# ビルドして起動する。Git Bash / MSYS2 から実行する。
# プロジェクタがつながっていればプロジェクタに全画面で、無ければ PC の画面にウィンドウで出す。
#   ./run.sh                       Debug ビルドで起動（config/projector.yaml）
#   ./run.sh --output debug        プロジェクタがあっても PC の画面にウィンドウで出す
#   ./run.sh --release             Release ビルドで起動（Windows に止められたら Debug で起動）
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

# Release を作り直すと、Windows のスマート アプリ コントロールに止められることがある。そのときは Debug で起動する
if [[ "$preset" == mingw-release ]] && ! "./$build_dir/planetarium.exe" --list-monitors >/dev/null 2>&1; then
    echo "[run] Release 版が起動できない（Windows のスマート アプリ コントロールなど）ので、Debug 版で起動します"
    preset=mingw-debug
    build_dir=build/debug
    [[ -f "$build_dir/build.ninja" ]] || cmake --preset "$preset"
    cmake --build --preset "$preset"
fi

# --output projector を先に置く（後ろに書いた --output debug が優先される）
exec "./$build_dir/planetarium.exe" --output projector "$@"
