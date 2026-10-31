#pragma once
#include <string>
#include <vector>

// 星と空の見せ方。星の位置計算は全モード共通で、これだけがモードごとに変わる
struct ModeStyle {
    bool realisticSky;  // true: 太陽高度による薄明・大気減光あり / false: 常に夜空
    float saturation;   // 星の色の彩度（1 = 黒体色そのまま）
    float size;         // 星像の大きさの倍率
    float brightness;   // 星の明るさの倍率
    float twinkle;      // 瞬きの振幅 [mag]
    float glow;         // 明るい星の光芒
    float tint[3];      // 空に足す色（線形 sRGB）
};

struct ModeInfo {
    const char* id;
    const char* name;
    ModeStyle style;
};

// index 0 がリアルモード、1〜 が幻想モード（数字キーと対応）
const std::vector<ModeInfo>& allModes();
// id から index を引く。見つからなければ 0（リアル）
size_t findMode(const std::string& id);
