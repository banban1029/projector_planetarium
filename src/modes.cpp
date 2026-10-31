#include "modes.hpp"

const std::vector<ModeInfo>& allModes() {
    // 幻想モードの固有演出（オーロラ・流星など）は未実装。現在は星の見せ方と空の色味だけが変わる
    static const std::vector<ModeInfo> modes = {
        // id, 名前, {realistic, 彩度, 大きさ, 明るさ, 瞬き, 光芒, 色味}
        {"real", "リアル", {true, 0.55f, 1.0f, 1.0f, 0.04f, 0.3f, {0.0f, 0.0f, 0.0f}}},
        {"aurora", "オーロラ", {false, 0.9f, 1.1f, 1.2f, 0.12f, 0.5f, {0.0f, 0.012f, 0.008f}}},
        {"nebula", "星雲", {false, 1.3f, 1.2f, 1.3f, 0.10f, 0.8f, {0.010f, 0.0f, 0.014f}}},
        {"meteor", "流星群", {false, 0.8f, 1.0f, 1.1f, 0.10f, 0.4f, {0.0f, 0.002f, 0.008f}}},
        {"constellation", "星座絵", {false, 0.7f, 1.1f, 1.2f, 0.08f, 0.5f, {0.008f, 0.006f, 0.0f}}},
        {"jewel", "宝石箱", {false, 1.8f, 1.6f, 1.6f, 0.50f, 1.2f, {0.008f, 0.0f, 0.006f}}},
        {"warp", "ワープ航行", {false, 1.0f, 1.0f, 1.4f, 0.0f, 0.6f, {0.0f, 0.004f, 0.012f}}},
        {"milkyway", "天の川", {false, 0.9f, 1.0f, 1.3f, 0.10f, 0.6f, {0.006f, 0.006f, 0.012f}}},
        {"mirror", "水鏡", {false, 1.0f, 1.1f, 1.2f, 0.20f, 0.6f, {0.0f, 0.008f, 0.012f}}},
    };
    return modes;
}

size_t findMode(const std::string& id) {
    const auto& modes = allModes();
    for (size_t i = 0; i < modes.size(); ++i) {
        if (id == modes[i].id) return i;
    }
    return 0;
}
