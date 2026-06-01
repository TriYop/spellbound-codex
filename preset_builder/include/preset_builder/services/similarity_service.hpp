#pragma once

#include "preset_builder/domain/track.hpp"

#include <string>
#include <vector>

namespace pb {

struct SimilarityGroup {
    std::vector<Track> tracks;
    std::string        suggestedName;  // derived from metadata
};

class SimilarityService {
public:
    // Normalized Euclidean distance across all 23 analysis dimensions.
    // Each dimension is normalized by a typical-range divisor before squaring.
    // Returns 0.0f for identical analyses; max theoretical ≈ 4.8.
    static float distance(const TrackAnalysis& a, const TrackAnalysis& b);

    // Average-linkage agglomerative clustering.
    // Returns groups with >= 2 tracks, sorted largest-first.
    // threshold: merge clusters whose average pairwise distance < threshold.
    std::vector<SimilarityGroup> discover(
        const std::vector<Track>& tracks,
        float threshold) const;

    // Suggest a preset name for a group of tracks based on their metadata.
    // Falls back to "Group N" (1-indexed fallback parameter) when no metadata.
    static std::string suggestName(const std::vector<Track>& tracks, int fallbackN = 1);
};

} // namespace pb
