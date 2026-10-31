# tests/verify/

これまでに行った検証を、**1 つのコマンドでまとめて実行**するスクリプト。
結果は `build/verify/report.md`（表とメモ）と、目で確かめるための一覧画像にまとめる。

```sh
python tests/verify/verify.py            # すべて（約 2〜3 分）
python tests/verify/verify.py --quick    # 描画速度と Stellarium を省く（約 1.5 分）
python tests/verify/verify.py --only keys,motion
python tests/verify/verify.py --no-build # ビルド済みのものを使う
```

MSYS2 Bash でも PowerShell でも同じコマンド（Python の標準ライブラリだけを使う）。
1 つでも FAIL があれば終了コード 1、最後の行に `RESULT: PASSED` / `RESULT: FAILED (...)` を出す。
WARN は「確かめられなかったが失敗ではない」（例：Windows のスマート アプリ コントロールがテストの exe の実行を止めた）。理由と対処が report.md に書かれる。

| ファイル | 内容 |
|---|---|
| [verify.py](verify.py) | 検証の本体 |
| [pngtools.py](pngtools.py) | PNG の読み書き、フレームの比較、一覧画像（コンタクトシート）作り |

## 検証の内容

| 名前（`--only`） | 内容 | 合格の条件 |
|---|---|---|
| `build` | Debug・Release を CMake でビルド | 成功し、警告が無い（third_party を除く） |
| `unit` | `ctest`（astro / clock / controls） | 全項目合格 |
| `keys` | アプリを起動し、実際のウィンドウに Windows のキー入力（WM_KEYDOWN / WM_KEYUP）を約 30 回送る | キーごとに、期待したログの行が 3 秒以内に出る（全画面でカーソルを隠す、を含む）。起動時にスリープを止めている。Esc で終了する |
| `motion` | 640×400 で連続撮影（`--record`）し、フレームを比べる | 瞬き（宝石箱）変化した画素 ≥ 1%、瞬き（リアル）平均差 ≥ 0.05、オーロラ ≥ 0.2%、流れ星 5 間隔中 3 以上で変化、フェードは中間の明るさのフレームが 2 枚以上 |
| `benchmark` | Release、1280×800、垂直同期なしで 10 条件を 3 秒ずつ | すべて 60 fps 以上 |
| `stellarium` | Stellarium にスクリプトで同じ日時・場所を設定し、16 天体 × 2 場面の高度・方位を出させ、`--dump-positions` と比べる | 恒星 ≤ 60″、太陽・惑星 ≤ 30″、月 ≤ 30″（魚眼の 1 画素 ≒ 810″）。Stellarium が無ければ SKIP |
| `show` | 上映の台本 `shows/autumn_2026.yaml` を読み、9 場面を `--seek` で撮る。大阪 → 阿智村の切り替えを実時間で撮る | 名前がすべて引ける、長さが 270〜330 秒、阿智村に着いてから星が増えていく |
| `screenshots` | シーン 8 枚（テストパターン・クレジットを含む）・モード 8 枚・操作パネルを撮り、一覧画像にする | 撮れること（見た目はレポートの説明を見て人か AI が確かめる） |

## 出力（build/verify/、git 管理外）

| ファイル | 内容 |
|---|---|
| `report.md` | 結果の表、各検証の数値、目で確かめる画像とその見方 |
| `sheet_scenes.png` | シーン 8 枚の一覧（星座と名前、大阪、阿智村の夏、夕方、満月、透視投影、テストパターン、クレジット） |
| `operator_panel.png` | 操作パネル |
| `sheet_show.png`、`show_arrival.png` | 上映の場面、大阪から阿智村への切り替え |
| `sheet_modes.png` | 幻想モード 8 種の一覧 |
| `motion_*.png` | オーロラ・流れ星・フェードの連続写真 |
| `screenshots/`、`motion/` | 1 枚ずつの画像 |
| `keys.log`、`stellarium/` | キー操作のログ、Stellarium の作業フォルダ（スクリプトと output.txt） |

実行のたびに `build/verify/` を消して作り直す。

## 検証を足すとき

`verify.py` に `check_xxx(r)` を書き、`CHECKS` と `main()` の `titles`・`funcs` に足す。
`r.fail("理由")` で FAIL、`r.skip("理由")` で SKIP、`r.info("文")` でレポートに書く。
画像の一覧は `r.images.append((ファイル名, 画像のリスト, 列数, 見るポイント))`。

合格の条件（しきい値）は、実際に測った値に余裕を持たせて決めている（[docs/verification.md](../../docs/verification.md)）。
FAIL を消すためにしきい値を緩めるのではなく、原因を調べること。
