#pragma once
#include <string>

// シェーダ・データ・設定を置いたフォルダ（末尾の / なし）。次の順に探す:
//   1. 環境変数 PLANETARIUM_ROOT
//   2. 実行ファイルと同じフォルダ（cmake --install で作った配布フォルダ）
//   3. ビルド時のソースフォルダ（開発中）
std::string resourceRoot();
