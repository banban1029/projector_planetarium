# data/

描画に使うデータ。

| ファイル | 内容 | git |
|---|---|---|
| [stars.csv](stars.csv) | 恒星表（6.5 等まで 8,920 星）。HYG Database から [scripts/fetch_hyg.py](../scripts/fetch_hyg.py) で作ったもの | 管理する |
| [constellation_lines.csv](constellation_lines.csv) | 星座線（線分 743 本, J2000）。d3-celestial から [scripts/fetch_constellations.py](../scripts/fetch_constellations.py) で作ったもの | 管理する |
| [constellation_names.csv](constellation_names.csv) | 星座名（89, 日本語・英語）と名前を置く位置・目立ちやすさ（rank） | 管理する |
| [star_names.csv](star_names.csv) | 主な星の日本語名（35 星）。HYG の英語名との対応表（このプロジェクトで作成。手で編集してよい） | 管理する |
| `raw/` | ダウンロードした元データ（`hygdata_v41.csv` 約 34 MB、`d3-celestial/*.json`） | 管理しない（`.gitignore`） |

## stars.csv

先頭行はコメント（出典とライセンス）、2 行目が列名。明るい順に並んでいる。

```
# Derived from HYG Database v4.1 (CC BY-SA 4.0) https://github.com/astronexus/HYG-Database
hip,ra_deg,dec_deg,mag,ci,con,proper
32349,101.287215,-16.716116,-1.44,0.009,CMa,Sirius
```

| 列 | 内容 |
|---|---|
| hip | Hipparcos 番号 |
| ra_deg, dec_deg | 赤経・赤緯 [度]（J2000.0） |
| mag | 実視等級 |
| ci | 色指数 B−V（空欄なら白） |
| con | 星座略符 |
| proper | 固有名 |

作り直す・より暗い星まで含める：

```sh
python scripts/fetch_hyg.py            # 6.5 等まで
python scripts/fetch_hyg.py --mag 7.5  # 7.5 等まで
```

## 星の名前を増やす

[star_names.csv](star_names.csv) に「英語名,日本語名」の行を足す。英語名は stars.csv の `proper` 列と完全に一致させる
（例：`Mirach,ミラク`）。再起動すれば `J` キーの名前表示に出る（再ビルド不要）。

## ライセンス

- HYG Database は **CC BY-SA 4.0**。stars.csv はその派生物なので同じライセンスに従い、先頭行のクレジットを消さないこと。
- d3-celestial は **BSD-3-Clause**。constellation_*.csv の先頭行の著作権表示を消さないこと。

詳細は [docs/data.md](../docs/data.md)。
