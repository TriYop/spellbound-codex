#include "preset_builder/services/export_service.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace pb {

namespace {

static constexpr std::array<const char*, 7> kBandAttr = {
    "sub", "lows", "lomid", "mids", "himid", "highs", "air"
};

std::string xmlEscape(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (const char c : s) {
        switch (c) {
            case '&':  r += "&amp;";  break;
            case '"':  r += "&quot;"; break;
            case '<':  r += "&lt;";   break;
            case '>':  r += "&gt;";   break;
            default:   r += c;        break;
        }
    }
    return r;
}

std::string formatBandElem(const char* elemName, const std::array<float, 7>& v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "  <" << elemName;
    for (int i = 0; i < 7; ++i)
        oss << ' ' << kBandAttr[static_cast<size_t>(i)]
            << "=\"" << v[static_cast<size_t>(i)] << '"';
    oss << "/>\n";
    return oss.str();
}

} // namespace

void ExportService::exportXml(const Preset& preset,
                              const PresetStats& stats,
                              const std::string& outputPath) const {
    std::ofstream out(outputPath);
    if (!out) throw std::runtime_error("ExportService: cannot write to " + outputPath);

    out << std::fixed << std::setprecision(4);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<MixAdvicePreset"
        << " name=\""        << xmlEscape(preset.name)        << "\""
        << " description=\"" << xmlEscape(preset.description) << "\""
        << ">\n";
    out << formatBandElem("BandRmsDb",       stats.bandRmsDb);
    out << formatBandElem("BandMinCorr",     stats.bandCorrMin);
    out << formatBandElem("BandTransientDb", stats.bandTransientDb);
    out << "  <Overall rmsDb=\""  << stats.overallRmsDb
        << "\" minCorr=\"" << stats.overallCorrMin << "\"/>\n";
    out << "</MixAdvicePreset>\n";
}

} // namespace pb
