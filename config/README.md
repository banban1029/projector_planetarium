# config/

実行時の設定（YAML）。**変更したら再起動するだけでよく、再ビルドは不要**。

| ファイル | 内容 |
|---|---|
| [projector.yaml](projector.yaml) | 既定の設定。起動時に引数で別のファイルを指定しなければこれを読む |

## 項目

| セクション | 内容 |
|---|---|
| `output` | `debug`（PC 画面にウィンドウ）/ `projector`（プロジェクタにフルスクリーン） |
| `operator_window` | 操作パネル（PC の画面の 2 つ目のウィンドウ）：`auto`（projector のときだけ）/ `on` / `off` |
| `projector` | プロジェクタの解像度（ML1050ST+ は 1280×800）、出力先モニタ番号、左右・上下反転、垂直同期 |
| `projection` | 投影方式（`fisheye` 天井向け / `perspective` 壁向け）、画角、視線の方向、回転 |
| `location` | 起動時の観測地の `id` |
| `locations` | 観測地の一覧。`id`・名前・緯度・経度・標高・タイムゾーン・`limiting_magnitude`（その場所の夜空で見える最暗等級＝光害の程度）・`landscape`（地平線の景色 `city` / `mountains` / `none`）。`L` キーで順に切り替える |
| `time` | 起動時の日時（`"now"` または `"2026-12-15 22:00"`）、時間倍率とその段階 |
| `seasons` | 季節プリセット（名前・月日・時刻）。並び順 = `S` キーで巡回する順 |
| `season_tour` | 季節の自動巡回（`T` キー）の ON/OFF と間隔 |
| `mode` | 起動時の表示モード（`real`, `aurora`, `nebula`, …） |
| `render` | 星の明るさ・大きさ、限界等級、大気減光、月の大きさ（`moon_scale`）、光のにじみ（`bloom`）、露出・ガンマ、フォント、起動時の補助表示（グリッド・方位・星座線・名前・画面表示） |
| `scenario` | 上映用の場面の並び（`P` キーで再生）。下の「シナリオを書く」 |

各項目の意味と既定値は YAML 内のコメントを参照。読み込み処理は [src/config.cpp](../src/config.cpp)。

## 設定を使い分ける

場所や設置ごとに YAML を複製して、起動時に指定する：

```sh
cp config/projector.yaml config/ceiling.yaml     # 天井投影用など
./run.sh config/ceiling.yaml                       # MSYS2 Bash
```

```powershell
.\run.ps1 config\ceiling.yaml                      # PowerShell
```

## 観測地を追加する

`locations` に項目を足す。`id` は `location:` や `--location` で使う名前（半角英数字）。

```yaml
locations:
  - id: osaka            # 大阪大学 吹田キャンパス（街明かりあり, 4.0 等まで）
  - id: achi             # 長野県 阿智村（日本一の星空, 6.5 等まで）
  - id: tokyo            # ← 追加例
    name: "東京"
    latitude_deg: 35.68
    longitude_deg: 139.77
    elevation_m: 40
    timezone_hours: 9
    limiting_magnitude: 3.0
```

`limiting_magnitude` は見える星の数だけでなく、光害の明るみ（オレンジ色の地平線）と天の川の濃さも決める。目安：都心 2〜3、郊外の市街地 4、田舎 5〜6、星空で有名な場所 6.5（星表に入っているのは 6.5 等まで）。

## シナリオを書く

`scenario.steps` に場面を並べる。各場面は `duration_sec` 秒続き、**書いた項目だけ**を変える（書かない項目は前の場面のまま）。

```yaml
scenario:
  loop: true                 # 最後まで行ったら最初に戻る
  steps:
    - { duration_sec: 20, location: achi, time: "2026-12-15 16:30", speed: 600, mode: real, message: "阿智村の冬の夕暮れ" }
    - { duration_sec: 20, speed: 60, constellations: true, labels: true, message: "冬の星座" }
```

| 項目 | 意味 |
|---|---|
| `duration_sec` | この場面を見せる実時間 [秒] |
| `location` / `time` / `speed` / `mode` | 観測地の id / 日時（地方時）/ 時間倍率 / モードの id |
| `constellations` / `constellation_names` / `labels` | 星座線 / 星座名 / 星・惑星の名前の表示（true / false） |
| `message` | 右下に 3 秒出す文字 |

場面が変わるたびに 1 秒かけてフェードする。`P` キーで再生・停止、`--play` で起動と同時に再生。

上映用の項目（字幕 `subtitle`、季節の目印 `asterisms`、指す円 `point`、`fade_sec`、`eye_adapt_sec`、
`meteor_rate`・`meteor_radiant`、`credits`）もある。台本だけを別のファイルに書いて `--scenario` で読むこともできる。
例と説明は [shows/autumn_2026.md](../shows/autumn_2026.md)。

よくある変更：

- プロジェクタにつなぐ → `output: projector`、`projector.monitor_index`（起動ログの `monitor[n]` の番号）
- 壁に映す → `projection.type: perspective`、`fov_deg: 70`、`center_alt_deg: 30`
- 天吊りで映像が上下逆さま → プロジェクタ本体の天吊り設定、または `projection.rotation_deg: 180`
- スクリーンの裏から映す（リア投影）で左右が逆 → `projector.flip_horizontal: true`
- 映像の北が部屋の北と合わない → `projection.rotation_deg` で回す（`C` キーで方位を表示して合わせる）
- 星が暗い／明るすぎる → `render.brightness`、`render.gamma`
