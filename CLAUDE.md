# CLAUDE.md

OpenGL のプラネタリウム（Windows、MSYS2 MinGW64、C++17、GLFW/GLEW、yaml-cpp）。
Optoma ML1050ST+（1280×800）で投影する。全体は [README.md](README.md)、計画と TODO は [PLAN.md](PLAN.md)、設計は [docs/](docs/README.md)。

## 変更したら検証する

**検証はスクリプトを実行するだけ**。手で 1 つずつ確かめなくてよい。

```sh
python tests/verify/verify.py           # すべて（約 2〜3 分）
python tests/verify/verify.py --quick   # 描画速度と Stellarium を省く（約 1.5 分）
```

実行したら次の 3 つを行う：

1. 最後の行が `RESULT: PASSED` か確かめる。FAILED なら `build/verify/report.md` の該当節を読んで原因を直す。
   **しきい値を緩めて FAIL を消さない。**
2. `build/verify/report.md` を読み、数値（Stellarium との差、fps など）に前回からの大きな変化が無いか見る。
3. report.md の「目で確かめる画像」に並んだ画像（`sheet_scenes.png`、`sheet_modes.png`、`motion_*.png`）を
   **すべて開き**、それぞれの説明どおりに見えるかを確かめる（例：北が上・東が左、名前が重なっていない、大阪は街明かり）。

検証の詳細は [tests/verify/README.md](tests/verify/README.md)、これまでの結果は [docs/verification.md](docs/verification.md)。
Stellarium が入っていなければ `stellarium` は SKIP になる（失敗ではない）。

## ビルドと実行

```sh
cmake --preset mingw-debug && cmake --build --preset mingw-debug
ctest --preset mingw-debug
./run.sh --time "2026-12-15 22:00" --location achi --screenshot out.png   # MSYS2 Bash（PowerShell は .\run.ps1）
```

`C:\msys64\mingw64\bin` を PATH に入れて実行する。画面を確かめるときは `--screenshot` で PNG を撮って見る。

## 書き方の約束

- コード中のコメントとドキュメントは日本語。コミットメッセージは英語で、内容ごとに分けてコミットする。
- シェーダの投影の式（`shaders/projection.glsl`）と C++ 版（`src/projection.cpp`）は同じ式に保つ（名前の配置が依存する）。
- 設定は `config/projector.yaml`。新しい設定項目は `src/config.*` と config/README.md にも足す。
- 機能を足したら、必要に応じて `tests/` のテストと `tests/verify/verify.py` の検証も足す。
