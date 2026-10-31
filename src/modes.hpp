#pragma once
#include <string>
#include <vector>

// 表示モードの一覧。星の位置計算は全モード共通で、「見せ方」だけが変わる。
// 描画実装ができるまでは placeholder の背景色で切り替えを確認する。
struct ModeInfo {
    const char* id;
    const char* name;
    bool fantasy;
    float placeholder[3];
};

// index 0 がリアルモード、1〜 が幻想モード（数字キーと対応）
const std::vector<ModeInfo>& allModes();
// id から index を引く。見つからなければ 0（リアル）
size_t findMode(const std::string& id);
