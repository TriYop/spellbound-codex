#include "preset_builder/services/ingest_service.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace pb {

TrackMetadata IngestService::parseFilenameMetadata(const std::string& filePath) {
    TrackMetadata m;
    m.source = MetadataSource::filename;

    fs::path p(filePath);
    std::string stem = p.stem().string();

    const std::string sepAscii  = " - ";
    const std::string sepEmDash = " \xe2\x80\x93 ";

    auto trySplit = [&](const std::string& sep) -> bool {
        const auto pos = stem.find(sep);
        if (pos == std::string::npos) return false;
        m.artist = stem.substr(0, pos);
        m.title  = stem.substr(pos + sep.size());
        return true;
    };

    if (!trySplit(sepAscii) && !trySplit(sepEmDash))
        m.title = stem;

    return m;
}

IngestReport IngestService::ingest(const std::string& /*path*/,
                                   TrackRepository&   /*repo*/,
                                   MetadataProvider&  /*metadata*/,
                                   mt::ProgressCallback /*progress*/) const {
    // Implemented in Task 7.
    return {};
}

} // namespace pb
