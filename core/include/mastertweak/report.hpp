#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/preset.hpp"

#include <string>

namespace mt {

// Format a human-readable Markdown document from analysis + advice + preset.
// inputFilename is only used as a display label in the document header.
std::string formatAdviceMarkdown(
    const AnalysisSnapshot& snap,
    const AdviceSet&        advice,
    const PresetData&       preset,
    const std::string&      inputFilename);

} // namespace mt
