#include "mastertweak/preset.hpp"

#include <pugixml.hpp>

#include <cstdlib>
#include <filesystem>
#include <unordered_set>

namespace mt {

namespace fs = std::filesystem;

namespace {

// Attribute names matching MixAdvice's XML schema band order
static constexpr const char* kBandAttrNames[PresetData::kNumBands] = {
    "sub", "lows", "lomid", "mids", "himid", "highs", "air"
};

bool parsePresetNode(const pugi::xml_node& root, PresetData& out, std::string* errOut) {
    if (std::string_view{root.name()} != "MixAdvicePreset") {
        if (errOut) *errOut = "Root element is not <MixAdvicePreset>";
        return false;
    }

    out.name        = root.attribute("name").as_string();
    out.description = root.attribute("description").as_string();

    auto readBandArr = [&](const char* elem, std::array<float, PresetData::kNumBands>& arr) -> bool {
        const pugi::xml_node node = root.child(elem);
        if (!node) {
            if (errOut) *errOut = std::string("Missing <") + elem + ">";
            return false;
        }
        for (int i = 0; i < PresetData::kNumBands; ++i) {
            const pugi::xml_attribute attr = node.attribute(kBandAttrNames[i]);
            if (!attr) {
                if (errOut) *errOut = std::string("Missing attribute '")
                                    + kBandAttrNames[i] + "' in <" + elem + ">";
                return false;
            }
            arr[static_cast<size_t>(i)] = attr.as_float();
        }
        return true;
    };

    if (!readBandArr("BandRmsDb",       out.bandRmsDb))       return false;
    if (!readBandArr("BandMinCorr",     out.bandMinCorr))     return false;
    if (!readBandArr("BandTransientDb", out.bandTransientDb)) return false;

    const pugi::xml_node overall = root.child("Overall");
    if (!overall) {
        if (errOut) *errOut = "Missing <Overall>";
        return false;
    }
    out.overallRmsDb   = overall.attribute("rmsDb").as_float(-18.f);
    out.overallMinCorr = overall.attribute("minCorr").as_float(0.6f);

    return true;
}

} // anonymous namespace

std::optional<PresetData> loadPreset(const std::string& xmlPath, std::string* errOut) {
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_file(xmlPath.c_str());
    if (!result) {
        if (errOut) *errOut = result.description();
        return std::nullopt;
    }

    PresetData p;
    if (!parsePresetNode(doc.first_child(), p, errOut))
        return std::nullopt;
    return p;
}

std::vector<PresetData> loadPresetsFromDir(const std::string& dirPath) {
    std::vector<PresetData> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
        if (ec) break;
        if (entry.path().extension() != ".xml") continue;
        if (auto p = loadPreset(entry.path().string()))
            out.push_back(std::move(*p));
    }
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

    return result;
}

} // namespace mt
