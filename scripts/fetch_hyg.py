"""HYG 星表をダウンロードし、描画用の軽量 CSV (data/stars.csv) に変換する。

    python scripts/fetch_hyg.py               # 6.5 等まで
    python scripts/fetch_hyg.py --mag 7.5     # より暗い星まで

出力列: hip, ra_deg, dec_deg, mag, ci, con, proper
  ra/dec : J2000.0 分点・元期（度）
  mag    : 実視等級 V
  ci     : 色指数 B-V（不明な星は空欄）
  con    : 星座略符（Ori など）
  proper : 固有名（Sirius など、無ければ空欄）

HYG Database: https://github.com/astronexus/HYG-Database (CC BY-SA 4.0)
"""

import argparse
import csv
import urllib.request
from pathlib import Path

URL = "https://raw.githubusercontent.com/astronexus/HYG-Database/main/hyg/CURRENT/hygdata_v41.csv"
ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "data" / "raw" / "hygdata_v41.csv"
OUT = ROOT / "data" / "stars.csv"


def download():
    if RAW.exists():
        return
    RAW.parent.mkdir(parents=True, exist_ok=True)
    print(f"downloading {URL}")
    urllib.request.urlretrieve(URL, RAW)


def convert(mag_limit):
    rows = []
    with RAW.open(newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            if r["proper"] == "Sol":
                continue
            mag = float(r["mag"])
            if mag > mag_limit:
                continue
            rows.append((
                r["hip"],
                f"{float(r['ra']) * 15.0:.6f}",  # 時 -> 度
                f"{float(r['dec']):.6f}",
                f"{mag:.2f}",
                r["ci"],
                r["con"],
                r["proper"].replace(",", " "),
            ))
    rows.sort(key=lambda row: float(row[3]))  # 明るい順
    with OUT.open("w", newline="", encoding="utf-8") as f:
        f.write("# Derived from HYG Database v4.1 (CC BY-SA 4.0) https://github.com/astronexus/HYG-Database\n")
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["hip", "ra_deg", "dec_deg", "mag", "ci", "con", "proper"])
        w.writerows(rows)
    print(f"wrote {len(rows)} stars (mag <= {mag_limit}) to {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--mag", type=float, default=6.5, help="limiting magnitude")
    args = parser.parse_args()
    download()
    convert(args.mag)
