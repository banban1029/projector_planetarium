# scripts/

補助スクリプト。アプリ本体の実行には不要（結果の CSV はリポジトリに含まれている）。

| スクリプト | 内容 |
|---|---|
| [fetch_hyg.py](fetch_hyg.py) | HYG 星表をダウンロード（`data/raw/`）し、描画用の `data/stars.csv` に変換する |
| [fetch_constellations.py](fetch_constellations.py) | d3-celestial の星座データをダウンロードし、`data/constellation_lines.csv`・`data/constellation_names.csv` に変換する |

## fetch_hyg.py

Python 3 の標準ライブラリだけで動く（追加のパッケージ不要）。MSYS2 の Python（`C:\msys64\mingw64\bin\python.exe`）で動作確認済み。

```sh
python scripts/fetch_hyg.py              # 6.5 等まで（既定）
python scripts/fetch_hyg.py --mag 7.5    # 限界等級を変える
```

- 元データ（約 34 MB）は初回だけダウンロードする。`data/raw/hygdata_v41.csv` があれば再利用する。
- 太陽を除き、指定等級までの星を明るい順に書き出す。赤経は時 → 度に変換する。
- 出力の列は [data/README.md](../data/README.md) を参照。

MSYS2 Bash でも PowerShell でも同じコマンドで実行できる（PowerShell では `scripts\fetch_hyg.py` と書いてもよい）。

## fetch_constellations.py

```sh
python scripts/fetch_constellations.py
```

- 元データ（`constellations.lines.json`・`constellations.json`）は初回だけ `data/raw/d3-celestial/` にダウンロードする。
- 星座線は折れ線を 2 点ずつの線分に分け、赤経を 0〜360° に直す（元データは −180〜180°）。
- 星座名は日本語名（`ja`）と英語名、名前を置く位置、目立ちやすさ（rank 1〜3）を書き出す。


検証のスクリプトは [tests/verify/](../tests/verify/README.md) にある。
