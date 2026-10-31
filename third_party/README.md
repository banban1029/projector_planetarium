# third_party/

ソースごと同梱している外部ライブラリ。どちらもビルドに CMake の静的ライブラリとして組み込む（[CMakeLists.txt](../CMakeLists.txt)）。
MSYS2 の pacman で入れるライブラリ（GLFW・GLEW・GLM・yaml-cpp）はここには置かない。

| フォルダ | ライブラリ | 用途 | ライセンス |
|---|---|---|---|
| [astronomy/](astronomy/) | [Astronomy Engine](https://github.com/cosinekitty/astronomy)（C 版 `astronomy.c` / `astronomy.h`） | 太陽・月・惑星の位置・等級・満ち欠け。テストでは自作の座標変換の検証にも使う | MIT（[astronomy/LICENSE](astronomy/LICENSE)） |
| [stb/](stb/) | [stb](https://github.com/nothings/stb) の `stb_truetype.h`・`stb_image_write.h` | 日本語フォントの読み込み（文字のアトラス）、スクリーンショットの PNG 保存 | MIT またはパブリックドメイン（[stb/LICENSE](stb/LICENSE)） |

- `stb/stb_impl.c` は、ヘッダオンリーの stb の実装部分を 1 回だけコンパイルするためのファイル（このプロジェクトで作成）。
- 取得元：各リポジトリの master ブランチ（2026-10-31 に取得）。更新するときは同じ URL から取り直す。

```sh
curl -L -o third_party/astronomy/astronomy.c https://raw.githubusercontent.com/cosinekitty/astronomy/master/source/c/astronomy.c
curl -L -o third_party/astronomy/astronomy.h https://raw.githubusercontent.com/cosinekitty/astronomy/master/source/c/astronomy.h
curl -L -o third_party/stb/stb_truetype.h    https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h
curl -L -o third_party/stb/stb_image_write.h https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
```
