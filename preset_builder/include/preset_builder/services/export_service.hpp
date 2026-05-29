#pragma once

#include "preset_builder/domain/preset.hpp"

#include <string>

namespace pb {

class ExportService {
public:
    // Writes MixAdvice-compatible XML to outputPath.
    // Throws std::runtime_error if the file cannot be written.
    void exportXml(const Preset& preset,
                   const PresetStats& stats,
                   const std::string& outputPath) const;
};

} // namespace pb
