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

`proper` は星の日本語名の表示（下の「主な星の日本語名」）で使う。`con` は今は使っていない。

### なぜ 6.5 等までか
- 肉眼で見える限界がおよそ 6〜6.5 等（暗い空の場合）。
- リアルモードの既定 `limiting_magnitude: 6.0` に、フェード用の 0.5 等を足した範囲。
- 1000 lm のプロジェクタでは、6 等より暗い星は部屋の暗さ次第でほとんど見えない。

## 星座線・星座名：d3-celestial

| 項目 | 内容 |
|---|---|
| 出典 | [ofrohn/d3-celestial](https://github.com/ofrohn/d3-celestial) の `data/constellations.lines.json`、`data/constellations.json` |
| ライセンス | **BSD-3-Clause**（Copyright (c) 2015 Olaf Frohn。著作権表示を残す） |
| 内容 | 88 星座（へび座は頭と尾で 2 つ、計 89）の線と、名前を置く位置・日本語名・目立ちやすさ（rank 1〜3） |

[scripts/fetch_constellations.py](../scripts/fetch_constellations.py) で CSV に変換する（元データは `data/raw/d3-celestial/`）。

```sh
python scripts/fetch_constellations.py
```

| ファイル | 列 |
|---|---|
| [data/constellation_lines.csv](../data/constellation_lines.csv) | con, ra1_deg, dec1_deg, ra2_deg, dec2_deg（線分 743 本, J2000） |
| [data/constellation_names.csv](../data/constellation_names.csv) | con, ra_deg, dec_deg, rank, ja, en |

名前は rank 1〜2 の星座だけ表示する（暗い星座まで出すと画面が埋まるため）。星座絵モードでは全部出す。

## 主な星の日本語名

[data/star_names.csv](../data/star_names.csv)（このプロジェクトで作成）。HYG の英語名（`proper` 列）と日本語名の対応表で、
1 等星を中心に 35 星。ベガ・アルタイルには「織姫星」「彦星」、プレアデス星団の中心アルキオネには「すばる」を付けた。
起動時に stars.csv の英語名と照合して位置を決める。

## 天の川

画像データは使わず、銀河座標の式とノイズで描く（[rendering.md §9](rendering.md#9-天の川)）。

## 月・惑星

位置の計算に [Astronomy Engine](https://github.com/cosinekitty/astronomy)（MIT）を使う。データファイルは不要（[astronomy.md §7.2](astronomy.md)）。

## フォント

Windows に標準で入っている日本語フォント（BIZ UDゴシックなど）を実行時に読む。フォントファイルはリポジトリに含めない。
