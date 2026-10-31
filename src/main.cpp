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
#include <memory>
#include <string>
#include <vector>

#include "astro.hpp"
#include "bodies.hpp"
#include "config.hpp"
#include "constellations.hpp"
#include "controls.hpp"
#include "effects.hpp"
#include "labels.hpp"
#include "modes.hpp"
#include "paths.hpp"
#include "sky_renderer.hpp"
#include "star_catalog.hpp"
#include "stb_image_write.h"

namespace {
// コマンドライン: planetarium [config.yaml] [--time "YYYY-MM-DD HH:MM"] [--mode id] [--location id]
//   [--grid] [--compass] [--constellations] [--labels] [--info] [--play]
//   [--screenshot out.png|out.ppm [--wait 秒]]（描いて画像を保存し終了。検証・資料用）
//   [--record 名前 --count N --interval 秒]（連続撮影: 名前_000.png ...。動きの確認用）
//   [--benchmark 秒]（垂直同期を切って描画速度を測り終了）
//   [--dump-positions]（主な天体の高度・方位を表示して終了。Stellarium との比較用）
struct Options {
    std::string configPath;
    std::string time, mode, location, screenshot, record;
    double wait = 0.0, interval = 0.5, benchmark = 0.0;
    int count = 8;
    bool dumpPositions = false;
    bool grid = false, compass = false, constellations = false, labels = false, info = false, play = false;
};

Options parseArgs(int argc, char** argv, const std::string& root) {
    Options o;
    o.configPath = root + "/config/projector.yaml";
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
        else if (a == "--grid") o.grid = true;
        else if (a == "--compass") o.compass = true;
        else if (a == "--constellations") o.constellations = true;
        else if (a == "--labels") o.labels = true;
        else if (a == "--info") o.info = true;
        else if (a == "--play") o.play = true;
        else o.configPath = a;
    }
    return o;
}

// 既定フレームバッファを PNG（拡張子 .png）または binary PPM で保存する
void saveScreenshot(const std::string& path, int w, int h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
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
GLFWwindow* createOutputWindow(const Config& cfg) {
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    for (int i = 0; i < monitorCount; ++i) {
        const GLFWvidmode* m = glfwGetVideoMode(monitors[i]);
        std::printf("monitor[%d]: %s %dx%d@%dHz\n", i, glfwGetMonitorName(monitors[i]), m->width,
                    m->height, m->refreshRate);
    }
    const auto& pc = cfg.projector;
    GLFWmonitor* target = nullptr;
    if (cfg.output == "projector") {
        if (pc.monitor_index < monitorCount) {
            target = monitors[pc.monitor_index];
            glfwWindowHint(GLFW_REFRESH_RATE, pc.refresh_rate);
        } else {
            std::printf("monitor_index %d not found, using windowed mode\n", pc.monitor_index);
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
              const Projection& projection, const astro::Mat3& j2000ToHorizon, int fbW, int fbH, double now) {
    const float px = static_cast<float>(std::min(fbW, fbH)) / 800.0f;
    const auto& o = app.overlays;
    std::vector<LabelRequest> labels;
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
    if (o.labels || style.constellationArt) {
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
            text.add(TextRenderer::Screen, bottomLeft, lines[i], 18.0f * px, c, TextRenderer::Left, margin, y);
        }
    }
    const double age = now - app.toastTime;
    if (!app.toast.empty() && age < 3.0) {
        const float c[4] = {0.85f, 0.88f, 0.95f, static_cast<float>(std::min(1.0, 3.0 - age))};
        text.add(TextRenderer::Screen, bottomRight, app.toast, 18.0f * px, c, TextRenderer::Right, -margin,
                 margin + 8.0f * px);
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
    if (!opts.screenshot.empty() || !opts.record.empty()) cfg.render.dither = false;  // 画像が圧縮しにくくなるので切る
    if (opts.benchmark > 0.0) cfg.projector.vsync = false;
    if (opts.dumpPositions) return dumpPositions(cfg, opts);

    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
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
    app.overlays = {r.show_grid || opts.grid, r.show_compass || opts.compass,
                    r.show_constellations || opts.constellations, r.show_labels || opts.labels,
                    r.show_info || opts.info};
    if (!app.clock.setLocalString(cfg.time.start)) {
        std::fprintf(stderr, "invalid time.start: %s (using now)\n", cfg.time.start.c_str());
    }

    std::vector<StarVertex> stars;
    ConstellationData constellations;
    std::vector<SkyLabel> starNames;
    std::unique_ptr<SkyRenderer> renderer;
    try {
        const std::string data = root + "/data";
        stars = loadStarCatalog(data + "/stars.csv");
        constellations = loadConstellations(data);
        starNames = loadStarNames(data);
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
    printHelp();
    if (opts.play) {
        if (const ScenarioStep* step = app.scenario->start()) applyScenarioStep(app, *step);
    }

    MeteorShower meteors;
    std::vector<LineVertex> effectLines;
    const double start = glfwGetTime();
    double last = start;
    double lastTitle = -1.0;
    int frame = 0, recorded = 0, fpsFrames = 0;
    double fpsStart = start;
    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const double dt = now - last;
        last = now;

        app.clock.update(dt);
        if (app.seasons.update(dt, app.clock)) {
            app.transition = true;
            showToast(app, "季節: " + app.seasons.currentName());
        }
        if (const ScenarioStep* step = app.scenario->update(dt)) applyScenarioStep(app, *step);

        const ObserverConfig& here = app.locations[app.location];
        const auto bodies = computeBodies(app.clock.julianDate(), here);
        const SkyState sky = computeSky(app.clock, here, bodies);
        app.sunAltDeg = sky.sunAltDeg;
        const ModeStyle& style = allModes()[app.mode].style;

        if (now - lastTitle > 0.1) {
            glfwSetWindowTitle(window, statusLine(app).c_str());
            lastTitle = now;
        }

        // 演出の線: 流れ星（流星群モードはふたご座流星群の放射点から）とワープの光跡
        effectLines.clear();
        astro::Vec3 radiant{0, 0, 0};
        if (style.meteorRate > 1.0) {
            radiant = sky.j2000ToHorizon * astro::equatorialVector(112.0, 33.0);
            if (radiant.z < 0.0) radiant = {0, 0, 0};
        }
        meteors.update(dt, style.meteorRate, radiant);
        meteors.appendLines(effectLines);
        if (style.warp) {
            appendWarpStreaks(effectLines, stars, sky.j2000ToHorizon, viewForward(app.view),
                              app.view.fovDeg * 0.5 * astro::kDeg, now);
        }

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        ViewSettings drawView = app.view;  // 水鏡モードは画角が変わるので、名前の配置も同じ投影で計算する
        int landscape = landscapeId(here.landscape);
        if (style.mirror) applyMirror(drawView, landscape);
        addTexts(text, app, style, constellations, starNames, bodies,
                 makeProjection(drawView, fbW, fbH, landscape), sky.j2000ToHorizon, fbW, fbH, now);
        if (app.transition) {
            renderer->startTransition(static_cast<float>(now), 1.0f);
            app.transition = false;
        }
        renderer->render({fbW, fbH, app.view, sky, style, static_cast<float>(now), app.overlays,
                          landscapeId(here.landscape), &bodies, &effectLines});
        if (opts.benchmark > 0.0) glFinish();  // GPU の処理が終わるまで待って、実際の描画時間を測る

        ++frame;
        if (!opts.screenshot.empty() && frame >= 3 && now - start >= opts.wait) {
            saveScreenshot(opts.screenshot, fbW, fbH);
            glfwSetWindowShouldClose(window, 1);
        }
        if (!opts.record.empty() && frame >= 3 && now - start >= opts.wait + recorded * opts.interval) {
            char name[512];
            std::snprintf(name, sizeof(name), "%s_%03d.png", opts.record.c_str(), recorded);
            saveScreenshot(name, fbW, fbH);
            if (++recorded >= opts.count) glfwSetWindowShouldClose(window, 1);
        }
        // 描画速度: 1 秒ごとに平均 fps を更新。--benchmark では指定秒数の平均を表示して終了
        ++fpsFrames;
        if (now - fpsStart >= 1.0) {
            app.fps = fpsFrames / (now - fpsStart);
            fpsFrames = 0;
            fpsStart = now;
        }
        if (opts.benchmark > 0.0 && now - start >= opts.benchmark) {
            const double elapsed = glfwGetTime() - start;
            std::printf("benchmark: %d frames in %.2f s = %.1f fps (%.2f ms/frame), %dx%d, mode %s\n", frame,
                        elapsed, frame / elapsed, 1000.0 * elapsed / frame, fbW, fbH, allModes()[app.mode].id);
            glfwSetWindowShouldClose(window, 1);
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    renderer.reset();  // GL コンテキストが生きているうちに解放する
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
