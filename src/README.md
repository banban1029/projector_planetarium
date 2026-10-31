# src/

C++17 のソースコード。ビルド設定は [../CMakeLists.txt](../CMakeLists.txt)。

## ファイルの役割

| ファイル | 役割 |
|---|---|
| [main.cpp](main.cpp) | 起動・メインループ。設定と星表を読み、毎フレーム「時刻を進める → 天文計算 → 描画」。コマンドラインオプション（`--time` など）とスクリーンショット保存 |
| [config.hpp](config.hpp) / [config.cpp](config.cpp) | [config/projector.yaml](../config/projector.yaml) を構造体に読み込む（yaml-cpp） |
| [sim_clock.hpp](sim_clock.hpp) / [sim_clock.cpp](sim_clock.cpp) | シミュレーション時刻。倍率・逆再生・一時停止、地方時 ⇔ Unix 時刻 ⇔ ユリウス日、現在時刻の表示 |
| [seasons.hpp](seasons.hpp) / [seasons.cpp](seasons.cpp) | 季節プリセットへのジャンプ（巡回・直接）と自動巡回 |
| [modes.hpp](modes.hpp) / [modes.cpp](modes.cpp) | 表示モード（リアル・幻想）の一覧と、モードごとの見せ方のパラメータ（`ModeStyle`） |
| [controls.hpp](controls.hpp) / [controls.cpp](controls.cpp) | キーボード操作、アプリの状態（`AppState`）、ウィンドウタイトルの状態表示 |
| [astro.hpp](astro.hpp) / [astro.cpp](astro.cpp) | 天文計算：恒星時、歳差、赤道 → 地平座標、太陽の位置、大気差 |
| [star_catalog.hpp](star_catalog.hpp) / [star_catalog.cpp](star_catalog.cpp) | [data/stars.csv](../data/stars.csv) の読み込み、B−V → 温度 → 星の色 |
| [sky_renderer.hpp](sky_renderer.hpp) / [sky_renderer.cpp](sky_renderer.cpp) | OpenGL の描画：背景（空・グリッド）、恒星、方位の文字、出力（露出・ガンマ）。薄明の空の色 |
| [gl_util.hpp](gl_util.hpp) / [gl_util.cpp](gl_util.cpp) | シェーダの読み込み・コンパイル・リンク（共通 GLSL の差し込み） |

## 依存関係

```
main ──▶ config, controls, sky_renderer, star_catalog, astro
controls ──▶ sim_clock, seasons, modes, sky_renderer（ViewSettings, Overlays）
sky_renderer ──▶ gl_util, astro, modes, star_catalog, config
star_catalog ──▶ astro
seasons ──▶ sim_clock, config
astro ──▶（なし。テスト tests/astro_test.cpp からも使う）
```

## 書き方の約束

- 角度は関数名・変数名に単位を付ける（`altDeg`、`raDeg`）。内部の三角関数はラジアン。
- 座標系：赤道（x = 春分点, z = 天の北極）、地平（x = 北, y = 東, z = 天頂）。詳細は [docs/astronomy.md](../docs/astronomy.md)。
- OpenGL は 4.5 Core の DSA（`glCreateBuffers`、`glProgramUniform*` など）を使う。
- コメントは日本語。アルゴリズムの詳しい説明は [docs/](../docs/README.md) に書き、コードには要点だけ書く。
