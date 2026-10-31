// 起動・メインループ。
// 毎フレーム: シミュレーション時刻を進める -> 天文計算（地平座標への変換行列・太陽・月・惑星）
//             -> 演出（流れ星・ワープ）と文字を積む -> 描画
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "astro.hpp"
#include "audio.hpp"
#include "bodies.hpp"
#include "config.hpp"
#include "constellations.hpp"
#include "controls.hpp"
#include "effects.hpp"
#include "guide.hpp"
#include "labels.hpp"
#include "modes.hpp"
#include "operator_window.hpp"
#include "paths.hpp"
#include "sky_renderer.hpp"
#include "star_catalog.hpp"
#include "stb_image_write.h"

namespace {
// コマンドライン: planetarium [config.yaml] [--time "YYYY-MM-DD HH:MM"] [--mode id] [--location id]
//   [--grid] [--compass] [--constellations] [--constellation-names] [--labels] [--info] [--play] [--operator]
//   [--test-pattern] [--credits]
//   [--scenario shows/xxx.yaml]（上映の台本を読む）[--seek 秒]（台本をその時点まで進めてから始める。確認用）
//   [--dump-narration]（台本の読み上げ文を表示して終了。scripts/make_show_audio.ps1 が使う）
//   [--list-monitors]（つながっているモニタを表示して終了）
//   [--output debug|projector] [--monitor 番号|名前]（出力先を設定ファイルより優先して指定）
//   [--keep-vsync]（--benchmark でも垂直同期を切らない。実際の上映と同じ速度を測る）
//   [--screenshot out.png|out.ppm [--wait 秒]]（描いて画像を保存し終了。検証・資料用）
//   [--record 名前 --count N --interval 秒]（連続撮影: 名前_000.png ...。動きの確認用）
//   [--benchmark 秒]（垂直同期を切って描画速度を測り終了）
//   [--dump-positions]（主な天体の高度・方位を表示して終了。Stellarium との比較用）
struct Options {
    std::string configPath;
    std::string time, mode, location, screenshot, record, scenario;
    double wait = 0.0, interval = 0.5, benchmark = 0.0, seek = 0.0;
    int count = 8;
    bool dumpPositions = false, dumpNarration = false, listMonitors = false, keepVsync = false;
    std::string output, monitor;  // --output / --monitor（設定ファイルより優先）
    std::string dumpNames, root;
    bool grid = false, compass = false, constellations = false, constellationNames = false, labels = false;
    bool info = false, play = false, operatorWindow = false, testPattern = false, credits = false;
};

Options parseArgs(int argc, char** argv, const std::string& root) {
    Options o;
    o.configPath = root + "/config/projector.yaml";
    o.root = root;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool hasValue = i + 1 < argc;
        if (a == "--time" && hasValue) o.time = argv[++i];
        else if (a == "--mode" && hasValue) o.mode = argv[++i];
        else if (a == "--location" && hasValue) o.location = argv[++i];
        else if (a == "--screenshot" && hasValue) o.screenshot = argv[++i];
        else if (a == "--wait" && hasValue) o.wait = std::stod(argv[++i]);
        else if (a == "--record" && hasValue) o.record = argv[++i];
        else if (a == "--count" && hasValue) o.count = std::stoi(argv[++i]);
        else if (a == "--interval" && hasValue) o.interval = std::stod(argv[++i]);
        else if (a == "--benchmark" && hasValue) o.benchmark = std::stod(argv[++i]);
        else if (a == "--dump-positions") o.dumpPositions = true;
        else if (a == "--dump-narration") o.dumpNarration = true;
        else if (a == "--list-monitors") o.listMonitors = true;
        else if (a == "--output" && hasValue) o.output = argv[++i];
        else if (a == "--monitor" && hasValue) o.monitor = argv[++i];
        else if (a == "--keep-vsync") o.keepVsync = true;
        else if (a == "--dump-names" && hasValue) o.dumpNames = argv[++i];
        else if (a == "--grid") o.grid = true;
        else if (a == "--compass") o.compass = true;
        else if (a == "--constellations") o.constellations = true;
        else if (a == "--constellation-names") o.constellationNames = true;
        else if (a == "--labels") o.labels = true;
        else if (a == "--info") o.info = true;
        else if (a == "--play") o.play = true;
        else if (a == "--operator") o.operatorWindow = true;
        else if (a == "--scenario" && hasValue) o.scenario = argv[++i];
        else if (a == "--seek" && hasValue) o.seek = std::stod(argv[++i]);
        else if (a == "--test-pattern") o.testPattern = true;
        else if (a == "--credits") o.credits = true;
        else o.configPath = a;
    }
    return o;
}

// 画像を PNG（拡張子 .png）または binary PPM で保存する。texture を渡せばそのテクスチャ、
// 0 なら今のコンテキストの既定フレームバッファを読む（出力画像は texture から読む。
// 画面への転送直後に既定フレームバッファを読むと、ドライバによっては 1 つ前のフレームが返るため）
void saveScreenshot(const std::string& path, int w, int h, GLuint texture = 0) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (texture) {
        glGetTextureImage(texture, 0, GL_RGB, GL_UNSIGNED_BYTE, static_cast<GLsizei>(px.size()), px.data());
    } else {
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    }
    bool ok;
    if (path.size() > 4 && path.substr(path.size() - 4) == ".png") {
        stbi_flip_vertically_on_write(1);
        ok = stbi_write_png(path.c_str(), w, h, 3, px.data(), w * 3) != 0;
    } else {
        FILE* f = std::fopen(path.c_str(), "wb");
        ok = f != nullptr;
        if (f) {
            std::fprintf(f, "P6\n%d %d\n255\n", w, h);
            for (int y = h - 1; y >= 0; --y) std::fwrite(&px[static_cast<size_t>(y) * w * 3], 1, w * 3, f);
            std::fclose(f);
        }
    }
    std::printf(ok ? "saved %s\n" : "cannot write %s\n", path.c_str());
}

SkyState computeSky(const SimClock& clock, const ObserverConfig& obs, const std::vector<BodyInfo>& bodies) {
    const double jd = clock.julianDate();
    SkyState sky{};
    sky.j2000ToHorizon = astro::j2000ToHorizon(jd, obs.latitude_deg, obs.longitude_deg);
    // 太陽は日付の分点で求まるので、歳差を含まない赤道 -> 地平の変換を使う
    double ra, dec, az;
    astro::sunEquatorial(jd, ra, dec);
    const double lst = astro::greenwichMeanSiderealDeg(jd) + obs.longitude_deg;
    sky.sunHorizon = astro::equatorialToHorizon(lst, obs.latitude_deg) * astro::equatorialVector(ra, dec);
    astro::altAz(sky.sunHorizon, sky.sunAltDeg, az);
    sky.nightMagLimit = obs.limiting_magnitude;
    sky.moonAltDeg = -90.0;
    for (const auto& b : bodies) {
        if (b.kind != BodyInfo::Moon) continue;
        astro::altAz(sky.j2000ToHorizon * b.dirJ2000, sky.moonAltDeg, az);
        sky.moonPhaseFraction = b.phaseFraction;
    }
    return sky;
}

// 上映の案内（季節の目印の線・指す円）を線に足し、その名前を labels に足す
void addGuide(const SkyGuide& guide, const AppState& app, const std::vector<BodyInfo>& bodies, double now,
              std::vector<LineVertex>& lines, std::vector<LabelRequest>& labels) {
    const double t = now - app.show.stepTime;
    const float fadeIn = static_cast<float>(std::clamp(t / 0.8, 0.0, 1.0));
    const float gold[4] = {1.0f, 0.82f, 0.45f, fadeIn}, white[4] = {0.9f, 0.96f, 1.0f, fadeIn};
    for (const auto& id : app.show.asterisms) {
        if (const auto* a = guide.asterism(id)) {
            guide.appendAsterism(lines, *a, t / 2.0, 0.9f * fadeIn);  // 2 秒かけて線が順に伸びる
            labels.push_back({TextRenderer::SkyJ2000, a->center, a->nameJa, 20.0f, {gold[0], gold[1], gold[2], gold[3]},
                              -1.0f, false});
        }
    }
    for (const auto& name : app.show.point) {
        astro::Vec3 dir;
        std::string display;
        if (!guide.find(name, bodies, dir, display)) continue;
        // 大きな円から縮んで止まり、ゆっくり明滅する
        const double radius = 2.2 + 4.0 * std::exp(-t * 3.0);
        const float alpha = fadeIn * static_cast<float>(0.75 + 0.25 * std::sin(now * 3.0));
        guide.appendPointer(lines, dir, radius, alpha);
        labels.push_back({TextRenderer::SkyJ2000, dir, display, 16.0f, {white[0], white[1], white[2], white[3]},
                          -0.5f, true});
    }
}

// 台本の名前（目印の id・星や惑星の名前）がすべて引けるかを確かめ、引けないものを表示する
int checkShowNames(const Scenario& scenario, const SkyGuide& guide, const std::vector<BodyInfo>& bodies) {
    int unknown = 0;
    for (const auto& st : scenario.steps) {
        for (const auto& id : st.asterisms) {
            if (!guide.asterism(id)) {
                std::fprintf(stderr, "[show] unknown asterism: %s\n", id.c_str());
                ++unknown;
            }
        }
        for (const auto& name : st.point) {
            astro::Vec3 dir;
            std::string display;
            if (!guide.find(name, bodies, dir, display)) {
                std::fprintf(stderr, "[show] unknown name: %s\n", name.c_str());
                ++unknown;
            }
        }
    }
    return unknown;
}

std::string narrationOf(const Scenario& scenario, size_t index) {
    return index < scenario.steps.size() ? narrationText(scenario.steps[index]) : std::string();
}

int landscapeId(const std::string& name) { return name == "mountains" ? 1 : name == "city" ? 2 : 0; }

ViewSettings viewFromConfig(const Config& cfg) {
    ViewSettings v;
    v.fisheye = cfg.projection.type == "fisheye";
    v.fovDeg = cfg.projection.fov_deg;
    v.centerAltDeg = cfg.projection.center_alt_deg;
    v.centerAzDeg = cfg.projection.center_az_deg;
    v.rotationDeg = cfg.projection.rotation_deg;
    v.flipH = cfg.projector.flip_horizontal;
    v.flipV = cfg.projector.flip_vertical;
    return v;
}

// output: projector なら指定モニタにフルスクリーン、debug（または未接続）ならウィンドウ
// プロジェクタのモニタを選ぶ: monitor_name を名前に含むもの → monitor_index → 見つからなければ nullptr
GLFWmonitor* findProjectorMonitor(const ProjectorConfig& pc, int& index) {
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (!pc.monitor_name.empty()) {
        std::string want = pc.monitor_name;
        std::transform(want.begin(), want.end(), want.begin(), ::tolower);
        for (int i = 0; i < count; ++i) {
            std::string name = glfwGetMonitorName(monitors[i]);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name.find(want) != std::string::npos) {
                index = i;
                return monitors[i];
            }
        }
    }
    // Windows のドライバによっては名前が "Generic PnP Monitor" になるので、
    // 次に「主モニタ以外で、今の解像度がプロジェクタの解像度のもの」を探す
    for (int i = 0; i < count; ++i) {
        const GLFWvidmode* m = glfwGetVideoMode(monitors[i]);
        if (monitors[i] != glfwGetPrimaryMonitor() && m->width == pc.width && m->height == pc.height) {
            index = i;
            return monitors[i];
        }
    }
    if (pc.monitor_index >= 0 && pc.monitor_index < count && monitors[pc.monitor_index] != glfwGetPrimaryMonitor()) {
        index = pc.monitor_index;
        return monitors[index];
    }
    return nullptr;
}

// つながっているモニタを表示する（--list-monitors と起動時のログ）
void printMonitors(const ProjectorConfig& pc) {
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    int projector = -1;
    findProjectorMonitor(pc, projector);
    for (int i = 0; i < count; ++i) {
        const GLFWvidmode* m = glfwGetVideoMode(monitors[i]);
        int x, y, mmW, mmH, modeCount;
        glfwGetMonitorPos(monitors[i], &x, &y);
        glfwGetMonitorPhysicalSize(monitors[i], &mmW, &mmH);
        const GLFWvidmode* modes = glfwGetVideoModes(monitors[i], &modeCount);
        bool native = false;
        for (int k = 0; k < modeCount; ++k) native |= modes[k].width == pc.width && modes[k].height == pc.height;
        std::printf("monitor[%d]: %s %dx%d@%dHz pos %d,%d size %dx%dmm%s%s%s\n", i, glfwGetMonitorName(monitors[i]),
                    m->width, m->height, m->refreshRate, x, y, mmW, mmH,
                    monitors[i] == glfwGetPrimaryMonitor() ? " primary" : "", i == projector ? " projector" : "",
                    native ? " supports-projector-resolution" : "");
    }
}

// output: projector ならプロジェクタのモニタにフルスクリーン、debug（または未接続）ならウィンドウ
GLFWwindow* createOutputWindow(const Config& cfg) {
    const auto& pc = cfg.projector;
    printMonitors(pc);
    GLFWmonitor* target = nullptr;
    if (cfg.output == "projector") {
        int index = -1;
        target = findProjectorMonitor(pc, index);
        if (target) {
            glfwWindowHint(GLFW_REFRESH_RATE, pc.refresh_rate);
            // 全画面のウィンドウは、他のウィンドウ（操作パネル）を触ると最小化されるのが既定。上映が消えないように切る
            glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
            std::printf("[output] fullscreen on monitor %d (%s) %dx%d@%dHz\n", index, glfwGetMonitorName(target),
                        pc.width, pc.height, pc.refresh_rate);
        } else {
            std::printf("[output] projector not found, using windowed mode\n");
        }
    }
    return glfwCreateWindow(pc.width, pc.height, "Projector Planetarium", target, nullptr);
}

size_t findLocation(const Config& cfg, const std::string& id) {
    for (size_t i = 0; i < cfg.locations.size(); ++i) {
        if (cfg.locations[i].id == id) return i;
    }
    if (!id.empty()) std::fprintf(stderr, "unknown location '%s', using the first one\n", id.c_str());
    return 0;
}

// 天球上の名前・方位（重ならないように配置）と、画面表示・通知を文字として積む
void addTexts(TextRenderer& text, const AppState& app, const ModeStyle& style, const ConstellationData& cons,
              const std::vector<SkyLabel>& starNames, const std::vector<BodyInfo>& bodies,
              const Projection& projection, const astro::Mat3& j2000ToHorizon, int fbW, int fbH, double now,
              std::vector<LabelRequest> labels) {
    const float px = static_cast<float>(std::min(fbW, fbH)) / 800.0f;
    const auto& o = app.overlays;
    for (auto& l : labels) l.sizePx *= px;  // 案内の名前（呼び出し側で 800px 基準の大きさを入れる）
    auto request = [&](TextRenderer::Anchor anchor, const astro::Vec3& dir, const std::string& s, float size,
                       const float c[4], float priority, bool beside) {
        labels.push_back({anchor, dir, s, size * px, {c[0], c[1], c[2], c[3]}, priority, beside});
    };
    // 優先度: 方位 0 → 月・惑星 1 → 星（明るい順）2〜3 → 星座（主要なものから）4〜6
    if (o.compass) {
        const char* names[] = {"北", "東", "南", "西"};
        const float red[4] = {1.0f, 0.3f, 0.25f, 1.0f}, gray[4] = {0.7f, 0.75f, 0.85f, 1.0f};
        for (int i = 0; i < 4; ++i) {
            const double az = i * 90.0 * astro::kDeg, alt = 8.0 * astro::kDeg;
            const astro::Vec3 dir{std::cos(alt) * std::cos(az), std::cos(alt) * std::sin(az), std::sin(alt)};
            request(TextRenderer::SkyHorizon, dir, names[i], 26.0f, i == 0 ? red : gray, 0.0f, false);
        }
    }
    if (o.labels) {
        const float white[4] = {0.85f, 0.85f, 0.80f, 0.85f}, planet[4] = {1.0f, 0.85f, 0.55f, 0.95f};
        for (const auto& b : bodies) {
            if (b.kind == BodyInfo::Sun && !style.realisticSky) continue;
            request(TextRenderer::SkyJ2000, b.dirJ2000, b.nameJa, 14.0f, planet, 1.0f, true);
        }
        for (const auto& s : starNames) {
            request(TextRenderer::SkyJ2000, s.dirJ2000, s.text, 13.0f, white, 2.0f + s.mag * 0.2f, true);
        }
    }
    if (o.constellationNames || style.constellationArt) {
        const float blue[4] = {0.45f, 0.60f, 0.85f, 0.85f}, gold[4] = {1.0f, 0.80f, 0.45f, 0.95f};
        for (const auto& n : cons.names) {
            if (!style.constellationArt && n.rank > 2) continue;  // 目立たない星座は名前を省く
            request(TextRenderer::SkyJ2000, n.dirJ2000, n.text, 15.0f, style.constellationArt ? gold : blue,
                    3.0f + n.rank, false);
        }
    }
    placeLabels(text, std::move(labels), projection, j2000ToHorizon, fbW, fbH);

    // 画面の左下に日時など、右下に通知（魚眼では円の外の黒い部分に出る）
    const astro::Vec3 bottomLeft{-1.0, -1.0, 0.0}, bottomRight{1.0, -1.0, 0.0};
    const float margin = 16.0f * px;
    if (o.info) {
        const float c[4] = {0.75f, 0.78f, 0.85f, 0.9f};
        const auto lines = infoLines(app);
        for (size_t i = 0; i < lines.size(); ++i) {
            const float y = margin + (lines.size() - 1 - i) * 24.0f * px + 8.0f * px;
            text.add(TextRenderer::Screen, bottomLeft, lines[i], 18.0f * px, c, TextRenderer::Left, margin, y,
                     TextRenderer::Overlay);
        }
    }
    const double age = now - app.toastTime;
    if (!app.toast.empty() && age < 3.0) {
        const float c[4] = {0.85f, 0.88f, 0.95f, static_cast<float>(std::min(1.0, 3.0 - age))};
        text.add(TextRenderer::Screen, bottomRight, app.toast, 18.0f * px, c, TextRenderer::Right, -margin,
                 margin + 8.0f * px, TextRenderer::Overlay);
    }
    // 字幕: 画面下の中央。読みやすいよう黒い影を付ける。場面の始めに 0.6 秒でフェードイン
    if (!app.show.subtitle.empty()) {
        std::vector<std::string> lines;
        std::string rest = app.show.subtitle;
        for (auto p = rest.find('\n'); p != std::string::npos; p = rest.find('\n')) {
            lines.push_back(rest.substr(0, p));
            rest = rest.substr(p + 1);
        }
        lines.push_back(rest);
        const float a = static_cast<float>(std::clamp((now - app.show.stepTime) / 0.6, 0.0, 1.0));
        const float shadow[4] = {0.0f, 0.0f, 0.0f, 0.85f * a}, white[4] = {0.95f, 0.96f, 1.0f, a};
        const astro::Vec3 bottom{0.0, -1.0, 0.0};
        for (size_t i = 0; i < lines.size(); ++i) {
            const float y = (34.0f + (lines.size() - 1 - i) * 36.0f) * px;
            for (int k = 0; k < 2; ++k) {
                text.add(TextRenderer::Screen, bottom, lines[i], 25.0f * px, k == 0 ? shadow : white, TextRenderer::Center,
                         k == 0 ? 2.0f * px : 0.0f, k == 0 ? y - 2.0f * px : y, TextRenderer::Overlay);
            }
        }
    }
    if (o.credits) {
        // データ・ライブラリの出典（CC BY-SA などの表示義務）。LICENSE と README にも同じ内容を書いている
        const char* lines[] = {
            "クレジット",
            "",
            "恒星データ：HYG Database v4.1（astronexus）  CC BY-SA 4.0",
            "星座線・星座名：d3-celestial（Olaf Frohn）  BSD-3-Clause",
            "太陽・月・惑星の計算：Astronomy Engine（Don Cross）  MIT",
            "文字の描画・画像の保存：stb（Sean Barrett）  MIT / Public Domain",
            "",
            "Projector Planetarium  MIT License",
        };
        const float title[4] = {1.0f, 0.95f, 0.85f, 1.0f}, body[4] = {0.85f, 0.88f, 0.95f, 1.0f};
        const int n = static_cast<int>(std::size(lines));
        for (int i = 0; i < n; ++i) {
            text.add(TextRenderer::Screen, astro::Vec3{0, 0, 0}, lines[i], (i == 0 ? 30.0f : 19.0f) * px,
                     i == 0 ? title : body, TextRenderer::Center, 0.0f, ((n - 1) * 0.5f - i) * 34.0f * px,
                     TextRenderer::Overlay);
        }
    }
}
}  // namespace

// 主な天体の高度・方位（大気差なし）を表示する。Stellarium のスクリプトで出した値と比べるため
int dumpPositions(const Config& cfg, const Options& opts) {
    const auto& obs = cfg.locations[findLocation(cfg, opts.location.empty() ? cfg.location : opts.location)];
    SimClock clock(obs.timezone_hours, cfg.time.speed_steps, 1.0);
    if (!clock.setLocalString(cfg.time.start)) return 1;
    const double jd = clock.julianDate();
    const astro::Mat3 m = astro::j2000ToHorizon(jd, obs.latitude_deg, obs.longitude_deg);
    std::printf("# %s %s  lat %.4f lon %.4f  JD %.6f\n", obs.id.c_str(), clock.formatLocal().c_str(),
                obs.latitude_deg, obs.longitude_deg, jd);
    const struct {
        const char* name;
        double ra, dec;
    } stars[] = {{"Sirius", 101.287215, -16.716116},  {"Betelgeuse", 88.792939, 7.407064},
                 {"Rigel", 78.634467, -8.201638},     {"Capella", 79.172328, 45.997991},
                 {"Vega", 279.234600, 38.783692},     {"Polaris", 37.946250, 89.264109},
                 {"Aldebaran", 68.980163, 16.509302}, {"Deneb", 310.357980, 45.280339}};
    double alt, az;
    for (const auto& s : stars) {
        astro::altAz(m * astro::equatorialVector(s.ra, s.dec), alt, az);
        std::printf("%s %.4f %.4f\n", s.name, alt, az);
    }
    const char* english[] = {"Sun", "Moon", "Mercury", "Venus", "Mars", "Jupiter", "Saturn", "Uranus"};
    const auto bodies = computeBodies(jd, obs);
    for (size_t i = 0; i < bodies.size() && i < 8; ++i) {
        astro::altAz(m * bodies[i].dirJ2000, alt, az);
        std::printf("%s %.4f %.4f\n", english[i], alt, az);
    }
    // --dump-names: 名前で指定した星・惑星（上映の字幕が正しいかを確かめる）
    if (!opts.dumpNames.empty()) {
        const SkyGuide guide(opts.root + "/data");
        std::stringstream list(opts.dumpNames);
        std::string name;
        while (std::getline(list, name, ',')) {
            astro::Vec3 dir;
            std::string display;
            if (!guide.find(name, bodies, dir, display)) {
                std::printf("%s (unknown)\n", name.c_str());
                continue;
            }
            astro::altAz(m * dir, alt, az);
            std::printf("%s %.4f %.4f\n", name.c_str(), alt, az);
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // ログを即時表示する
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    const std::string root = resourceRoot();
    const Options opts = parseArgs(argc, argv, root);

    Config cfg;
    try {
        cfg = loadConfig(opts.configPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
    if (!opts.time.empty()) cfg.time.start = opts.time;
    if (!opts.scenario.empty()) {
        try {
            cfg.scenario = loadScenario(opts.scenario);
            double total = 0.0;
            for (const auto& st : cfg.scenario.steps) total += st.duration_sec;
            if (opts.dumpNarration) {
                // タブ区切り: 先頭の # 行が設定、続いて「場面番号 秒数 読み上げ文」
                const auto& a = cfg.scenario.audio;
                std::printf("#title\t%s\n#dir\t%s\n#voice\t%s\n#rate\t%d\n#gain\t%g\n#bgm\t%s\n#total\t%.0f\n",
                            cfg.scenario.title.c_str(), a.narrationDir.c_str(), a.voice.c_str(), a.rate,
                            a.narrationGain, a.bgm.c_str(), total);
                for (size_t i = 0; i < cfg.scenario.steps.size(); ++i) {
                    const auto& st = cfg.scenario.steps[i];
                    std::printf("%02zu\t%.1f\t%s\n", i + 1, st.duration_sec, narrationText(st).c_str());
                }
                return 0;
            }
            std::printf("[show] %s (%zu steps, %.0f s)\n", opts.scenario.c_str(), cfg.scenario.steps.size(), total);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s\n", e.what());
            return 1;
        }
    }
    if (!opts.screenshot.empty() || !opts.record.empty()) cfg.render.dither = false;  // 画像が圧縮しにくくなるので切る
    if (opts.benchmark > 0.0 && !opts.keepVsync) cfg.projector.vsync = false;
    if (!opts.output.empty()) cfg.output = opts.output;
    if (!opts.monitor.empty()) {
        // 数字なら番号、それ以外は名前の一部
        if (opts.monitor.find_first_not_of("0123456789") == std::string::npos) {
            cfg.projector.monitor_index = std::stoi(opts.monitor);
            cfg.projector.monitor_name.clear();
        } else {
            cfg.projector.monitor_name = opts.monitor;
        }
    }
    if (opts.dumpPositions) return dumpPositions(cfg, opts);

#ifdef _WIN32
    // 上映中に画面が消えたりスリープしたりしないようにする（終了すると元に戻る）
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    std::printf("[power] display sleep is prevented while running\n");
#endif
    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    if (opts.listMonitors) {
        printMonitors(cfg.projector);
        glfwTerminate();
        return 0;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = createOutputWindow(cfg);
    if (!window) {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(cfg.projector.vsync ? 1 : 0);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::fprintf(stderr, "glewInit failed\n");
        return 1;
    }
    std::printf("GL %s / %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    std::printf("resources: %s\n", root.c_str());

    const size_t location = findLocation(cfg, opts.location.empty() ? cfg.location : opts.location);
    const auto& obs = cfg.locations[location];
    std::printf("output: %s %dx%d, location: %s (%.2f, %.2f)\n", cfg.output.c_str(), cfg.projector.width,
                cfg.projector.height, obs.name.c_str(), obs.latitude_deg, obs.longitude_deg);

    // 時刻の表示は起動時の観測地のタイムゾーンで行う（日本国内の切り替えなら常に JST）
    AppState app{
        window,
        SimClock(obs.timezone_hours, cfg.time.speed_steps, cfg.time.speed),
        SeasonCycler(cfg.seasons, cfg.season_tour.interval_sec, cfg.season_tour.enabled),
        findMode(opts.mode.empty() ? cfg.mode : opts.mode),
        cfg.locations,
        location,
        std::make_unique<ScenarioPlayer>(cfg.scenario),
    };
    app.view = viewFromConfig(cfg);
    const auto& r = cfg.render;
    app.overlays.grid = r.show_grid || opts.grid;
    app.overlays.compass = r.show_compass || opts.compass;
    app.overlays.constellations = r.show_constellations || opts.constellations;
    app.overlays.constellationNames = r.show_constellation_names || opts.constellationNames;
    app.overlays.labels = r.show_labels || opts.labels;
    app.overlays.info = r.show_info || opts.info;
    app.overlays.testPattern = opts.testPattern;
    app.overlays.credits = opts.credits;
    if (!app.clock.setLocalString(cfg.time.start)) {
        std::fprintf(stderr, "invalid time.start: %s (using now)\n", cfg.time.start.c_str());
    }

    std::vector<StarVertex> stars;
    ConstellationData constellations;
    std::vector<SkyLabel> starNames;
    std::unique_ptr<SkyGuide> guide;
    std::unique_ptr<SkyRenderer> renderer;
    try {
        const std::string data = root + "/data";
        stars = loadStarCatalog(data + "/stars.csv");
        constellations = loadConstellations(data);
        starNames = loadStarNames(data);
        guide = std::make_unique<SkyGuide>(data);
        if (checkShowNames(cfg.scenario, *guide, computeBodies(app.clock.julianDate(), obs)) > 0) {
            std::fprintf(stderr, "[show] some names in the scenario are unknown (see above)\n");
        }
        std::printf("stars: %zu, constellation lines: %zu, names: %zu + %zu\n", stars.size(),
                    constellations.segments.size(), constellations.names.size(), starNames.size());
        renderer = std::make_unique<SkyRenderer>(root, stars, constellations, cfg.render);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // 使う文字を先にフォントから焼き込んでおく（起動後に新しい文字が出ると、その時に作り直す）
    TextRenderer& text = renderer->text();
    for (const auto& n : constellations.names) text.addCharacters(n.text);
    for (const auto& n : starNames) text.addCharacters(n.text);
    for (const auto& m : allModes()) text.addCharacters(m.name);
    for (const auto& l : cfg.locations) text.addCharacters(l.name);
    for (const auto& s : cfg.seasons) text.addCharacters(s.name);
    for (const auto& s : cfg.scenario.steps) text.addCharacters(s.message);
    text.addCharacters("北東南西太陽月水星金星火星木星土星天王星モード観測地季節時間逆再生一時停止現在時刻実"
                       "方位グリッド星座線名前魚眼天頂透視南シナリオ停止がありません自動巡回▶（）：");
    text.build();

    installControls(app);

    // 操作パネル（PC の画面に出す 2 つ目のウィンドウ）
    std::unique_ptr<OperatorWindow> operatorWindow;
    if (opts.operatorWindow || cfg.operator_window == "on" ||
        (cfg.operator_window == "auto" && cfg.output == "projector")) {
        try {
            operatorWindow = std::make_unique<OperatorWindow>(window, root, cfg.render.font);
            attachControls(operatorWindow->window(), app);
            std::printf("[operator] panel window opened\n");
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s\n", e.what());
        }
    }
    printHelp();
    if (opts.play) {
        if (const ScenarioStep* step = app.scenario->start()) applyScenarioStep(app, *step);
        // --seek: 描かずに台本と時計だけ進める（上映の途中の場面を確かめるため）
        for (double t = 0.0; t < opts.seek; t += 0.05) {
            app.clock.update(0.05);
            if (const ScenarioStep* step = app.scenario->update(0.05)) applyScenarioStep(app, *step);
        }
        if (opts.seek > 0.0) {
            app.show.stepTime = glfwGetTime() - 10.0;  // 線・円・字幕のアニメーションは終わった状態で見せる
            app.show.eyeAdaptStart = -1.0;
            app.transition = false;
            std::printf("[show] seek %.1f s -> step %zu\n", opts.seek, app.scenario->index() + 1);
        }
    }

    // 上映の音（台本に audio があるときだけ）。画像を撮るとき・速度を測るときは鳴らさない
    std::unique_ptr<AudioPlayer> audio;
    const auto& showAudio = cfg.scenario.audio;
    if (!showAudio.narrationDir.empty() || !showAudio.bgm.empty()) {
        audio = std::make_unique<AudioPlayer>();
        if (!opts.screenshot.empty() || !opts.record.empty() || opts.benchmark > 0.0) app.audioMuted = true;
    }
    auto showFile = [&](const std::string& relative) { return root + "/" + relative; };
    bool wasPlaying = false;

    MeteorShower meteors;
    std::vector<LineVertex> effectLines;
    const double start = glfwGetTime();
    double last = start;
    double lastTitle = -1.0;
    int frame = 0, recorded = 0, fpsFrames = 0;
    double fpsStart = start, recordStart = -1.0, benchStart = -1.0;
    int benchFrames = 0;
    // 操作パネルを閉じても終了する
    while (!glfwWindowShouldClose(window) && !(operatorWindow && glfwWindowShouldClose(operatorWindow->window()))) {
        const double now = glfwGetTime();
        const double dt = now - last;
        last = now;

        app.clock.update(dt);
        if (app.seasons.update(dt, app.clock)) {
            app.transition = true;
            showToast(app, "季節: " + app.seasons.currentName());
        }
        if (const ScenarioStep* step = app.scenario->update(dt)) applyScenarioStep(app, *step);
        if (audio) {
            // 再生を始めたら BGM、止めたら音を止める。場面が変わったらその場面のナレーション
            const bool playing = app.scenario->playing();
            if (playing && !wasPlaying && !showAudio.bgm.empty()) {
                if (std::filesystem::exists(showFile(showAudio.bgm))) {
                    audio->playBgm(showFile(showAudio.bgm), static_cast<float>(showAudio.bgmVolume),
                                   static_cast<float>(showAudio.bgmDuck));
                } else {
                    std::fprintf(stderr, "[audio] %s not found (run scripts/make_show_audio.ps1)\n", showAudio.bgm.c_str());
                }
            }
            if (!playing && wasPlaying) {
                audio->stopNarration();
                audio->stopBgm();
            }
            wasPlaying = playing;
            if (app.show.stepChanged && playing && !showAudio.narrationDir.empty()) {
                char name[32];
                std::snprintf(name, sizeof(name), "/step_%02zu.wav", app.show.stepIndex + 1);
                const std::string path = showFile(showAudio.narrationDir) + name;
                if (std::filesystem::exists(path)) {
                    audio->playNarration(path);
                } else if (!narrationOf(app.scenario->scenario(), app.show.stepIndex).empty()) {
                    std::fprintf(stderr, "[audio] %s not found (run scripts/make_show_audio.ps1)\n", path.c_str());
                }
            }
            app.show.stepChanged = false;
            audio->setMuted(app.audioMuted);
            audio->update(dt);
        }

        const ObserverConfig& here = app.locations[app.location];
        const auto bodies = computeBodies(app.clock.julianDate(), here);
        SkyState sky = computeSky(app.clock, here, bodies);
        // 目が暗さに慣れる演出: 見える星を 4 等級まで → その場所の限界まで、だんだん増やす
        if (app.show.eyeAdaptStart >= 0.0) {
            const double t = (now - app.show.eyeAdaptStart) / app.show.eyeAdaptSec;
            if (t < 1.0) {
                const double k = t <= 0.0 ? 0.0 : t * t * (3.0 - 2.0 * t);
                sky.eyeLimit = 4.0 + (here.limiting_magnitude - 4.0) * k;
            } else {
                app.show.eyeAdaptStart = -1.0;
            }
        }
        app.sunAltDeg = sky.sunAltDeg;
        const ModeStyle& style = allModes()[app.mode].style;

        if (now - lastTitle > 0.1) {
            glfwSetWindowTitle(window, statusLine(app).c_str());
            lastTitle = now;
        }

        // 演出の線: 流れ星（流星群モードはふたご座流星群の放射点から）とワープの光跡
        effectLines.clear();
        astro::Vec3 radiant{0, 0, 0};
        double meteorRate = app.show.meteorRate >= 0.0 ? app.show.meteorRate : style.meteorRate;
        if (app.show.hasRadiant) {
            radiant = sky.j2000ToHorizon * astro::equatorialVector(app.show.radiantRa, app.show.radiantDec);
        } else if (style.meteorRate > 1.0) {
            radiant = sky.j2000ToHorizon * astro::equatorialVector(112.0, 33.0);
        }
        if (radiant.z < 0.0) radiant = {0, 0, 0};
        meteors.update(dt, meteorRate, radiant);
        meteors.appendLines(effectLines);
        if (style.warp) {
            appendWarpStreaks(effectLines, stars, sky.j2000ToHorizon, viewForward(app.view),
                              app.view.fovDeg * 0.5 * astro::kDeg, now);
        }

        std::vector<LabelRequest> guideLabels;
        addGuide(*guide, app, bodies, now, effectLines, guideLabels);

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        ViewSettings drawView = app.view;  // 水鏡モードは画角が変わるので、名前の配置も同じ投影で計算する
        int landscape = landscapeId(here.landscape);
        if (style.mirror) applyMirror(drawView, landscape);
        addTexts(text, app, style, constellations, starNames, bodies,
                 makeProjection(drawView, fbW, fbH, landscape), sky.j2000ToHorizon, fbW, fbH, now,
                 std::move(guideLabels));
        if (app.transition) {
            renderer->startTransition(static_cast<float>(now), app.transitionSec);
            app.transition = false;
        }
        renderer->render({fbW, fbH, app.view, sky, style, static_cast<float>(now), app.overlays,
                          landscapeId(here.landscape), &bodies, &effectLines});
        if (opts.benchmark > 0.0) glFinish();  // GPU の処理が終わるまで待って、実際の描画時間を測る
        if (operatorWindow) {
            operatorWindow->render(renderer->outputTexture(), fbW, fbH, operatorLines(app));
            glfwMakeContextCurrent(window);
        }

        ++frame;
        if (!opts.screenshot.empty() && frame >= 3 && now - start >= opts.wait) {
            saveScreenshot(opts.screenshot, fbW, fbH, renderer->outputTexture());
            if (operatorWindow) {  // 操作パネルも「名前_operator.png」に保存する
                glfwMakeContextCurrent(operatorWindow->window());
                int ow, oh;
                operatorWindow->framebufferSize(ow, oh);
                const std::string base = opts.screenshot.substr(0, opts.screenshot.find_last_of('.'));
                saveScreenshot(base + "_operator.png", ow, oh);
                glfwMakeContextCurrent(window);
            }
            glfwSetWindowShouldClose(window, 1);
        }
        // 連続撮影の間隔は 1 枚目を撮った時刻から数える（起動直後のフレームが遅くても間隔がそろうように）
        if (!opts.record.empty() && frame >= 3 && now - start >= opts.wait &&
            (recordStart < 0.0 || now - recordStart >= recorded * opts.interval)) {
            if (recordStart < 0.0) recordStart = now;
            char name[512];
            std::snprintf(name, sizeof(name), "%s_%03d.png", opts.record.c_str(), recorded);
            std::printf("[record] frame %d at t = %.3f s\n", recorded, now - start);
            saveScreenshot(name, fbW, fbH, renderer->outputTexture());
            if (++recorded >= opts.count) glfwSetWindowShouldClose(window, 1);
        }
        // 描画速度: 1 秒ごとに平均 fps を更新。--benchmark では指定秒数の平均を表示して終了
        ++fpsFrames;
        if (now - fpsStart >= 1.0) {
            app.fps = fpsFrames / (now - fpsStart);
            fpsFrames = 0;
            fpsStart = now;
        }
        // 最初の 1 秒（全画面への切り替え・文字の準備）は数えず、その後 benchmark 秒を測る
        if (opts.benchmark > 0.0 && benchStart < 0.0 && now - start >= 1.0) {
            benchStart = now;
            benchFrames = 0;
        }
        if (benchStart >= 0.0) ++benchFrames;
        if (opts.benchmark > 0.0 && benchStart >= 0.0 && now - benchStart >= opts.benchmark) {
            const double elapsed = glfwGetTime() - benchStart;
            std::printf("benchmark: %d frames in %.2f s = %.1f fps (%.2f ms/frame), %dx%d, mode %s\n", benchFrames,
                        elapsed, benchFrames / elapsed, 1000.0 * elapsed / benchFrames, fbW, fbH,
                        allModes()[app.mode].id);
            glfwSetWindowShouldClose(window, 1);
        }
        if (operatorWindow) operatorWindow->present(window);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    audio.reset();
    operatorWindow.reset();
    renderer.reset();  // GL コンテキストが生きているうちに解放する
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
