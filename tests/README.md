# tests/

天文計算・時刻・季節・シナリオ・キー操作の自動テスト。

| ファイル | 内容 |
|---|---|
| [astro_test.cpp](astro_test.cpp) | [src/astro.cpp](../src/astro.cpp) のテスト（ユリウス日、恒星時、歳差、太陽位置、地平座標、大気差）と、Astronomy Engine との比較 |
| [controls_test.cpp](controls_test.cpp) | すべてのキー操作（GLFW のキーコールバックと同じ `handleKey()` を呼ぶ。Shift の組み合わせ、押しっぱなし、ウィンドウ無しでも落ちないこと） |
| [verify/](verify/README.md) | すべての検証をまとめて実行するスクリプト（Python）。結果は `build/verify/report.md` |
| [clock_test.cpp](clock_test.cpp) | 時刻（文字列の読み書き、倍率・逆再生・一時停止）、季節の巡回・直接ジャンプ・自動巡回、シナリオ再生のテスト |

## 実行

MSYS2 Bash・PowerShell どちらでも同じ：

```sh
cmake --build --preset mingw-debug
ctest --preset mingw-debug
```

各項目の結果を見たいときはテストの実行ファイルを直接動かす：

```sh
./build/debug/astro_test.exe       # MSYS2 Bash
```

```powershell
.\build\debug\astro_test.exe       # PowerShell
```

```
[ OK ] GMST Meeus 12.a              actual=197.693195 expected=197.693195 diff=9.09e-08
[ OK ] precession RA  Meeus 21.b    actual=41.547214 expected=41.547213 diff=1.37e-06
...
PASSED (0 failures)
```

## 期待値の出典

- Jean Meeus『Astronomical Algorithms』第 2 版の例題（12.a 恒星時、21.b 歳差、25.a 太陽）
- 幾何学的に答えが決まる場合（南中、東の地平線、天の北極の高度 = 緯度）
- 独立した実装：Astronomy Engine（[third_party/](../third_party/README.md)）。4 星 × 3 日時で高度・方位の差が 0.02° 以内
- 時刻・季節・シナリオは、手で計算できる日付（例：10/31 から次の季節 → 翌年 1/20 の冬）

テスト項目と許容誤差の一覧は [docs/astronomy.md §9](../docs/astronomy.md#9-検証)。

## テストを追加するとき

`check("名前", 実際の値, 期待値, 許容誤差)` を `main()` に足す。
これらに加えて、実際のウィンドウでのキー操作・動き・描画速度・Stellarium との比較・目視用の画像までを
まとめて行うスクリプトが [verify/](verify/README.md) にある（`python tests/verify/verify.py`）。検証の記録は [docs/verification.md](../docs/verification.md)。

新しいテストファイル `tests/xxx_test.cpp` を作る場合は、[CMakeLists.txt](../CMakeLists.txt) の `foreach(name astro clock)` に `xxx` を足す。
