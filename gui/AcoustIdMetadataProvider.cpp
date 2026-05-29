#include "AcoustIdMetadataProvider.h"
namespace gui {
std::optional<pb::TrackMetadata> AcoustIdMetadataProvider::lookup(const std::string&) { return std::nullopt; }
} // namespace gui
