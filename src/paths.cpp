#include "paths.hpp"

#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {
bool hasResources(const fs::path& dir) {
    std::error_code ec;
    return fs::exists(dir / "shaders" / "stars.vert", ec) && fs::exists(dir / "data" / "stars.csv", ec);
}

fs::path executableDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) return fs::path(buf).parent_path();
#endif
    return {};
}
}  // namespace

std::string resourceRoot() {
    if (const char* env = std::getenv("PLANETARIUM_ROOT"); env && hasResources(env)) return env;
    if (const fs::path exe = executableDir(); !exe.empty() && hasResources(exe)) {
        return exe.generic_u8string();
    }
    return PLANETARIUM_ROOT;
}
