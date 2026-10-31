# Projector Planetarium 開発方針

OpenGL で星空をリアルタイム描画し、プロジェクタ（Optoma ML1050ST+）で天井・壁に投影する。
**リアルモード**（日本から見える実際の星空・季節）と **幻想モード**（演出重視）を持ち、時間軸に沿って動く。

---

## 1. 開発環境

### 結論：VS Code + CMake + MSYS2 MinGW64（GCC）

| 観点 | VS Code + CMake + MinGW | Visual Studio (MSVC) |
|---|---|---|
| Claude Code からのビルド・実行 | ◎ `cmake --build` をそのまま叩ける | △ `msbuild` で可能だが、IDE 設定（.sln/.vcxproj）を手で触りにくい |
| 現在のPCの状態 | ◎ MSYS2 に GCC 15.2・GLFW・GLEW・GLM が既に入っている | × 未インストール（数GB） |
| ライブラリ導入 | ◎ `pacman -S` で一発 | △ vcpkg 等が別途必要 |
| デバッガ | ○ gdb（VS Code から使用可） | ◎ 非常に強力 |
| GPU デバッグ | RenderDoc（どちらでも使える） | 同左 |

ビルド定義を **CMake** に統一しておけば、後から Visual Studio（「フォルダーを開く」で CMake をそのまま読める）へ移ることもできるので、ロックインはない。

### 確認済みの環境（2026-10-31）

| 項目 | 内容 |
|---|---|
| GPU | Intel Arc 130V（ドライバ 32.0.101.8626） |
| OpenGL | **4.6 Core**、GLSL 4.60（テストプログラムで確認） |
| システム DLL | `C:\Windows\System32\opengl32.dll`（ドライバの実装を呼ぶ窓口） |
| コンパイラ | `C:\msys64\mingw64\bin\g++.exe` 15.2.0 |
| ヘッダ・ライブラリ | `C:\msys64\mingw64\include\GL\`（gl.h, glext.h, glew.h, freeglut）、`GLFW\glfw3.h`、`glm\` |
| 既存パッケージ | glfw 3.4, glew 2.2.0, glm 1.0.3, freeglut 3.8.0, gdb 17.1, pkgconf |
| 今回追加 | cmake 4.2.3, ninja 1.13.2, yaml-cpp, clang-tools-extra（clangd/clang-format） |
| VS Code 拡張 | C/C++, CMake Tools, clang-format（導入済み） |

> 「OpenGL がどこにあるか」：OpenGL 本体は GPU ドライバに含まれ、Windows の `opengl32.dll` 経由で呼ばれる。
> 開発用のヘッダ／インポートライブラリ（`libopengl32.a`）は MSYS2 の `mingw64` 配下にある。
> 1.1 以降の関数は GLEW がランタイムで読み込む。

### ビルド・実行

```sh
# PATH に C:\msys64\mingw64\bin が通っていること（現在通っている）
cmake --preset mingw-debug
cmake --build --preset mingw-debug
./build/debug/planetarium.exe            # 既定で config/projector.yaml を読む
./build/debug/planetarium.exe other.yaml # 別の設定で起動
```

VS Code では CMake Tools が `CMakePresets.json` を読み、F5 で gdb デバッグできる（`.vscode/launch.json`）。

### 使用ライブラリ

| 用途 | ライブラリ | 備考 |
|---|---|---|
| ウィンドウ・入力・マルチモニタ | GLFW 3.4 | プロジェクタ側モニタにフルスクリーン出力 |
| GL 関数ローダ | GLEW 2.2 | 導入済みのため採用（将来 glad2 に置換可） |
| 数学 | GLM | |
| 設定 | yaml-cpp | `config/*.yaml` |
| 天体暦（惑星・月・太陽、歳差・章動） | [Astronomy Engine](https://github.com/cosinekitty/astronomy)（C 版 1 ファイル、MIT） | 追加予定。`third_party/` に同梱 |
| 操作 UI | [Dear ImGui](https://github.com/ocornut/imgui) | 追加予定。PC 画面側の操作パネル |
| 画像・文字 | stb_image / stb_truetype | 追加予定 |

---

## 2. プロジェクタ

**Optoma ML1050ST+**（DLP 0.45" DMD、LED 光源）

| 項目 | 値 | 設計への影響 |
|---|---|---|
| ネイティブ解像度 | **1280×800 (WXGA, 16:10)** | この解像度でレンダリング・出力する（スケーリングで星がボケるのを防ぐ） |
| 最大入力 | 1920×1200 | 入力はできるが縮小されるため使わない |
| 投写比 | **0.8:1**（短焦点） | 投写距離 2.0 m → 幅 2.5 m、高さ約 1.56 m |
| 明るさ | 1000 lm | 部屋を完全に暗くする前提。黒浮きがあるので背景は純黒・円外マスク |
| コントラスト | 20,000:1（ダイナミック） | 暗い星はガンマ／最小輝度を調整して潰れないようにする |

「S1J」の型番部分（同梱品・国内型番など）は未確認。分かり次第追記する。
これらはコードに埋め込まず **`config/projector.yaml` で管理**する。別のプロジェクタや設置場所は YAML を増やして切り替える。

### 出力の進め方（デバッグ画面 → 実機）

1. **今：`output: debug`**。PC 画面に、プロジェクタと同じ 1280×800 のウィンドウを出して開発する。
   星の位置・明るさ・モード・時間操作がこの画面で正しく表現できることを確認する（Stellarium と比較）。
2. **その後：`output: projector`**。Optoma を接続し、`monitor_index` のモニタにフルスクリーン出力する。
   実機で輝度・ガンマ・反転・回転・台形補正を調整する。

コードは同じで、YAML の `output` を切り替えるだけ。プロジェクタが見つからない場合はウィンドウ表示にフォールバックする。

### 投影方式

- **天井投影（推奨）**：天頂を中心とした **等距離魚眼（azimuthal equidistant）** で全天を円形に描く。円の外は黒。
  16:10 の画面に内接させるので、使える円の直径は 800px。
- **壁投影**：南（または任意の方位）を向いた **透視投影**。地平線と地上シルエットを表示。
- 共通：左右・上下反転（天吊り／リア投影）、回転、将来的に台形補正・ワープメッシュ（平面でない面への投影）。

---

## 3. 機能

### 3.1 リアルモード
- 観測地（既定：大阪大学吹田キャンパス 34.82°N, 135.52°E, JST）の **地平座標** で描画。
- 恒星：HYG（Hipparcos ベース）を **6 等級前後まで**（約 5,000〜9,000 個）。
  等級 → 明るさ・大きさ、色指数 B−V → 星の色。
- 太陽・月（満ち欠け）・惑星を Astronomy Engine で計算。
- 太陽高度に応じた空の明るさ（薄明 → 夜）、地平線付近の大気減光。
- 天の川（テクスチャ or 点群）。
- 星座線・星座名（ON/OFF）。季節の代表星座（夏の大三角、冬のダイヤモンドなど）のハイライト。

### 3.2 幻想モード（複数種類）
物理的な正しさよりも演出を優先する。ただし **星の位置はリアルモードと同じ計算を使う**（同じ空の「見せ方」違い）。
そのため季節ジャンプ・時間倍率はどのモードでも効く。

| キー | id | 名前 | 演出のイメージ |
|---|---|---|---|
| 0 | real | リアル | 実際の見え方（3.1） |
| 1 | aurora | オーロラ | 北の空にゆらめくオーロラのカーテン（ノイズシェーダ） |
| 2 | nebula | 星雲 | 色とりどりの星雲・ガスが漂う、星の色を強調 |
| 3 | meteor | 流星群 | 放射点からの流星が多数流れる |
| 4 | constellation | 星座絵 | 星座線と星座絵（神話の絵）が光ってフェードイン |
| 5 | jewel | 宝石箱 | 星を大きく・カラフルに瞬かせる、ブルーム強め |
| 6 | warp | ワープ航行 | 星が中心から流れる光跡になる |
| 7 | milkyway | 天の川 | 天の川を極端に明るく・立体的に |
| 8 | mirror | 水鏡 | 下半分に星空が水面のように映り、揺らぐ |

一覧は [src/modes.cpp](src/modes.cpp) にあり、追加しやすい構成にしている。現段階では仮の背景色でモード切替のみ確認できる。
描画を実装するときは各モードを共通インターフェース（`update(dt, 空の状態)` / `render()`）のクラスにする。

### 3.3 時間軸・季節
- シミュレーション時刻 = 開始時刻 + 経過実時間 × 倍率（逆再生・一時停止あり）。
- 倍率は **実時間に対する倍率**（×1 = 実時間, ×3600 = 1 秒で 1 時間, ×86400 = 1 秒で 1 日）。
  段階は YAML の `time.speed_steps` で定義し、↑↓ で切り替える。
- **季節ジャンプ**：YAML の `seasons`（春 4/15・初夏 6/10・夏 8/10・秋 10/20・冬 1/20 の 21 時台、編集可）へ飛ぶ。
  - **巡回**：S で「今のシミュレーション時刻から見て次に来る季節」へ進む。冬 → 翌年の春 … と年をまたいで回り続ける。Shift+S で逆回り。
  - **直接**：F1〜F5 で各季節の最も近い年の日付へ。
  - **自動巡回**：T で ON/OFF。`season_tour.interval_sec` 秒ごとに次の季節へ進む（上映用）。
- 速度を上げると日周運動（北極星中心の回転）や季節による星座の移り変わりが見える。
- 将来：シナリオファイル（YAML）で「時刻・モード・演出」のタイムラインを自動再生（上映用）。

### 3.4 キーボード操作（実装済み）

| キー | 動作 |
|---|---|
| 0 / 1〜8 | リアル / 幻想モードを直接選択 |
| M / Shift+M | 次 / 前のモード（巡回） |
| S / Shift+S | 次 / 前の季節（巡回） |
| F1〜F5 | 季節プリセットへ直接ジャンプ |
| T | 季節の自動巡回 ON/OFF |
| ↑ / ↓ | 時間倍率を上げる / 下げる |
| Space | 一時停止 / 再開 |
| R | 逆再生の切り替え |
| ← / → | 1 時間戻す / 進める（Shift で 1 日） |
| N | 現在時刻・×1 に戻す |
| F11 | 全画面切り替え |
| H / Esc | ヘルプ表示 / 終了 |

状態（モード・日時・倍率・季節）はデバッグ中はウィンドウタイトルに表示する。
実機投影ではタイトルが見えないので、後で PC 側の操作パネル（ImGui）に表示する。

---

## 4. アーキテクチャ

```
config/            projector.yaml（解像度・投影方式・観測地・時間・季節・描画・シナリオ）
data/              星表・星座線・星座名・星の日本語名（CSV）
  raw/             ダウンロードした元データ（git 管理外）
scripts/           データ変換（Python）
shaders/           GLSL（sky, stars, lines, body, text, bloom, post と共通の projection.glsl）
third_party/       Astronomy Engine、stb
src/               C++（ファイルの役割は src/README.md）
tests/             天文計算・時刻・季節・シナリオのテスト
docs/              設計ドキュメント
```

### 描画パイプライン（実装済み。詳細は [docs/rendering.md](docs/rendering.md)）
1. **天文計算（CPU）**：時刻 → ユリウス日 → 地方恒星時・歳差 → J2000→地平の行列を毎フレーム作る。月・惑星は Astronomy Engine。
2. **星（GPU）**：J2000 赤道座標の単位ベクトルを VBO に入れ、行列を uniform で渡す。点スプライト（ガウシアン）で加算合成。
3. **投影は各シェーダで直接**：共通の GLSL（`projection.glsl`）で魚眼・透視・回転・反転を行う。
   キューブマップを経由しないので、星が 1px の点のまま締まって見える。背景は同じ式の逆投影で塗る。
4. ポストエフェクト（ブルーム、水鏡、フェード、ガンマ、ディザ）→ プロジェクタへ出力。

### ウィンドウ構成
- **出力ウィンドウ**：`monitor_index` のモニタ（プロジェクタ）にフルスクリーン、UI なし。
- **操作ウィンドウ**（後で追加）：PC 画面に ImGui の操作パネル＋プレビュー。共有コンテキストでテクスチャを共有。
- プロジェクタ未接続時はウィンドウ表示にフォールバック（実装済み）。

---

## 5. データ

| データ | 入手先 | ライセンス |
|---|---|---|
| 恒星 | [HYG Database](https://github.com/astronexus/HYG-Database)（hyg_v4x.csv） | CC BY-SA 4.0 |
| 星座線・星座名（和名含む） | [Stellarium skycultures](https://github.com/Stellarium/stellarium-skycultures)（modern） | GPL / CC 系（要確認） |
| 星座線・天の川輪郭（JSON） | [d3-celestial data](https://github.com/ofrohn/d3-celestial/tree/master/data) | BSD-3 |
| 天の川画像 | ESA Gaia / NASA SVS の全天パノラマ | 各配布元の条件に従う |

ライセンスを `data/LICENSES.md` にまとめて記録する（CC BY-SA のデータを含む点に注意）。

---

## 6. マイルストーン

1. **[済] 環境構築**：MSYS2 / CMake / GLFW / GLEW / yaml-cpp、YAML 読み込み、デバッグウィンドウ出力。
2. **[済] 操作の骨組み**：シミュレーション時刻（倍率・逆再生・一時停止）、季節の巡回／直接ジャンプ／自動巡回、モード切替（仮の背景色）。
3. **[済] 星を出す**：HYG 変換スクリプト、点スプライト描画（等級→明るさ・大きさ、B−V→色）、魚眼／透視投影。
4. **[済] 正しい空**：観測地・恒星時・歳差・地平座標、大気差・大気減光、薄明、グリッド表示。天文計算は Meeus の例題でテスト済み。
   設計と実装アルゴリズムは [docs/](docs/README.md) にまとめた。
5. **[済] 観測地**：YAML の `locations`（大阪大学・阿智村）、`L` キー。場所ごとの夜空の暗さ（光害）と地平線の景色。
6. **[済] 太陽系**：月（満ち欠け・地表からの視差）・惑星（水星〜天王星）・太陽を Astronomy Engine で。月明かり。
7. **[済] 見栄え（リアル）**：天の川（式で生成）、星座線・星座名・星の日本語名、方位（漢字）、ブルーム。
8. **[済] 幻想モード**：オーロラ・星雲・流星群・星座絵・宝石箱（十字の光条）・ワープ航行・天の川・水鏡の演出、切り替えのフェード。
9. **[済] 上映の仕組み（一部）**：シナリオ再生（YAML、`P` キー）、画面表示（日時・通知）、配布用フォルダ（`cmake --install`）。
10. **実機投影**：Optoma 接続（`output: projector`）、輝度・ガンマ・反転・回転・台形補正。
11. **上映の仕上げ**：PC 側の操作ウィンドウ（ImGui）など。

検証方針：天体位置はテスト（Meeus の例題・Astronomy Engine との比較）で確かめ、画面全体は **Stellarium で同じ日時・場所を表示して比較**する。

## 7. やり残したこと（TODO）

### 確認・検証（人の目で）
- [x] Stellarium との比較（32 天体の位置。差は最大 39″）。画面どうしの目視比較は未（Stellarium の撮影がこの PC で崩れるため）
- [x] キー操作（全キーの自動テスト ＋ 実際のウィンドウへのキー入力）
- [x] 瞬き・オーロラの揺れ・流れ星・フェード（連続撮影で確認、弱かったものを強めた）
- [x] フレームレートの計測（1280×800 で 109〜270 fps）
- [x] 上の検証をまとめて実行するスクリプト（`python tests/verify/verify.py`）
- [ ] 人の目での最終確認（実機のプロジェクタで、暗い部屋で見る）

### プロジェクタ（Optoma ML1050ST+）
- [ ] 接続（`output: projector`、`monitor_index` の確認）
- [ ] 実機での明るさ・ガンマ・黒浮き・暗い星の見え方の調整（`render:`）
- [ ] 設置に合わせた回転（北を部屋の北へ）・反転・台形補正、平らでない面へのワープ補正
- [ ] 投写距離と映る大きさの決定、型番の「S1J」の確認

### 機能
- [ ] PC 側の操作ウィンドウ（プロジェクタとは別に、プレビューと操作パネル）
- [ ] 星座絵（神話の絵の画像。ライセンスの合う画像が必要）
- [x] 星の名前・星座名が重ならない配置（置けないものは省く）
- [ ] 月の満ち欠けに合わせた明るさの変化（今は円盤の明るさ一定で、形だけ変わる）
- [ ] 6.5 等より暗い星（幻想モードで星を増やすなど）
- [ ] 章動・光行差・固有運動（表示上は 1 画素未満の差。優先度低）

---

## 8. 参考リポジトリ

| リポジトリ | 参考になる点 |
|---|---|
| [Stellarium/stellarium](https://github.com/Stellarium/stellarium) | 定番のプラネタリウム（C++/Qt/OpenGL）。魚眼・ドーム投影、星の明るさモデル、大気表現。検証用の「正解」としても使う |
| [Stellarium/stellarium-skycultures](https://github.com/Stellarium/stellarium-skycultures) | 星座線・星座名データ |
| [cosinekitty/astronomy](https://github.com/cosinekitty/astronomy) | Astronomy Engine。C 1 ファイルで惑星・月・座標変換。依存が少なく組み込みやすい |
| [astronexus/HYG-Database](https://github.com/astronexus/HYG-Database) | 恒星カタログ |
| [ofrohn/d3-celestial](https://github.com/ofrohn/d3-celestial) | Web の星図。星座線・天の川の JSON、投影方式の実装例 |
| [OpenSpace/OpenSpace](https://github.com/OpenSpace/OpenSpace) | 研究用の宇宙可視化（ドーム上映対応、C++/OpenGL）。大規模構成の参考 |
| [CelestiaProject/Celestia](https://github.com/CelestiaProject/Celestia) | 3D 宇宙シミュレータ（C++/OpenGL）。星の描画・時間制御 |
| [liberfa/erfa](https://github.com/liberfa/erfa) | IAU SOFA 互換の天文計算ライブラリ（高精度が必要になった場合） |
| [ocornut/imgui](https://github.com/ocornut/imgui) | 操作 UI |
| [nothings/stb](https://github.com/nothings/stb) | 画像読み込み・フォント |
| [LearnOpenGL](https://learnopengl.com/) | モダン OpenGL（FBO、キューブマップ、ブルーム）の解説 |
| Paul Bourke「Dome / fisheye projection」 (paulbourke.net/dome/) | 魚眼・ドーム投影、ワープメッシュの理論 |
