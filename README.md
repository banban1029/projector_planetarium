# Projector Planetarium

OpenGL で星空を描画し、プロジェクタ（Optoma ML1050ST+）で投影するプログラム。
開発方針は [PLAN.md](PLAN.md) を参照。

> 現段階では星はまだ描画されない。モードごとの仮の背景色と、ウィンドウタイトルの表示（モード・日時・倍率・季節）で
> キーボード操作を確認できる状態。

---

## 1. 必要なもの（このPCでは導入済み）

| もの | 場所 / 入手 |
|---|---|
| MSYS2（GCC, CMake, Ninja, GLFW, GLEW, GLM, yaml-cpp） | `C:\msys64\mingw64\bin` |
| VS Code ＋ 拡張「C/C++」「CMake Tools」 | 導入済み |

`C:\msys64\mingw64\bin` が **PATH に入っていること**（実行時に `glfw3.dll` などをここから読むため）。確認方法：

```powershell
g++ --version     # 「Built by MSYS2 project」と出ればOK
cmake --version
```

別のPCで一から揃える場合は、MSYS2 を入れて「MSYS2 MINGW64」ターミナルで次を実行し、`C:\msys64\mingw64\bin` を PATH に追加する。

```sh
pacman -S --needed mingw-w64-x86_64-{gcc,gdb,cmake,ninja,glfw,glew,glm,yaml-cpp,clang-tools-extra}
```

---

## 2. 実行方法

### 方法 A：VS Code から（おすすめ）

1. VS Code で `projector_planetarium` フォルダを開く。
2. 初回のみ：下のステータスバー（または `Ctrl+Shift+P` →「CMake: Select Configure Preset」）で
   **「MSYS2 MinGW64 Debug」** を選ぶ。
3. ビルド：**`F7`**（または `Ctrl+Shift+P` →「CMake: Build」）
4. 実行：
   - **`Shift+F5`** … デバッガなしで実行（「CMake: Run Without Debugging」）
   - **`Ctrl+F5`** … gdb でデバッグ実行（「CMake: Debug」）
5. ウィンドウが開いたら、そのウィンドウをクリックしてからキー操作する（ログは VS Code のターミナルに出る）。

### 方法 B：起動スクリプト（Git Bash）

プロジェクトフォルダで：

```sh
./run.sh                       # ビルドして起動（Debug, config/projector.yaml）
./run.sh --release             # Release ビルドで起動
./run.sh config/other.yaml     # 別の設定ファイルで起動
```

初回は自動で `cmake --preset` も実行する。変更があったファイルだけ再ビルドしてから起動する。
VS Code のターミナルで「Git Bash」を選べばそのまま使える。

### 方法 C：ターミナル（PowerShell）から

プロジェクトフォルダで：

```powershell
# 初回のみ（CMakeLists.txt を変えたときも）
cmake --preset mingw-debug

# ビルド
cmake --build --preset mingw-debug

# 実行
.\build\debug\planetarium.exe
```

別の設定ファイルを使う場合は引数で渡す：

```powershell
.\build\debug\planetarium.exe config\other.yaml
```

リリース（最適化）ビルドは `mingw-debug` を `mingw-release` に、実行ファイルを `.\build\release\planetarium.exe` に読み替える。

### 起動したときの表示

ターミナルに次のようなログとキー一覧が出て、1280×800 のウィンドウが開く。

```
monitor[0]: Generic PnP Monitor 2880x1800@60Hz
GL 4.5.0 - Build 32.0.101.8626 / Intel(R) Arc(TM) 130V GPU (8GB)
output: debug 1280x800, observer: 大阪大学 吹田キャンパス (34.82, 135.52)
---- keys ----
  ...
```

ウィンドウタイトルに現在の状態が表示される：

```
[リアル] 2027-01-20 23:41:07 (UTC+9)  >> x3600  季節: 冬  |  現在 2026-10-31 21:05:12
 ~~~~~~  ~~~~~~~~~~~~~~~~~~~~~~~~~~~  ~~ ~~~~~  ~~~~~~~~       ~~~~~~~~~~~~~~~~~~~~~~~~
 モード   シミュレーション日時（地方時）     │  時間倍率  最後に飛んだ季節   実際の現在時刻（倍率・ジャンプに関係なく常に実時間）
                                       └ >> 再生 / << 逆再生 / || 一時停止
```

季節は季節ジャンプ後のみ表示され、自動巡回中は `(巡回中)` が付く。

### 終了

`Esc`、またはウィンドウの × ボタン。

---

## 3. キーボード操作

| キー | 動作 |
|---|---|
| `0` | リアルモード |
| `1`〜`8` | 幻想モード（1 オーロラ / 2 星雲 / 3 流星群 / 4 星座絵 / 5 宝石箱 / 6 ワープ航行 / 7 天の川 / 8 水鏡） |
| `M` / `Shift+M` | 次 / 前のモード（巡回） |
| `S` / `Shift+S` | 次 / 前の季節（巡回、年をまたいで回り続ける） |
| `F1`〜`F5` | 春 / 初夏 / 夏 / 秋 / 冬 へ直接ジャンプ |
| `T` | 季節の自動巡回 ON/OFF |
| `↑` / `↓` | 時間倍率を上げる / 下げる |
| `Space` | 一時停止 / 再開 |
| `R` | 逆再生の切り替え |
| `←` / `→` | 1 時間戻す / 進める（`Shift` 付きで 1 日、押しっぱなしで連続） |
| `N` | 現在時刻・×1（実時間）に戻す |
| `F11` | 全画面切り替え |
| `H` | キー一覧をターミナルに表示 |
| `Esc` | 終了 |

時間倍率の段階（`↑` を押すごと）：

```
×1 → ×10 → ×60 → ×300 → ×600 → ×1800 → ×3600 → ×21600 → ×86400
実時間  10倍  1秒で1分  5分  10分   30分    1時間    6時間     1日
```

---

## 4. 操作例

### 例1：今夜の星空を実時間で見る
1. `0`（リアル）
2. `N`（現在時刻・×1）

→ タイトルが `[リアル] 2026-10-31 21:05:12 (UTC+9)  >> x1  |  現在 2026-10-31 21:05:12` のように、
シミュレーション日時と現在時刻が一致して進む。

### 例2：冬の星空に飛ぶ
1. `F5`

→ `季節: 冬`、日時が 1 月 20 日 21:00 になる。ターミナルに `[season] 冬 -> 2027-01-20 21:00:00 (UTC+9)`。

### 例3：四季を順番に巡る
1. `S` → 冬（次に来る季節）
2. `S` → 春
3. `S` → 初夏
4. `S` → 夏
5. `S` → 秋
6. `S` → 冬（翌年）… と回り続ける。`Shift+S` で逆回り。

### 例4：一晩の星の動きを早送りで見る
1. `F1`（春 21:00）
2. `↑` を 6 回 → `x3600`（1 秒で 1 時間）
3. 星が東から昇り西へ沈む（日周運動）のを見る
4. `Space` で一時停止、`←` / `→` で 1 時間ずつ戻す・進める
5. `N` で現在時刻・実時間に戻る

### 例5：時間を巻き戻す
1. `↑` を 5 回 → `x1800`
2. `R` → タイトルが `<<` になり、時間が逆に進む
3. もう一度 `R` で順方向に戻る

### 例6：上映用（幻想モードで季節を自動巡回）
1. `1`（オーロラ）など好きな幻想モードを選ぶ
2. `T` → `(巡回中)` になり、30 秒ごとに次の季節へ進む
3. 途中で `M` を押すとモードだけ切り替わり、巡回は続く
4. もう一度 `T` で巡回停止

巡回間隔や季節の日付は [config/projector.yaml](config/projector.yaml) の `season_tour` / `seasons` で変更できる。

---

## 5. 設定（config/projector.yaml）

よく変える項目：

| 項目 | 意味 |
|---|---|
| `output` | `debug`（PC画面にウィンドウ）/ `projector`（プロジェクタにフルスクリーン） |
| `projector.width` / `height` | 出力解像度（ML1050ST+ は 1280×800） |
| `projector.monitor_index` | `output: projector` のときの出力先モニタ番号（起動ログの `monitor[n]`） |
| `observer` | 観測地（緯度・経度・タイムゾーン） |
| `time.start` | 起動時の日時。`"now"` または `"2026-12-14 22:00"` |
| `time.speed` / `speed_steps` | 起動時の倍率 / `↑↓` で切り替える倍率の段階 |
| `seasons` | 季節プリセット（並び順 = 巡回順） |
| `season_tour` | 自動巡回の ON/OFF と間隔（秒） |
| `mode` | 起動時のモード（`real`, `aurora`, `nebula`, …） |

YAML を変えたら **再起動するだけ**でよい（再ビルド不要）。

---

## 6. 困ったとき

| 症状 | 対処 |
|---|---|
| `glfw3.dll が見つかりません` などのエラー | `C:\msys64\mingw64\bin` が PATH に入っていない。PATH を追加して VS Code / ターミナルを開き直す |
| exe をダブルクリックするとすぐ閉じる | 設定ファイルのエラーの可能性。ターミナルから実行してメッセージを見る |
| キーが効かない | 星空のウィンドウをクリックしてフォーカスを当てる |
| `cmake --preset` で失敗する | `build` フォルダを削除してからやり直す |
| VS Code でプリセットが出ない | `Ctrl+Shift+P` →「Developer: Reload Window」 |
