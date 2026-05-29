#pragma once

#include "preset_builder/domain/track.hpp"

#include <optional>
#include <string>

namespace pb {

// Port: implemented in the GUI layer (AcoustIdMetadataProvider) so the core
// lib stays Qt-free. IngestService calls this and falls back to filename
// parsing when it returns nullopt.
class MetadataProvider {
public:
    virtual ~MetadataProvider() = default;
    virtual std::optional<TrackMetadata> lookup(const std::string& audioFilePath) = 0;
};

} // namespace pb
