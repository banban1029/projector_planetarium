#pragma once
#include <string>
#include <vector>

// 星と空の見せ方。星の位置計算は全モード共通で、これだけがモードごとに変わる
struct ModeStyle {
    bool realisticSky = false;  // true: 太陽高度による薄明・大気減光・月明かり・光害あり / false: 常に暗い夜空
    float saturation = 1.0f;    // 星の色の彩度（1 = 黒体色そのまま）
    float size = 1.0f;          // 星像の大きさの倍率
    float brightness = 1.0f;    // 星の明るさの倍率
    float twinkle = 0.1f;       // 瞬きの振幅 [mag]
    float glow = 0.5f;          // 明るい星の光芒
    float tint[3] = {0, 0, 0};  // 空に足す色（線形 sRGB）
    float bloom = 0.4f;         // 光のにじみ
    // 演出（0 = なし）
    float milkyWay = 1.0f;      // 天の川の明るさの倍率（リアルモードは空の暗さでさらに弱まる）
    float aurora = 0.0f;        // オーロラ
    float nebula = 0.0f;        // 星雲
    float meteorRate = 0.0f;    // 流れ星の数 [個/秒]
    bool constellationArt = false;  // 星座線と星座名を光らせて常に表示
    float spikes = 0.0f;        // 明るい星の十字の光条
    bool warp = false;          // 星が光跡になって流れる
    bool mirror = false;        // 地平線の下に星空が水面のように映る（魚眼は画角を広げる）
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
