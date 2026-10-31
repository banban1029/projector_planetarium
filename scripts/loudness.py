"""WAV（16bit）の音量を gain 倍にする。割れないように先読みリミッタで大きいところだけ抑える。標準ライブラリだけで動く。

    python scripts/loudness.py step_01.wav 5        その場で書き換える

単純に 5 倍すると波形の頭が切れて割れるので、
「gain 倍したときに天井（CEILING）を超えるところ」だけ、少し前から音量を下げ、ゆっくり戻す。
小さい音は gain 倍、大きい音は天井で揃う（放送の音声のように、聞き取りやすく大きくなる）。
"""

import array
import math
import sys
import wave
from collections import deque

CEILING = 0.85     # 最大の振幅（BGM と重なっても割れないよう少し余裕を残す）
LOOKAHEAD = 0.004  # 先読み [秒]。この時間かけて音量を下げ始める
RELEASE = 0.06     # 下げた音量を戻す時定数 [秒]


def limit(x, rate, gain):
    n = len(x)
    la = max(1, int(LOOKAHEAD * rate))
    # 各サンプルで許せる倍率（gain 倍して天井を超えないための倍率）
    need = [min(gain, CEILING / abs(v)) if v else gain for v in x]
    # 先読み: 窓 [i, i+la] の最小値（単調キュー）
    win = [0.0] * n
    q = deque()
    for i in range(n + la):
        if i < n:
            while q and need[q[-1]] >= need[i]:
                q.pop()
            q.append(i)
        j = i - la
        if j >= 0:
            while q[0] < j:
                q.popleft()
            win[j] = need[q[0]]
    # 下げるのはすぐ、戻すのはゆっくり
    rel = math.exp(-1.0 / (RELEASE * rate))
    g = gain
    out = array.array('f', bytes(4 * n))
    for i in range(n):
        g = win[i] if win[i] < g else win[i] + (g - win[i]) * rel
        out[i] = max(-CEILING, min(CEILING, x[i] * g))
    return out


def main():
    path, gain = sys.argv[1], float(sys.argv[2])
    with wave.open(path) as w:
        params = w.getparams()
        if params.sampwidth != 2:
            sys.exit(f'{path}: 16bit WAV only')
        pcm = array.array('h', w.readframes(params.nframes))
    x = [v / 32768.0 for v in pcm]
    y = limit(x, params.framerate * params.nchannels, gain)
    with wave.open(path, 'wb') as w:
        w.setparams(params)
        w.writeframes(array.array('h', (int(v * 32767) for v in y)).tobytes())


if __name__ == '__main__':
    main()
