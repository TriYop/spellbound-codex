#include "mastertweak/preset.hpp"

#include "audioplugins/common/analysis/PresetIO.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <unordered_set>

namespace mt {

namespace fs = std::filesystem;

namespace {

PresetData toMtPresetData(const audioplugins::common::analysis::PresetData& src) {
    PresetData out;
    out.name            = src.name;
    out.description      = src.description;
    out.bandRmsDb        = src.bandRmsDb;
    out.bandMinCorr      = src.bandMinCorr;
    out.bandTransientDb  = src.bandTransientDb;
    out.overallRmsDb     = src.overallRmsDb;
    out.overallMinCorr   = src.overallMinCorr;
    return out;
}

} // anonymous namespace

std::optional<PresetData> loadPreset(const std::string& xmlPath, std::string* errOut) {
    auto p = audioplugins::common::analysis::PresetIO::load(xmlPath, errOut);
    if (!p) return std::nullopt;
    return toMtPresetData(*p);
}

std::vector<PresetData> loadPresetsFromDir(const std::string& dirPath) {
    const auto common = audioplugins::common::analysis::PresetIO::loadFromDirectory(dirPath);
    std::vector<PresetData> out;
    out.reserve(common.size());
    for (const auto& p : common) out.push_back(toMtPresetData(p));
    return out;
}

static std::string userPresetsDir() {
    const char* home = std::getenv("HOME");
    if (!home) return {};
    return std::string(home) + "/.config/MixAdvice/Presets";
}

std::optional<PresetData> resolvePreset(const std::string& nameOrPath,
                                         const std::string& executableDir) {
    // 1. Treat as literal path if it looks like one
    if (nameOrPath.find('/') != std::string::npos || nameOrPath.ends_with(".xml")) {
        return loadPreset(nameOrPath);
    }

    // 2. Search bundled presets/
    auto tryDir = [&](const std::string& dir) -> std::optional<PresetData> {
        if (dir.empty()) return std::nullopt;
        for (const auto& p : loadPresetsFromDir(dir)) {
            if (p.name == nameOrPath) return p;
            // Also match file stem (e.g. "pop-billie-eilish")
        }
        // Try stem-based match
        const fs::path xmlPath = fs::path(dir) / (nameOrPath + ".xml");
        if (fs::exists(xmlPath)) return loadPreset(xmlPath.string());
        return std::nullopt;
    };

    if (!executableDir.empty()) {
        const std::string bundled = executableDir + "/presets";
        if (auto p = tryDir(bundled)) return p;
    }

    // 3. User presets (~/.config/MixAdvice/Presets)
    return tryDir(userPresetsDir());
}

std::vector<PresetData> enumeratePresets(const std::string& executableDir) {
    std::vector<PresetData> result;
    std::unordered_set<std::string> seen;

    auto addDir = [&](const std::string& dir) {
        for (auto& p : loadPresetsFromDir(dir)) {
            if (seen.insert(p.name).second)
                result.push_back(std::move(p));
        }
    };

    if (!executableDir.empty())
        addDir(executableDir + "/presets");
    addDir(userPresetsDir());

    std::sort(result.begin(), result.end(), [](const PresetData& a, const PresetData& b) {
        auto toLower = [](const std::string& s) {
            std::string t = s;
            for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return t;
        };
        return toLower(a.name) < toLower(b.name);
    });

    return result;
}

} // namespace mt
