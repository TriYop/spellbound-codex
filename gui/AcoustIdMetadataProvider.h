#pragma once

#include "preset_builder/ports/metadata_provider.hpp"

#include <optional>
#include <string>

namespace gui {

// Implements pb::MetadataProvider using fpcalc (fingerprinting) + AcoustID API.
// Best-effort: silently returns nullopt if fpcalc is not on PATH or network fails.
// Called from the IngestWorker thread; creates a fresh QNetworkAccessManager per call.
class AcoustIdMetadataProvider : public pb::MetadataProvider {
public:
    std::optional<pb::TrackMetadata> lookup(const std::string& audioFilePath) override;

private:
    struct Fingerprint { std::string fp; int duration = 0; };
    static std::optional<Fingerprint> runFpcalc(const std::string& path);
    static std::optional<pb::TrackMetadata> queryAcoustId(const Fingerprint& fp);
};

} // namespace gui
