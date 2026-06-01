#include "preset_builder/services/similarity_service.hpp"

#include <doctest.h>

#include <array>
#include <cmath>

using pb::SimilarityGroup;
using pb::SimilarityService;
using pb::Track;
using pb::TrackAnalysis;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static TrackAnalysis makeAnalysis(float rmsOffset = 0.f) {
    TrackAnalysis a;
    a.bandRmsDb       = {-20.f + rmsOffset, -18.f + rmsOffset, -16.f + rmsOffset,
                          -14.f + rmsOffset, -12.f + rmsOffset, -10.f + rmsOffset,
                          -8.f + rmsOffset};
    a.bandCorr        = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f};
    a.bandTransientDb = {6.f, 5.f, 4.f, 3.f, 2.f, 1.f, 0.f};
    a.overallRmsDb    = -18.f + rmsOffset;
    a.overallCorr     = 0.85f;
    return a;
}

// Bass-heavy spectral tilt at the same overall level — genuinely different shape.
// Relative shape: {+12, +8, +4, 0, -4, -8, -12} dB vs flat shape of makeAnalysis().
static TrackAnalysis makeTiltedAnalysis() {
    TrackAnalysis a;
    a.bandRmsDb       = {-8.f, -12.f, -16.f, -20.f, -24.f, -28.f, -32.f};
    a.bandCorr        = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f};
    a.bandTransientDb = {6.f, 5.f, 4.f, 3.f, 2.f, 1.f, 0.f};
    a.overallRmsDb    = -20.f;
    a.overallCorr     = 0.85f;
    return a;
}

static Track makeTrack(float rmsOffset = 0.f) {
    Track t;
    t.analysis = makeAnalysis(rmsOffset);
    return t;
}

static Track makeTrackWithMeta(float rmsOffset,
                                const std::string& genre,
                                const std::string& artist,
                                int year) {
    Track t = makeTrack(rmsOffset);
    t.metadata.genre  = genre;
    t.metadata.artist = artist;
    t.metadata.year   = year;
    return t;
}

// ---------------------------------------------------------------------------
// distance() tests
// ---------------------------------------------------------------------------

TEST_CASE("SimilarityService::distance is zero for identical analyses") {
    TrackAnalysis a = makeAnalysis();
    CHECK(SimilarityService::distance(a, a) == doctest::Approx(0.f));
}

TEST_CASE("SimilarityService::distance is non-zero for different analyses") {
    // makeAnalysis(0) relative shape: bandRmsDb-overallRmsDb = {-2,0,2,4,6,8,10}.
    // makeTiltedAnalysis relative shape: {12,8,4,0,-4,-8,-12}.
    // Shape diffs: {-14,-8,-2,4,10,16,22}; normalized by 40.
    // sum = (196+64+4+16+100+256+484)/1600 = 1120/1600 = 0.7; sqrt ≈ 0.8367
    TrackAnalysis a = makeAnalysis(0.f);
    TrackAnalysis b = makeTiltedAnalysis();
    float d = SimilarityService::distance(a, b);
    CHECK(d > 0.f);
    CHECK(d == doctest::Approx(std::sqrt(0.7f)).epsilon(0.001f));
}

// ---------------------------------------------------------------------------
// discover() tests
// ---------------------------------------------------------------------------

TEST_CASE("SimilarityService::discover: two similar tracks cluster below threshold") {
    // Two flat-spectrum tracks (distance 0) and one tilted-spectrum outlier
    // (distance ≈ 0.529). Threshold 0.3 groups the flat pair, excludes the outlier.
    Track t0 = makeTrack(0.f);
    Track t1 = makeTrack(0.f);        // identical spectral shape → distance 0
    Track t2; t2.analysis = makeTiltedAnalysis();  // different shape → distance ≈ 0.529

    SimilarityService svc;
    auto groups = svc.discover({t0, t1, t2}, 0.5f);

    REQUIRE(groups.size() == 1);
    CHECK(groups[0].tracks.size() == 2);
    for (const auto& t : groups[0].tracks)
        CHECK(t.analysis.overallRmsDb == doctest::Approx(-18.f));
}

TEST_CASE("SimilarityService::discover: outlier excluded from group") {
    Track trackA = makeTrack(0.f);
    Track trackB = makeTrack(0.f);
    Track trackC; trackC.analysis = makeTiltedAnalysis();  // different spectral shape
    // Give trackC a unique hash so we can identify it in the output.
    trackC.id.hash = "outlier-hash";

    SimilarityService svc;
    auto groups = svc.discover({trackA, trackB, trackC}, 0.3f);

    REQUIRE(groups.size() == 1);
    const auto& g = groups[0];
    CHECK(g.tracks.size() == 2);
    // Verify the outlier (trackC) is not in the group.
    bool outlierInGroup = false;
    for (const auto& t : g.tracks)
        if (t.id.hash == trackC.id.hash) outlierInGroup = true;
    CHECK_FALSE(outlierInGroup);
}

TEST_CASE("SimilarityService::discover: threshold 0 returns empty") {
    Track t0 = makeTrack(0.f);
    Track t1 = makeTrack(0.f);

    SimilarityService svc;
    // threshold 0: no cluster whose avg distance is < 0 can be formed.
    auto groups = svc.discover({t0, t1}, 0.f);
    CHECK(groups.empty());
}

TEST_CASE("SimilarityService::discover: high threshold merges all into one group") {
    Track t0 = makeTrack(0.f);
    Track t1 = makeTrack(5.f);
    Track t2 = makeTrack(10.f);

    SimilarityService svc;
    auto groups = svc.discover({t0, t1, t2}, 100.f);

    REQUIRE(groups.size() == 1);
    CHECK(groups[0].tracks.size() == 3);
}

// ---------------------------------------------------------------------------
// suggestName() tests
// ---------------------------------------------------------------------------

TEST_CASE("SimilarityService::suggestName: genre and decade from metadata") {
    std::vector<Track> tracks;
    tracks.push_back(makeTrackWithMeta(0.f, "Rock", "ArtistA", 2012));
    tracks.push_back(makeTrackWithMeta(0.f, "Rock", "ArtistA", 2015));
    tracks.push_back(makeTrackWithMeta(0.f, "Rock", "ArtistA", 2010));

    std::string name = SimilarityService::suggestName(tracks, 1);

    // Dominant genre = "Rock" (100% of 3), dominant artist = "ArtistA",
    // years 2010..2015 range=5 ≤15, sorted=[2010,2012,2015], median = 2012, decade = 2010s
    // Expected: "ArtistA Rock 2010s"
    CHECK(name.find("Rock") != std::string::npos);
    CHECK(name.find("2010s") != std::string::npos);
}

TEST_CASE("SimilarityService::suggestName: fallback when no metadata") {
    std::vector<Track> tracks;
    tracks.push_back(makeTrack(0.f));
    tracks.push_back(makeTrack(1.f));

    std::string name = SimilarityService::suggestName(tracks, 3);
    CHECK(name == "Group 3");
}
