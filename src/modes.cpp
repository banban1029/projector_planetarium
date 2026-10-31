#include "modes.hpp"

namespace {
ModeStyle look(float saturation, float size, float brightness, float twinkle, float glow, float r, float g, float b,
               float bloom) {
    ModeStyle s;
    s.saturation = saturation;
    s.size = size;
    s.brightness = brightness;
    s.twinkle = twinkle;
    s.glow = glow;
    s.tint[0] = r;
    s.tint[1] = g;
    s.tint[2] = b;
    s.bloom = bloom;
    return s;
}

std::vector<ModeInfo> buildModes() {
    // look(彩度, 大きさ, 明るさ, 瞬き, 光芒, 空の色 r, g, b, にじみ)
    ModeStyle real = look(0.55f, 1.0f, 1.0f, 0.12f, 0.3f, 0, 0, 0, 0.15f);
    real.realisticSky = true;
    real.meteorRate = 0.02f;  // 散在流星（数分に 1 個）

    ModeStyle aurora = look(0.9f, 1.1f, 1.2f, 0.12f, 0.5f, 0.0f, 0.004f, 0.003f, 0.4f);
    aurora.aurora = 1.0f;
    aurora.milkyWay = 0.6f;

    ModeStyle nebula = look(1.3f, 1.2f, 1.3f, 0.10f, 0.8f, 0.003f, 0.0f, 0.005f, 0.5f);
    nebula.nebula = 1.0f;
    nebula.milkyWay = 0.8f;

    ModeStyle meteor = look(0.8f, 1.0f, 1.1f, 0.10f, 0.4f, 0.0f, 0.002f, 0.008f, 0.5f);
    meteor.meteorRate = 2.5f;

    ModeStyle constellation = look(0.7f, 1.1f, 1.2f, 0.08f, 0.5f, 0.005f, 0.004f, 0.0f, 0.4f);
    constellation.constellationArt = true;
    constellation.milkyWay = 0.5f;

    ModeStyle jewel = look(1.8f, 1.6f, 1.6f, 0.5f, 1.2f, 0.006f, 0.0f, 0.004f, 0.8f);
    jewel.spikes = 1.0f;
    jewel.milkyWay = 0.5f;

    ModeStyle warp = look(1.0f, 1.0f, 0.35f, 0.0f, 0.6f, 0.0f, 0.004f, 0.012f, 0.6f);
    warp.warp = true;
    warp.milkyWay = 0.3f;

    ModeStyle milkyway = look(0.9f, 1.0f, 1.3f, 0.10f, 0.6f, 0.002f, 0.002f, 0.005f, 0.5f);
    milkyway.milkyWay = 4.0f;

    ModeStyle mirror = look(1.0f, 1.1f, 1.2f, 0.20f, 0.6f, 0.0f, 0.004f, 0.008f, 0.4f);
    mirror.mirror = true;

    return {
        {"real", "リアル", real},
        {"aurora", "オーロラ", aurora},
        {"nebula", "星雲", nebula},
        {"meteor", "流星群", meteor},
        {"constellation", "星座絵", constellation},
        {"jewel", "宝石箱", jewel},
        {"warp", "ワープ航行", warp},
        {"milkyway", "天の川", milkyway},
        {"mirror", "水鏡", mirror},
    };
}
}  // namespace

const std::vector<ModeInfo>& allModes() {
    static const std::vector<ModeInfo> modes = buildModes();
    return modes;
}

size_t findMode(const std::string& id) {
    const auto& modes = allModes();
    for (size_t i = 0; i < modes.size(); ++i) {
        if (id == modes[i].id) return i;
    }
    return 0;
}
