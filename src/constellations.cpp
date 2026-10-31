#include "constellations.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace {
// コメント行（#）と見出し行を飛ばして、各行をカンマで分割して返す
std::vector<std::vector<std::string>> readCsv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<std::vector<std::string>> rows;
    std::string line;
    bool header = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (header) {
            header = false;
            continue;
        }
        std::vector<std::string> f;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, ',')) f.push_back(field);
        if (line.back() == ',') f.emplace_back();
        rows.push_back(std::move(f));
    }
    return rows;
}
}  // namespace

ConstellationData loadConstellations(const std::string& dataDir) {
    ConstellationData d;
    for (const auto& f : readCsv(dataDir + "/constellation_lines.csv")) {
        if (f.size() < 5) continue;
        d.segments.emplace_back(astro::equatorialVector(std::stod(f[1]), std::stod(f[2])),
                                astro::equatorialVector(std::stod(f[3]), std::stod(f[4])));
    }
    for (const auto& f : readCsv(dataDir + "/constellation_names.csv")) {
        if (f.size() < 5) continue;
        d.names.push_back({astro::equatorialVector(std::stod(f[1]), std::stod(f[2])), f[4], std::stoi(f[3]), 0.0f});
    }
    return d;
}

std::vector<SkyLabel> loadStarNames(const std::string& dataDir) {
    std::unordered_map<std::string, std::string> ja;
    for (const auto& f : readCsv(dataDir + "/star_names.csv")) {
        if (f.size() >= 2) ja[f[0]] = f[1];
    }
    std::vector<SkyLabel> out;
    for (const auto& f : readCsv(dataDir + "/stars.csv")) {  // hip,ra_deg,dec_deg,mag,ci,con,proper
        if (f.size() < 7) continue;
        auto it = ja.find(f[6]);
        if (it == ja.end()) continue;
        out.push_back({astro::equatorialVector(std::stod(f[1]), std::stod(f[2])), it->second, 1, std::stof(f[3])});
    }
    return out;
}
