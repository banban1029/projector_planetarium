#include "modes.hpp"

const std::vector<ModeInfo>& allModes() {
    static const std::vector<ModeInfo> modes = {
        {"real", "リアル", false, {0.00f, 0.01f, 0.04f}},
        {"aurora", "オーロラ", true, {0.00f, 0.12f, 0.08f}},
        {"nebula", "星雲", true, {0.10f, 0.02f, 0.12f}},
        {"meteor", "流星群", true, {0.02f, 0.03f, 0.10f}},
        {"constellation", "星座絵", true, {0.08f, 0.06f, 0.02f}},
        {"jewel", "宝石箱", true, {0.10f, 0.02f, 0.06f}},
        {"warp", "ワープ航行", true, {0.00f, 0.06f, 0.14f}},
        {"milkyway", "天の川", true, {0.06f, 0.06f, 0.10f}},
        {"mirror", "水鏡", true, {0.00f, 0.08f, 0.12f}},
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
