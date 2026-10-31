"""上映の BGM（静かな環境音）を作曲して WAV に書く。標準ライブラリだけで動く。

    python scripts/make_bgm.py build/shows/autumn_2026/bgm.wav

ゆっくり移り変わる 4 つの和音（1 つ 16 秒、4 秒かけて重なりながら切り替わる）と、
まれに鳴る高い鈴のような音。64 秒で 1 周し、最後と最初がつながるので繰り返し再生できる。
乱数の種を固定しているので、何度作っても同じ曲になる。
"""

import array
import math
import random
import struct
import sys
import wave
from pathlib import Path

RATE = 22050
CHORD_SEC = 16.0
OVERLAP_SEC = 4.0
# 和音（周波数 [Hz]）: Cmaj9 → Am9 → Fmaj7 → G6sus。低めでやわらかく
CHORDS = [
    [130.81, 196.00, 246.94, 293.66, 329.63],
    [110.00, 164.81, 196.00, 246.94, 261.63],
    [87.31, 174.61, 220.00, 261.63, 329.63],
    [98.00, 146.83, 196.00, 220.00, 293.66],
]
BELL_NOTES = [523.25, 587.33, 659.25, 783.99, 880.00, 1046.50]  # ペンタトニック


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else 'bgm.wav')
    out.parent.mkdir(parents=True, exist_ok=True)
    total_sec = CHORD_SEC * len(CHORDS)
    n = int(total_sec * RATE)
    buf = array.array('f', bytes(4 * n))

    # 和音のパッド: 各音を 2 本の少しずらした正弦波で鳴らし、ゆっくり揺らす
    for ci, chord in enumerate(CHORDS):
        start = ci * CHORD_SEC - OVERLAP_SEC / 2
        length = CHORD_SEC + OVERLAP_SEC
        for k, f in enumerate(chord):
            amp = 0.06 / (1 + 0.25 * k)
            for detune in (0.998, 1.002):
                w = 2 * math.pi * f * detune / RATE
                lfo = 2 * math.pi * (0.07 + 0.013 * k) / RATE
                for i in range(int(length * RATE)):
                    t = i / RATE
                    # 上がり・下がりは OVERLAP_SEC かけて sin² で
                    if t < OVERLAP_SEC:
                        env = math.sin(0.5 * math.pi * t / OVERLAP_SEC) ** 2
                    elif t > length - OVERLAP_SEC:
                        env = math.sin(0.5 * math.pi * (length - t) / OVERLAP_SEC) ** 2
                    else:
                        env = 1.0
                    j = (int(start * RATE) + i) % n  # 1 周の端で折り返してつなげる
                    buf[j] += amp * env * (0.8 + 0.2 * math.sin(lfo * i + k)) * math.sin(w * i + k * 1.7)

    # 鈴の音: 1 周に 10 回、ランダムな時刻に短く鳴らす
    rng = random.Random(20261102)
    for _ in range(10):
        f = rng.choice(BELL_NOTES)
        at = int(rng.uniform(0.5, total_sec - 5.0) * RATE)
        w = 2 * math.pi * f / RATE
        for i in range(int(4.0 * RATE)):
            env = math.exp(-i / (0.9 * RATE)) * min(1.0, i / (0.01 * RATE))
            buf[(at + i) % n] += 0.035 * env * (math.sin(w * i) + 0.3 * math.sin(2.76 * w * i))

    peak = max(abs(x) for x in buf) or 1.0
    gain = 0.5 / peak  # 余裕を持たせる（音量はアプリ側で決める）
    pcm = array.array('h', (int(max(-1.0, min(1.0, x * gain)) * 32767) for x in buf))
    with wave.open(str(out), 'wb') as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(pcm.tobytes())
    print(f'wrote {out} ({total_sec:.0f} s loop)')


if __name__ == '__main__':
    main()
