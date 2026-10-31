# データ

## 恒星：HYG Database

| 項目 | 内容 |
|---|---|
| 出典 | [astronexus/HYG-Database](https://github.com/astronexus/HYG-Database)（`hyg/CURRENT/hygdata_v41.csv`） |
| 元データ | Hipparcos 星表、Yale 輝星星表、Gliese 近傍星表を統合したもの（約 12 万星） |
| ライセンス | **CC BY-SA 4.0**（クレジット表示・同じライセンスでの再配布が必要） |
| 座標 | J2000.0 分点・元期の赤経・赤緯 |

### 変換

[scripts/fetch_hyg.py](../scripts/fetch_hyg.py) が元データ（約 34 MB）を `data/raw/` にダウンロードし、
描画に必要な列だけを残した [data/stars.csv](../data/stars.csv) を作る。

```sh
python scripts/fetch_hyg.py            # 6.5 等まで（8,920 星、約 390 KB）
python scripts/fetch_hyg.py --mag 7.5  # より暗い星まで
```

- `data/raw/` は git 管理外（大きいため）。`data/stars.csv` はリポジトリに含める（ビルドしてすぐ動くように）。
- 太陽（Sol）は除く。明るい順に並べる。
- 先頭行のコメントに出典とライセンスを書く（CC BY-SA の表示義務）。

### data/stars.csv の列

| 列 | 内容 | 例（シリウス） |
|---|---|---|
| hip | Hipparcos 番号（無い星は空） | 32349 |
| ra_deg | 赤経 [度]（J2000） | 101.287215 |
| dec_deg | 赤緯 [度]（J2000） | −16.716116 |
| mag | 実視等級 V | −1.44 |
| ci | 色指数 B−V（不明なら空 → 白で描く） | 0.009 |
| con | 星座略符 | CMa |
| proper | 固有名 | Sirius |

`con` と `proper` は今は使っていない。星座線・星名表示（マイルストーン 6）で使う。

### なぜ 6.5 等までか
- 肉眼で見える限界がおよそ 6〜6.5 等（暗い空の場合）。
- リアルモードの既定 `limiting_magnitude: 6.0` に、フェード用の 0.5 等を足した範囲。
- 1000 lm のプロジェクタでは、6 等より暗い星は部屋の暗さ次第でほとんど見えない。

## 今後追加するデータ

| データ | 予定する出典 | ライセンス |
|---|---|---|
| 星座線・星座名 | [Stellarium skycultures](https://github.com/Stellarium/stellarium-skycultures) | 要確認（文化ごとに異なる） |
| 天の川 | ESA Gaia の全天画像 など | 配布元の条件に従う |
| 月・惑星 | [Astronomy Engine](https://github.com/cosinekitty/astronomy)（計算ライブラリ） | MIT |
