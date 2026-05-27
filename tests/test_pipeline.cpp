#include "mastertweak/pipeline.hpp"
#include "mastertweak/io.hpp"
#include "mastertweak/preset.hpp"

#include <doctest.h>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <vector>

namespace fs = std::filesystem;

static constexpr float kSr = 44100.f;
static constexpr int   kSrI = 44100;

// Write a 1-second 1 kHz sine at -12 dBFS to a temp WAV, run the full pipeline,
// verify the output is well-formed and its true peak doesn't exceed the ceiling.
TEST_CASE("pipeline: render raises level and respects limiter ceiling") {
    // ── Build a minimal "flat" preset ────────────────────────────────────────
    mt::PresetData preset;
    preset.name        = "test-flat";
    preset.overallRmsDb = -14.f;
    preset.overallMinCorr = 0.5f;
    for (int i = 0; i < 7; ++i) {
        preset.bandRmsDb[static_cast<size_t>(i)]      = -18.f;
        preset.bandMinCorr[static_cast<size_t>(i)]    =  0.5f;
        preset.bandTransientDb[static_cast<size_t>(i)]=  8.f;
    }

    // ── Write input WAV ───────────────────────────────────────────────────────
    const std::string inPath  = (fs::temp_directory_path() / "mt_pipeline_in.wav").string();
    const std::string outPath = (fs::temp_directory_path() / "mt_pipeline_out.wav").string();

    {
        mt::AudioFile af;
        af.sampleRate  = kSrI;
        af.numChannels = 2;
        af.numFrames   = kSrI;
        af.bitDepth    = 24;
        af.samples.assign(2, std::vector<float>(static_cast<size_t>(kSrI)));
        const float amp = std::pow(10.f, -24.f / 20.f);  // -24 dBFS — well below preset target
        for (int i = 0; i < kSrI; ++i) {
            const float s = amp * std::sin(2.f * std::numbers::pi_v<float> * 1000.f
                                           * static_cast<float>(i) / kSr);
            af.samples[0][static_cast<size_t>(i)] = s;
            af.samples[1][static_cast<size_t>(i)] = s;
        }
        REQUIRE(mt::writeAudioFile(inPath, af));
    }

    // ── Run full pipeline ─────────────────────────────────────────────────────
    std::string err;
    auto result = mt::renderFile(inPath, outPath, preset, {}, nullptr, {}, &err);
    REQUIRE_MESSAGE(result.ok, "render failed: " << err);

    // ── Verify output ─────────────────────────────────────────────────────────
    auto out = mt::readAudioFile(outPath);
    REQUIRE(out.has_value());
    CHECK(out->numChannels == 2);
    CHECK(out->sampleRate  == kSrI);

    // Peak must not exceed -1 dBTP ceiling (linear ≈ 0.891).
    // We allow a small sample-rate quantisation margin.
    float maxAbs = 0.f;
    for (const auto& ch : out->samples)
        for (auto s : ch)
            maxAbs = std::max(maxAbs, std::abs(s));

    CHECK(maxAbs <= 0.892f);   // -1 dBTP ceiling ≈ 0.891 (two-pass limiter guarantees this)

    // Output should be louder than the input (-12 dBFS → pipeline adds makeup gain).
    float outRms = 0.f;
    for (int f = kSrI / 4; f < kSrI; ++f) {  // skip first quarter (compressor settling)
        const float s = out->samples[0][static_cast<size_t>(f)];
        outRms += s * s;
    }
    outRms = std::sqrt(outRms / (kSrI * 3 / 4));

    const float inputRmsLinear = std::pow(10.f, -24.f / 20.f) / std::sqrt(2.f);  // sine RMS
    CHECK(outRms > inputRmsLinear);  // pipeline must have raised the level

    // Cleanup
    fs::remove(inPath);
    fs::remove(outPath);
}

TEST_CASE("pipeline: analyseOnly returns valid snapshot without rendering") {
    mt::PresetData preset;
    preset.name = "test";
    preset.overallRmsDb = -14.f;

    const std::string inPath = (fs::temp_directory_path() / "mt_analyse_in.wav").string();
    {
        mt::AudioFile af;
        af.sampleRate  = kSrI;
        af.numChannels = 2;
        af.numFrames   = kSrI;
        af.bitDepth    = 24;
        af.samples.assign(2, std::vector<float>(static_cast<size_t>(kSrI)));
        for (int i = 0; i < kSrI; ++i) {
            const float s = 0.5f * std::sin(2.f * std::numbers::pi_v<float> * 440.f
                                            * static_cast<float>(i) / kSr);
            af.samples[0][static_cast<size_t>(i)] = s;
            af.samples[1][static_cast<size_t>(i)] = s;
        }
        REQUIRE(mt::writeAudioFile(inPath, af));
    }

    std::string err;
    auto result = mt::analyseOnly(inPath, preset, &err);
    REQUIRE_MESSAGE(result.ok, err);

    // A 440 Hz tone at -6 dBFS should show up in Lows band
    CHECK(result.analysis.overallAvgDb > -30.f);   // not silence
    CHECK(result.analysis.overallAvgDb < 0.f);      // not clipping
    CHECK(result.analysis.overallCorr  > 0.9f);     // mono → high correlation

    fs::remove(inPath);
}
