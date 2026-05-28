#include "mastertweak/target_level.hpp"
#include <doctest.h>
#include <cmath>

TEST_CASE("target_level: table has exactly 8 entries") {
    CHECK(mt::kTargetLevelProfiles.size() == 8);
}

TEST_CASE("target_level: all entries have negative LUFS and non-positive ceiling") {
    for (const auto& p : mt::kTargetLevelProfiles) {
        CHECK(p.lufs < 0.f);
        CHECK(p.peakCeiling <= 0.f);
        CHECK(!p.name.empty());
    }
}

TEST_CASE("target_level: findTargetLevel case-insensitive hit") {
    const auto* p = mt::findTargetLevel("spotify");
    REQUIRE(p != nullptr);
    CHECK(p->lufs == doctest::Approx(-14.f));
    CHECK(p->peakCeiling == doctest::Approx(-1.f));
}

TEST_CASE("target_level: findTargetLevel uppercase") {
    CHECK(mt::findTargetLevel("APPLE MUSIC") != nullptr);
}

TEST_CASE("target_level: findTargetLevel unknown returns nullptr") {
    CHECK(mt::findTargetLevel("does not exist") == nullptr);
}

TEST_CASE("target_level: CD / Download entry exists") {
    const auto* p = mt::findTargetLevel("cd / download");
    REQUIRE(p != nullptr);
    CHECK(p->lufs == doctest::Approx(-9.f));
    CHECK(p->peakCeiling == doctest::Approx(-0.1f));
}
