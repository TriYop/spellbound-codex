#include "preset_builder/services/export_service.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace pb {

namespace {

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

std::string formatBand(const std::array<float, 7>& v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    for (int i = 0; i < 7; ++i) {
        if (i > 0) oss << ' ';
        oss << v[static_cast<size_t>(i)];
    }
    return oss.str();
}

} // namespace

void ExportService::exportXml(const Preset& preset,
                              const PresetStats& stats,
                              const std::string& outputPath) const {
    std::ofstream out(outputPath);
    if (!out) throw std::runtime_error("ExportService: cannot write to " + outputPath);

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<preset"
        << " name=\""        << xmlEscape(preset.name)        << "\""
        << " description=\"" << xmlEscape(preset.description) << "\""
        << ">\n";
    out << std::fixed << std::setprecision(4);
    out << "  <bandRmsDb>"       << formatBand(stats.bandRmsDb)       << "</bandRmsDb>\n";
    out << "  <bandMinCorr>"     << formatBand(stats.bandCorrMin)     << "</bandMinCorr>\n";
    out << "  <bandTransientDb>" << formatBand(stats.bandTransientDb) << "</bandTransientDb>\n";
    out << "  <overallRmsDb>"    << stats.overallRmsDb                << "</overallRmsDb>\n";
    out << "  <overallMinCorr>"  << stats.overallCorrMin              << "</overallMinCorr>\n";
    out << "</preset>\n";
}

} // namespace pb
