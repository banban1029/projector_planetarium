# src/

C++17 のソースコード。ビルド設定は [../CMakeLists.txt](../CMakeLists.txt)。

OpenGL を使わない部分は `planetarium_core`（静的ライブラリ）にまとめ、テストからも使う。

## ファイルの役割

### 本体・操作
| ファイル | 役割 |
|---|---|
| [main.cpp](main.cpp) | 起動・メインループ。設定・星表・星座を読み、毎フレーム「時刻を進める → 天文計算 → 演出と文字を積む → 描画」。コマンドラインオプション、スクリーンショット・連続撮影、描画速度の計測、天体位置の出力 |
| [controls.hpp](controls.hpp) / [controls.cpp](controls.cpp) | キーボード操作（`handleKey()`。テストからも呼ぶ）、アプリの状態（`AppState`）、通知（右下）、画面表示（左下）、ウィンドウタイトル、シナリオの場面の反映 |
| [operator_window.hpp](operator_window.hpp) / [operator_window.cpp](operator_window.cpp) | 操作パネル（2 つ目のウィンドウ）。出力テクスチャのプレビューと状態・キー一覧 |
| [paths.hpp](paths.hpp) / [paths.cpp](paths.cpp) | シェーダ・データの置き場所を探す（環境変数 → 実行ファイルの隣 → ソースフォルダ） |

### planetarium_core（OpenGL を使わない）
| ファイル | 役割 |
|---|---|
| [config.hpp](config.hpp) / [config.cpp](config.cpp) | [config/projector.yaml](../config/projector.yaml) を構造体に読み込む（yaml-cpp） |
| [sim_clock.hpp](sim_clock.hpp) / [sim_clock.cpp](sim_clock.cpp) | シミュレーション時刻。倍率・逆再生・一時停止、地方時 ⇔ Unix 時刻 ⇔ ユリウス日、現在時刻の表示 |
| [seasons.hpp](seasons.hpp) / [seasons.cpp](seasons.cpp) | 季節プリセットへのジャンプ（巡回・直接）と自動巡回 |
| [scenario.hpp](scenario.hpp) / [scenario.cpp](scenario.cpp) | シナリオ（場面の並び）を実時間で進める |
| [astro.hpp](astro.hpp) / [astro.cpp](astro.cpp) | 天文計算：恒星時、歳差、赤道 → 地平座標、太陽の位置、大気差 |
| [bodies.hpp](bodies.hpp) / [bodies.cpp](bodies.cpp) | 太陽・月・惑星の位置・等級・満ち欠け（Astronomy Engine） |

### 描画
| ファイル | 役割 |
|---|---|
| [sky_renderer.hpp](sky_renderer.hpp) / [sky_renderer.cpp](sky_renderer.cpp) | 描画の全体：背景 → 星座線 → 恒星・惑星 → 月・太陽 → 演出の線 → 文字 → ブルーム → 出力。薄明・月明かり・光害の計算 |
| [star_catalog.hpp](star_catalog.hpp) / [star_catalog.cpp](star_catalog.cpp) | [data/stars.csv](../data/stars.csv) の読み込み、B−V → 温度 → 星の色 |
| [constellations.hpp](constellations.hpp) / [constellations.cpp](constellations.cpp) | 星座線・星座名・星の日本語名の CSV を読む |
| [line_renderer.hpp](line_renderer.hpp) / [line_renderer.cpp](line_renderer.cpp) | 太さのある線（ジオメトリシェーダ）。大円に沿った分割 `appendArc()` |
| [text_renderer.hpp](text_renderer.hpp) / [text_renderer.cpp](text_renderer.cpp) | 日本語の文字（stb_truetype でフォントからアトラスを作る） |
| [projection.hpp](projection.hpp) / [projection.cpp](projection.cpp) | 視点の設定（`ViewSettings`）と投影のパラメータ。シェーダの投影・景色の式の C++ 版（名前の配置に使う） |
| [labels.hpp](labels.hpp) / [labels.cpp](labels.cpp) | 名前を画面上で重ならないように配置する |
| [effects.hpp](effects.hpp) / [effects.cpp](effects.cpp) | 流れ星、ワープの光跡（CPU で線を作る） |
| [modes.hpp](modes.hpp) / [modes.cpp](modes.cpp) | 表示モード（リアル・幻想 8 種）の一覧と、見せ方・演出のパラメータ（`ModeStyle`） |
| [gl_util.hpp](gl_util.hpp) / [gl_util.cpp](gl_util.cpp) | シェーダの読み込み・コンパイル・リンク（共通 GLSL の差し込み、ジオメトリシェーダ） |

## 依存関係

```
main ──▶ controls, sky_renderer, constellations, star_catalog, effects, bodies, paths
controls ──▶ sim_clock, seasons, scenario, modes, sky_renderer（ViewSettings, Overlays）
sky_renderer ──▶ text_renderer, line_renderer, gl_util, bodies, constellations, star_catalog, modes, config
bodies ──▶ astro, Astronomy Engine（third_party）
astro ──▶（なし。テストからも使う）
```

## 書き方の約束

- 角度は関数名・変数名に単位を付ける（`altDeg`、`raDeg`）。内部の三角関数はラジアン。
- 座標系：赤道（x = 春分点, z = 天の北極）、地平（x = 北, y = 東, z = 天頂）。詳細は [docs/astronomy.md](../docs/astronomy.md)。
- OpenGL は 4.5 Core の DSA（`glCreateBuffers`、`glProgramUniform*` など）を使う。
- コメントは日本語。アルゴリズムの詳しい説明は [docs/](../docs/README.md) に書き、コードには要点だけ書く。
