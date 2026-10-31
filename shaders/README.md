# shaders/

GLSL 4.50 のシェーダ。実行時に読み込むので、**シェーダだけを変えた場合は再ビルド不要**（再起動するだけ）。
読み込みは [src/sky_renderer.cpp](../src/sky_renderer.cpp) など、コンパイルは [src/gl_util.cpp](../src/gl_util.cpp)。

## ファイルと描く順の対応

| 順 | 頂点 | ジオメトリ | フラグメント | 内容 |
|---|---|---|---|---|
| ① 背景 | [fullscreen.vert](fullscreen.vert) | | [sky.frag](sky.frag) | 画素ごとに逆投影し、空の色（薄明・月明かり）・天の川・光害・オーロラ・星雲・地上の景色・グリッド |
| ② 星座線 | [lines.vert](lines.vert) | [lines.geom](lines.geom) | [lines.frag](lines.frag) | 線分を太さのある四角形に広げる（流れ星・ワープの光跡も同じ） |
| ③ 恒星・惑星 | [stars.vert](stars.vert) | | [stars.frag](stars.frag) | 1 星 = 1 点スプライト。大気差・減光・瞬き・明るさ・大きさ、ガウスの星像・光芒・十字の光条 |
| ④ 月・太陽 | [body.vert](body.vert) | | [body.frag](body.frag) | 円盤・満ち欠け・海の模様・にじみ |
| ⑤ 文字 | [text.vert](text.vert) | | [text.frag](text.frag) | 名前・方位・画面表示 |
| ⑥ ブルーム | [fullscreen.vert](fullscreen.vert) | | [bloom.frag](bloom.frag) | 明るい部分の抽出とガウスぼかし |
| ⑦ 仕上げ | [fullscreen.vert](fullscreen.vert) | | [post.frag](post.frag) | 水鏡の映り込み、にじみ、フェード、露出・ガンマ・ディザ → 出力テクスチャ（8bit） |
| ⑧ 重ねる文字 | [text.vert](text.vert) | | [text.frag](text.frag) | 画面表示・通知・クレジット（仕上げの影響を受けない） |
| ⑨ テストパターン | [fullscreen.vert](fullscreen.vert) | | [testpattern.frag](testpattern.frag) | X キー。出力の値をそのまま出す調整用の模様 |
| ⑩ 操作パネル | [fullscreen.vert](fullscreen.vert) | | [preview.frag](preview.frag) | 出力テクスチャを操作パネルに縮小表示 |

①〜⑤は 16bit 浮動小数（線形）のバッファに描き、⑦で画面に出す。

## 共通部分

[projection.glsl](projection.glsl) は単独のシェーダではなく、各シェーダ（頂点・フラグメント）の `#version` 行の直後に差し込まれる。

- `projectHorizon(h)`：地平座標の方向 → 画面（順投影。星・線・文字・月）
- `unprojectNdc(ndc, h)`：画面 → 地平座標の方向（逆投影。背景・水鏡）
- `groundAltDeg(方位)` / `aboveGround(h, 余裕)`：地平線の景色（山並み・街並み）の高さ。すべての描画が同じ景色に隠れる

差し込み後も行番号がずれないよう `#line 2` を入れているので、コンパイルエラーの行番号は元のファイルの行番号になる。

> 注意：`noise1`〜`noise4` は GLSL の組み込み関数名なので、自作のノイズ関数には別の名前（`valueNoise` など）を付ける。

## 主な uniform

| uniform | 設定元 | 意味 |
|---|---|---|
| `u_view`, `u_projType`, `u_halfFov`, `u_aspect`, `u_screen`, `u_landscape` | `setProjectionUniforms()` | 投影と景色（共通） |
| `u_eqToHor` | 毎フレーム CPU で計算 | J2000 赤道 → 地平の 3×3 行列 |
| `u_magLimit`, `u_magSat`, `u_brightness`, `u_sigma` | YAML `render:` ＋ 薄明・月明かり ＋ モード | 星の明るさと大きさ |
| `u_saturation`, `u_twinkle`, `u_glow`, `u_spikes`, `u_tint` | モード（`ModeStyle`） | 見せ方 |
| `u_milkyWay`, `u_lightPollution`, `u_aurora`, `u_nebula` | 観測地 ＋ モード | 背景の演出 |

式の説明は [docs/rendering.md](../docs/rendering.md)。
