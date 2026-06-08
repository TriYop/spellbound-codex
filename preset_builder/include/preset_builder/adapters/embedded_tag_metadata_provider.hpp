#pragma once

#include "preset_builder/ports/metadata_provider.hpp"

namespace pb {

// Implements MetadataProvider by reading embedded audio tags via TagLib.
// Returns nullopt only if TagLib cannot open the file.
// Empty-tag files return a TrackMetadata with all optional fields unset.
class EmbeddedTagMetadataProvider final : public MetadataProvider {
public:
    std::optional<TrackMetadata> lookup(const std::string& audioFilePath) override;
};

} // namespace pb
