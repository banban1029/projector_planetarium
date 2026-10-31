# .vscode/

VS Code の設定（このプロジェクト専用）。

| ファイル | 内容 |
|---|---|
| [settings.json](settings.json) | CMake Tools にプリセット（[CMakePresets.json](../CMakePresets.json)）を使わせる、MSYS2 の CMake を使う、IntelliSense に `compile_commands.json` を使う、ターミナルの PATH に `C:\msys64\mingw64\bin` を足す |
| [launch.json](launch.json) | `Ctrl+F5` / F5 で gdb デバッグ実行するときの設定 |

## 使い方

- `F7`：ビルド
- `Shift+F5`：デバッガなしで実行
- `Ctrl+F5`：gdb でデバッグ実行（ブレークポイントが使える）

初回は下のステータスバーでプリセット「MSYS2 MinGW64 Debug」を選ぶ。
詳しくは [README.md](../README.md) の「方法 A」。
