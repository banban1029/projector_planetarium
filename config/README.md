# config/

実行時の設定（YAML）。**変更したら再起動するだけでよく、再ビルドは不要**。

| ファイル | 内容 |
|---|---|
| [projector.yaml](projector.yaml) | 既定の設定。起動時に引数で別のファイルを指定しなければこれを読む |

## 項目

| セクション | 内容 |
|---|---|
| `output` | `debug`（PC 画面にウィンドウ）/ `projector`（プロジェクタにフルスクリーン） |
| `projector` | プロジェクタの解像度（ML1050ST+ は 1280×800）、出力先モニタ番号、左右・上下反転、垂直同期 |
| `projection` | 投影方式（`fisheye` 天井向け / `perspective` 壁向け）、画角、視線の方向、回転 |
| `observer` | 観測地の緯度・経度・標高・タイムゾーン（既定は大阪大学吹田キャンパス） |
| `time` | 起動時の日時（`"now"` または `"2026-12-15 22:00"`）、時間倍率とその段階 |
| `seasons` | 季節プリセット（名前・月日・時刻）。並び順 = `S` キーで巡回する順 |
| `season_tour` | 季節の自動巡回（`T` キー）の ON/OFF と間隔 |
| `mode` | 起動時の表示モード（`real`, `aurora`, `nebula`, …） |
| `render` | 星の明るさ・大きさ、限界等級、大気減光、露出・ガンマ、グリッド・方位表示 |

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

よくある変更：

- プロジェクタにつなぐ → `output: projector`、`projector.monitor_index`（起動ログの `monitor[n]` の番号）
- 壁に映す → `projection.type: perspective`、`fov_deg: 70`、`center_alt_deg: 30`
- 天吊りで映像が上下逆さま → プロジェクタ本体の天吊り設定、または `projection.rotation_deg: 180`
- スクリーンの裏から映す（リア投影）で左右が逆 → `projector.flip_horizontal: true`
- 映像の北が部屋の北と合わない → `projection.rotation_deg` で回す（`C` キーで方位を表示して合わせる）
- 星が暗い／明るすぎる → `render.brightness`、`render.gamma`
