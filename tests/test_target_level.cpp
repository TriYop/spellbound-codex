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

TEST_CASE("LufsAnalyser: stereo 100 Hz sine at -20 dBFS -> ~-21.87 LUFS") {
    // Peak amplitude -20 dBFS: A = 10^(-20/20) = 0.1
    // K-weighting stage 2 (HP at 38 Hz) attenuates 100 Hz by ~-1.18 dB at 44100 Hz.
    // Without K-weighting: LUFS = -0.691 + 10*log10(A^2) = -20.691
    // With -1.18 dB attenuation: LUFS ≈ -20.691 - 1.18 = -21.87
    // Tolerance ±0.5 LU around the analytically expected value.
    const float A = std::pow(10.f, -20.f / 20.f);
    auto buf = makeTestSine(A, 3.f);
    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 2);
    const float lufs = la.measure(buf, static_cast<int>(3.f * kSr));
    CHECK(lufs > -22.4f);
    CHECK(lufs < -21.4f);
}

TEST_CASE("LufsAnalyser: mono input does not crash") {
    const int frames = kSrI * 3;
    std::vector<std::vector<float>> buf(1, std::vector<float>(static_cast<size_t>(frames), 0.05f));
    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 1);
    const float lufs = la.measure(buf, frames);
    CHECK(std::isfinite(lufs));
}

#include "mastertweak/pipeline.hpp"
#include "mastertweak/io.hpp"
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("pipeline: Spotify target normalises output to ~-14 LUFS") {
    // Build a flat preset whose overallRmsDb = -14 to avoid extreme gain trims.
    mt::PresetData preset;
    preset.name           = "test-flat";
    preset.overallRmsDb   = -14.f;
    preset.overallMinCorr = 0.5f;
    for (int i = 0; i < 7; ++i) {
        preset.bandRmsDb[static_cast<size_t>(i)]       = -18.f;
        preset.bandMinCorr[static_cast<size_t>(i)]     =  0.5f;
        preset.bandTransientDb[static_cast<size_t>(i)] =  8.f;
    }

    // Write 3-second 100 Hz stereo sine at -20 dBFS.
    const std::string inPath  = (fs::temp_directory_path() / "mt_lufs_in.wav").string();
    const std::string outPath = (fs::temp_directory_path() / "mt_lufs_out.wav").string();
    {
        mt::AudioFile af;
        af.sampleRate  = kSrI;
        af.numChannels = 2;
        af.numFrames   = kSrI * 3;
        af.bitDepth    = 24;
        af.samples.assign(2, std::vector<float>(static_cast<size_t>(kSrI * 3)));
        const float amp = std::pow(10.f, -20.f / 20.f);
        for (int i = 0; i < kSrI * 3; ++i) {
            const float s = amp * std::sin(2.f * std::numbers::pi_v<float>
                                           * 100.f * static_cast<float>(i) / kSr);
            af.samples[0][static_cast<size_t>(i)] = s;
            af.samples[1][static_cast<size_t>(i)] = s;
        }
        REQUIRE(mt::writeAudioFile(inPath, af));
    }

    // Render with Spotify target.
    mt::RenderOptions opts;
    opts.targetLevel = *mt::findTargetLevel("spotify");

    std::string err;
    auto result = mt::renderFile(inPath, outPath, preset, opts, nullptr, {}, &err);
    REQUIRE_MESSAGE(result.ok, "render failed: " << err);

    // Re-measure output LUFS.
    auto out = mt::readAudioFile(outPath);
    REQUIRE(out.has_value());

    mt::dsp::LufsAnalyser la;
    la.prepare(kSr, 2);
    const float measuredLufs = la.measure(out->samples, out->numFrames);

    // Should land within ±1.5 LU of the Spotify target (-14 LUFS).
    CHECK(measuredLufs > -15.5f);
    CHECK(measuredLufs < -12.5f);

    // Sample-peak must not exceed -1 dBFS (≈ 0.891). The limiter uses sample-peak
    // detection (not 4× oversampled true-peak) so this is the enforced constraint.
    float maxAbs = 0.f;
    for (const auto& ch : out->samples)
        for (auto s : ch)
            maxAbs = std::max(maxAbs, std::abs(s));
    CHECK(maxAbs <= 0.892f);
}
