# shaders/

GLSL 4.50 のシェーダ。実行時に読み込むので、**シェーダだけを変えた場合は再ビルド不要**（再起動するだけ）。
読み込みは [src/sky_renderer.cpp](../src/sky_renderer.cpp)、コンパイルは [src/gl_util.cpp](../src/gl_util.cpp)。

## ファイルとパスの対応

| パス（描く順） | 頂点シェーダ | フラグメントシェーダ | 内容 |
|---|---|---|---|
| ① 背景 | [fullscreen.vert](fullscreen.vert) | [sky.frag](sky.frag) | 画素ごとに逆投影し、空の色（薄明）・地面・高度方位グリッドを塗る |
| ② 恒星 | [stars.vert](stars.vert) | [stars.frag](stars.frag) | 1 星 = 1 点スプライト。大気差・減光・瞬き・明るさ・大きさ、ガウスの星像と光芒 |
| ③ 方位の文字 | [compass.vert](compass.vert) | [compass.frag](compass.frag) | N/E/S/W（C キー）。地平線の少し上に置く |
| ④ 出力 | [fullscreen.vert](fullscreen.vert) | [post.frag](post.frag) | HDR → 露出・ガンマ補正・ディザ → 画面 |

①〜③は 16bit 浮動小数（線形）のバッファに描き、④で画面に出す。

## 共通部分

[projection.glsl](projection.glsl) は単独のシェーダではなく、①②③の各シェーダの `#version` 行の直後に差し込まれる。
地平座標 ⇔ 画面の変換（魚眼・透視、回転・反転）をここにまとめ、星・背景・文字が同じ式で投影されるようにしている。

- `projectHorizon(h)`：地平座標の方向 → 画面（順投影。星・文字）
- `unprojectNdc(ndc, h)`：画面 → 地平座標の方向（逆投影。背景）

差し込み後も行番号がずれないよう `#line 2` を入れているので、コンパイルエラーの行番号は元のファイルの行番号になる。

## 主な uniform

| uniform | 設定元 | 意味 |
|---|---|---|
| `u_view`, `u_projType`, `u_halfFov`, `u_aspect`, `u_screen` | `setProjectionUniforms()` | 投影（全シェーダ共通） |
| `u_eqToHor` | 毎フレーム CPU で計算 | J2000 赤道 → 地平の 3×3 行列 |
| `u_magLimit`, `u_magSat`, `u_brightness`, `u_sigma` | YAML `render:` ＋ 薄明 ＋ モード | 星の明るさと大きさ |
| `u_saturation`, `u_twinkle`, `u_glow`, `u_tint` | モード（`ModeStyle`） | 見せ方 |

式の説明は [docs/rendering.md](../docs/rendering.md)。
