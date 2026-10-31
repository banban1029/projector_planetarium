"""上映の BGM（ロマンチックで静かな曲）を作曲して WAV（ステレオ）に書く。標準ライブラリだけで動く。

    python scripts/make_bgm.py build/shows/autumn_2026/bgm.wav

ヘ長調、8 つの和音（1 つ 8 秒、64 秒で 1 周）。
- やわらかいピアノの分散和音（アルペジオ）が、ゆっくり上って下りる
- その上で、オルゴールのような高い音が短い旋律を歌う
- 下では、あたたかいストリングスのような和音（パッド）が鳴り続ける
- 全体に残響（リバーブ）をかけ、左右に少し広げる
最後と最初がつながるので繰り返し再生できる。乱数は使わないので、何度作っても同じ曲になる。
作るのに 1 分ほどかかる。
"""

import array
import math
import sys
import wave
from pathlib import Path

RATE = 22050
BEAT = 0.5          # 8 分音符の長さ [秒]（4 分音符 = 60 BPM）
CHORD_SEC = 8.0     # 和音 1 つの長さ（16 拍）


def hz(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


# 和音（MIDI 番号。低音 → 上の音）: Fmaj7 → Em7 → Am9 → Dm9 → B♭maj7 → Am7 → Gsus4 → C(add9)
CHORDS = [
    [41, 53, 57, 60, 64],
    [40, 52, 55, 59, 62],
    [45, 52, 55, 59, 60],
    [38, 50, 53, 57, 64],
    [46, 53, 57, 62, 65],
    [45, 52, 55, 60, 64],
    [43, 50, 55, 60, 62],
    [48, 52, 55, 60, 62],
]
# 旋律（和音ごとに [拍, MIDI, 長さ（拍）]）。高いオルゴールの音で、間をたっぷりとる
MELODY = [
    [(0, 81, 3), (4, 79, 1), (5, 77, 2), (8, 76, 6)],
    [(0, 79, 3), (4, 76, 1), (5, 74, 2), (8, 71, 6)],
    [(0, 72, 2), (2, 76, 2), (4, 79, 4), (10, 83, 4)],
    [(0, 81, 6), (8, 77, 2), (10, 76, 2), (12, 74, 4)],
    [(0, 74, 3), (4, 77, 1), (5, 81, 2), (8, 82, 6)],
    [(0, 81, 3), (4, 79, 1), (5, 76, 2), (8, 72, 6)],
    [(0, 74, 4), (4, 72, 2), (6, 74, 2), (8, 79, 6)],
    [(0, 76, 8), (10, 74, 2), (12, 72, 4)],
]
# アルペジオで弾く順番（和音の上 4 音の番号。上って下りる 8 拍を 2 回）
ARP = [1, 2, 3, 4, 3, 2, 3, 2]


def add(buf, start, samples, gain, pan):
    """モノラルの音をステレオのバッファに足す（1 周の端で折り返す。pan: -1 左〜1 右）"""
    n = len(buf) // 2
    left = gain * math.cos((pan + 1) * math.pi / 4)
    right = gain * math.sin((pan + 1) * math.pi / 4)
    j = int(start * RATE) % n
    for v in samples:
        buf[2 * j] += left * v
        buf[2 * j + 1] += right * v
        j += 1
        if j == n:
            j = 0


_cache = {}


def piano(midi, length=3.0):
    """やわらかいピアノ風の音: 倍音ごとに減衰の速さを変え、高い倍音ほど早く消える"""
    key = ('piano', midi)
    if key not in _cache:
        f = hz(midi)
        out = []
        for i in range(int(length * RATE)):
            t = i / RATE
            v = 0.0
            for h, amp, decay in ((1, 1.0, 1.6), (2, 0.35, 0.9), (3, 0.12, 0.5)):
                v += amp * math.exp(-t / decay) * math.sin(2 * math.pi * f * h * 1.0006 ** (h - 1) * t)
            out.append(v * min(1.0, t / 0.006) * min(1.0, (length - t) / 0.3))
        _cache[key] = out
    return _cache[key]


def musicbox(midi, beats):
    """オルゴール風の高い音: 澄んだ正弦波と、少しずれた倍音"""
    key = ('box', midi, beats)
    if key not in _cache:
        f = hz(midi)
        length = beats * BEAT + 2.5
        out = []
        for i in range(int(length * RATE)):
            t = i / RATE
            env = math.exp(-t / 1.4) * min(1.0, t / 0.004) * min(1.0, (length - t) / 0.5)
            out.append(env * (math.sin(2 * math.pi * f * t) + 0.18 * math.sin(2 * math.pi * f * 4.02 * t)))
        _cache[key] = out
    return _cache[key]


def pad(chord):
    """ストリングス風のパッド: 少しずつ音程をずらした正弦波を重ね、ゆっくり出入りする"""
    length = CHORD_SEC + 3.0
    out = [0.0] * int(length * RATE)
    for k, midi in enumerate(chord):
        f = hz(midi)
        amp = 0.5 if k == 0 else 0.22
        for detune in (0.997, 1.0, 1.003):
            w = 2 * math.pi * f * detune / RATE
            for i in range(len(out)):
                out[i] += amp * math.sin(w * i + k)
    for i in range(len(out)):
        t = i / RATE
        out[i] *= min(1.0, t / 2.0) ** 2 * min(1.0, (length - t) / 2.5) ** 2
    return out


def reverb(buf):
    """シュレーダー型の残響（くし形フィルタ 4 つ + オールパス 2 つ）。左右で遅れを変えて広がりを出す。
    1 周を 2 回つなげて処理し、後半を使う（繰り返したときに残響が途切れない）"""
    n = len(buf) // 2
    out = array.array('f', bytes(4 * 2 * n))
    for ch, combs, allps in ((0, (1116, 1188, 1277, 1356), (556, 441)),
                             (1, (1139, 1211, 1300, 1379), (579, 464))):
        x = [buf[2 * (i % n) + ch] for i in range(2 * n)]
        wet = [0.0] * (2 * n)
        for d in combs:
            d = d * RATE // 44100 * 2
            line = [0.0] * d
            p = 0
            low = 0.0
            for i in range(2 * n):
                y = line[p]
                low = 0.7 * y + 0.3 * low  # 高い音ほど早く消える
                line[p] = x[i] + 0.84 * low
                p = p + 1 if p + 1 < d else 0
                wet[i] += y
        for d in allps:
            d = d * RATE // 44100
            line = [0.0] * d
            p = 0
            for i in range(2 * n):
                b = line[p]
                v = wet[i] + 0.5 * b
                line[p] = v
                wet[i] = b - 0.5 * v
                p = p + 1 if p + 1 < d else 0
        for i in range(n):
            out[2 * i + ch] = 0.75 * x[n + i] + 0.09 * wet[n + i]
    return out


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else 'bgm.wav')
    out.parent.mkdir(parents=True, exist_ok=True)
    total_sec = CHORD_SEC * len(CHORDS)
    n = int(total_sec * RATE)
    buf = array.array('f', bytes(4 * 2 * n))

    for ci, chord in enumerate(CHORDS):
        t0 = ci * CHORD_SEC
        add(buf, t0 - 1.5, pad(chord), 0.05, 0.0)
        # ピアノ: 低音を 1 拍目に、上の音を 8 分音符で上り下り
        add(buf, t0, piano(chord[0] - 12 if chord[0] > 40 else chord[0], 4.0), 0.10, -0.2)
        for b in range(16):
            midi = chord[ARP[b % 8]]
            # 2 回目は 1 オクターブ上で、少し弱く
            add(buf, t0 + b * BEAT, piano(midi + (12 if b >= 8 else 0)), 0.075 if b < 8 else 0.055,
                -0.4 + 0.8 * (ARP[b % 8] - 1) / 3)
        for beat, midi, beats in MELODY[ci]:
            add(buf, t0 + beat * BEAT, musicbox(midi, beats), 0.05, 0.3)

    print('reverb ...')
    buf = reverb(buf)
    peak = max(abs(x) for x in buf) or 1.0
    gain = 0.95 / peak  # 音量はアプリ側（台本の bgm_volume）で決める
    pcm = array.array('h', (int(max(-1.0, min(1.0, x * gain)) * 32767) for x in buf))
    with wave.open(str(out), 'wb') as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(pcm.tobytes())
    print(f'wrote {out} ({total_sec:.0f} s loop, stereo)')


if __name__ == '__main__':
    main()
