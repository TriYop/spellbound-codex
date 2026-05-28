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

#include "mastertweak/dsp/lufs_analyser.hpp"
#include <numbers>
#include <vector>

static constexpr float kSr = 44100.f;
static constexpr int   kSrI = 44100;

// Make a stereo 100 Hz sine of given amplitude and length in seconds.
// 100 Hz is well below the K-weighting shelf (1682 Hz) and well above
// the high-pass (38 Hz), so K-weighting gain is approximately 0 dB there.
static std::vector<std::vector<float>> makeTestSine(float ampPeak, float durationSec) {
    const int frames = static_cast<int>(durationSec * kSr);
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(frames)));
    for (int i = 0; i < frames; ++i) {
        const float s = ampPeak * std::sin(2.f * std::numbers::pi_v<float>
                                           * 100.f * static_cast<float>(i) / kSr);
        buf[0][static_cast<size_t>(i)] = s;
        buf[1][static_cast<size_t>(i)] = s;
    }
    return buf;
}

TEST_CASE("LufsAnalyser: silence returns floor value") {
    const int frames = kSrI * 3;  // 3 seconds
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(frames), 0.f));
    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 2);
    const float lufs = la.measure(buf, frames);
    CHECK(lufs <= -69.f);  // at or below the absolute gate floor
}

TEST_CASE("LufsAnalyser: short buffer below one block returns floor") {
    // 100ms < 400ms block — no complete blocks, should return floor
    const int frames = kSrI / 10;
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(frames), 0.1f));
    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 2);
    const float lufs = la.measure(buf, frames);
    CHECK(lufs <= -69.f);
}

TEST_CASE("LufsAnalyser: stereo 100 Hz sine at -20 dBFS -> ~-20.7 LUFS") {
    // Peak amplitude -20 dBFS: A = 10^(-20/20) = 0.1
    // K-weighting stage 2 (HP at 38 Hz) attenuates 100 Hz by ~-1.18 dB at 44100 Hz,
    // so actual result is ~-21.87 LUFS. Tolerance is ±2 dB around ideal -20.691.
    //   z = A^2 * G^2 (G = K-weighted gain at 100 Hz)
    //   LUFS = -0.691 + 10*log10(z)
    const float A = std::pow(10.f, -20.f / 20.f);
    auto buf = makeTestSine(A, 3.f);
    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 2);
    const float lufs = la.measure(buf, static_cast<int>(3.f * kSr));
    CHECK(lufs > -23.0f);
    CHECK(lufs < -19.0f);
}

TEST_CASE("LufsAnalyser: mono input does not crash") {
    const int frames = kSrI * 3;
    std::vector<std::vector<float>> buf(1, std::vector<float>(static_cast<size_t>(frames), 0.05f));
    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 1);
    const float lufs = la.measure(buf, frames);
    CHECK(std::isfinite(lufs));
}
