# tests/

天文計算の自動テスト。

| ファイル | 内容 |
|---|---|
| [astro_test.cpp](astro_test.cpp) | [src/astro.cpp](../src/astro.cpp) のテスト（ユリウス日、恒星時、歳差、太陽位置、地平座標、大気差） |

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

テスト項目と許容誤差の一覧は [docs/astronomy.md §9](../docs/astronomy.md#9-検証)。

## テストを追加するとき

`check("名前", 実際の値, 期待値, 許容誤差)` を `main()` に足す。
新しいテストファイルを作る場合は [CMakeLists.txt](../CMakeLists.txt) に `add_executable` と `add_test` を追加する。
