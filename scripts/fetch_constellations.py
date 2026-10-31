"""星座線・星座名（d3-celestial）をダウンロードし、描画用の CSV に変換する。

    python scripts/fetch_constellations.py

出力:
  data/constellation_lines.csv : con, ra1_deg, dec1_deg, ra2_deg, dec2_deg（線分 1 本 = 1 行, J2000）
  data/constellation_names.csv : con, ra_deg, dec_deg, rank, ja, en（名前を置く位置, J2000）

d3-celestial: https://github.com/ofrohn/d3-celestial (BSD-3-Clause, Copyright (c) 2015 Olaf Frohn)
"""

import csv
import json
import urllib.request
from pathlib import Path

BASE = "https://raw.githubusercontent.com/ofrohn/d3-celestial/master/data/"
ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "data" / "raw" / "d3-celestial"
CREDIT = "# Derived from d3-celestial (BSD-3-Clause, (c) 2015 Olaf Frohn) https://github.com/ofrohn/d3-celestial\n"


def load(name):
    path = RAW / name
    if not path.exists():
        RAW.mkdir(parents=True, exist_ok=True)
        print(f"downloading {BASE + name}")
        urllib.request.urlretrieve(BASE + name, path)
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def ra360(lon):
    # d3-celestial の経度は -180..180（赤経 > 180° は負）
    return lon % 360.0


def main():
    lines = load("constellations.lines.json")
    with (ROOT / "data" / "constellation_lines.csv").open("w", newline="", encoding="utf-8") as f:
        f.write(CREDIT)
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["con", "ra1_deg", "dec1_deg", "ra2_deg", "dec2_deg"])
        count = 0
        for feat in lines["features"]:
            for poly in feat["geometry"]["coordinates"]:
                for (a, b) in zip(poly, poly[1:]):
                    w.writerow([feat["id"], f"{ra360(a[0]):.4f}", f"{a[1]:.4f}", f"{ra360(b[0]):.4f}", f"{b[1]:.4f}"])
                    count += 1
    print(f"wrote {count} line segments")

    names = load("constellations.json")
    with (ROOT / "data" / "constellation_names.csv").open("w", newline="", encoding="utf-8") as f:
        f.write(CREDIT)
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["con", "ra_deg", "dec_deg", "rank", "ja", "en"])
        for feat in names["features"]:
            p = feat["properties"]
            lon, lat = feat["geometry"]["coordinates"]
            w.writerow([feat["id"], f"{ra360(lon):.4f}", f"{lat:.4f}", p.get("rank", "3"), p["ja"], p["en"]])
    print(f"wrote {len(names['features'])} constellation names")


if __name__ == "__main__":
    main()
