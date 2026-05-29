#pragma once
#include "preset_builder/ports/metadata_provider.hpp"
namespace gui {
class AcoustIdMetadataProvider : public pb::MetadataProvider {
public:
    std::optional<pb::TrackMetadata> lookup(const std::string& audioFilePath) override;
};
} // namespace gui
