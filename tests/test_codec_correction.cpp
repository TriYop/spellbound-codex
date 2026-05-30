#include "mastertweak/codec_correction.hpp"
#include "mastertweak/io.hpp"

#include <doctest.h>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <vector>

namespace fs = std::filesystem;

// ─── test helpers ─────────────────────────────────────────────────────────────

static mt::AudioFile makeMono(int sr, float durationSec) {
    mt::AudioFile af;
    af.sampleRate  = sr;
    af.numChannels = 1;
    af.numFrames   = static_cast<int>(durationSec * static_cast<float>(sr));
    af.bitDepth    = 24;
    af.samples.assign(1, std::vector<float>(static_cast<size_t>(af.numFrames), 0.f));
    return af;
}

static void addSine(mt::AudioFile& af, float freqHz, float amp = 0.1f) {
    for (int i = 0; i < af.numFrames; ++i)
        af.samples[0][static_cast<size_t>(i)] +=
            amp * std::sin(2.f * std::numbers::pi_v<float> * freqHz
                           * static_cast<float>(i) / static_cast<float>(af.sampleRate));
}

// Linear-congruential white noise in [-1, 1]
static float lcgSample(uint32_t& seed) {
    seed = seed * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<int32_t>(seed)) / 2147483648.f;
}

// ─── SourceFormat detection ───────────────────────────────────────────────────

TEST_CASE("readAudioFile: WAV → SourceFormat::wav") {
    const auto path = (fs::temp_directory_path() / "mt_sf_wav.wav").string();
    auto af = makeMono(44100, 0.1f);
    af.samples[0][0] = 0.5f;
    REQUIRE(mt::writeAudioFile(path, af, {24, false}));

    std::string err;
    const auto loaded = mt::readAudioFile(path, &err);
    REQUIRE_MESSAGE(loaded.has_value(), err);
    CHECK(loaded->sourceFormat == mt::SourceFormat::wav);
}

TEST_CASE("readAudioFile: FLAC → SourceFormat::flac") {
    const auto path = (fs::temp_directory_path() / "mt_sf_flac.flac").string();
    auto af = makeMono(44100, 0.1f);
    af.samples[0][0] = 0.1f;
    REQUIRE(mt::writeAudioFile(path, af, {24, true}));

    std::string err;
    const auto loaded = mt::readAudioFile(path, &err);
    REQUIRE_MESSAGE(loaded.has_value(), err);
    CHECK(loaded->sourceFormat == mt::SourceFormat::flac);
}

// ─── computeCodecCorrection ───────────────────────────────────────────────────

TEST_CASE("computeCodecCorrection: silence → all-zero corrections") {
    // Silence has no HF baseline; baseline < -50 dBFS → early return
    auto af = makeMono(44100, 2.f);
    // samples already zeroed by makeMono
    const auto corr = mt::computeCodecCorrection(af);
    for (float c : corr)
        CHECK(c == 0.f);
}

TEST_CASE("computeCodecCorrection: file shorter than FFT size → all-zero corrections") {
    // File of only 100 frames: too short to build a single 4096-pt window
    mt::AudioFile af;
    af.sampleRate  = 44100;
    af.numChannels = 1;
    af.numFrames   = 100;
    af.samples.assign(1, std::vector<float>(100, 0.5f));
    const auto corr = mt::computeCodecCorrection(af);
    for (float c : corr)
        CHECK(c == 0.f);
}

TEST_CASE("computeCodecCorrection: bass-only signal → all-zero corrections") {
    // 440 Hz sine has no HF energy; baseline (8-12 kHz) will be < -50 dBFS
    auto af = makeMono(44100, 2.f);
    addSine(af, 440.f, 0.5f);
    const auto corr = mt::computeCodecCorrection(af);
    for (float c : corr)
        CHECK(c == 0.f);
}

TEST_CASE("computeCodecCorrection: rolled-off spectrum → positive Air and Highs corrections") {
    // Multi-sine from 1 kHz to 12 kHz, nothing above 12 kHz.
    // Simulates an MP3 with aggressive rolloff above 12 kHz.
    auto af = makeMono(44100, 2.f);
    for (float f : {1000.f, 2000.f, 4000.f, 6000.f, 8000.f, 10000.f, 12000.f})
        addSine(af, f, 0.1f);

    const auto corr = mt::computeCodecCorrection(af);

    // Air (band 6): must be boosted — nothing above 12 kHz
    CHECK(corr[6] > 0.f);
    CHECK(corr[6] <= 6.f);   // clamped at max

    // Highs upper half (band 5): also boosted (nothing at 12–16 kHz)
    CHECK(corr[5] > 0.f);
    CHECK(corr[5] <= 1.5f);  // clamped at max

    // HiMids (band 4): small positive correction (proportional to Air)
    CHECK(corr[4] >= 0.f);
    CHECK(corr[4] <= 0.5f);  // clamped at max

    // Low bands (0–3): no correction
    for (size_t i = 0; i < 4; ++i)
        CHECK(corr[i] == 0.f);
}

TEST_CASE("computeCodecCorrection: rolled-off spectrum hits Air clamp at 6 dB") {
    // Only low-frequency and mid-frequency content; above 12 kHz: zero energy.
    // Baseline will be well above -50 dBFS; Air will be near -100 dBFS → max clamp.
    auto af = makeMono(44100, 3.f);
    for (float f : {1000.f, 3000.f, 6000.f, 9000.f, 12000.f})
        addSine(af, f, 0.2f);

    const auto corr = mt::computeCodecCorrection(af);
    CHECK(corr[6] == doctest::Approx(6.f).epsilon(0.01f));  // hits max clamp
}

TEST_CASE("computeCodecCorrection: white noise (flat spectrum) → small Air correction") {
    // Pseudorandom white noise → approximately flat spectrum → corrections ≈ 0
    mt::AudioFile af;
    af.sampleRate  = 44100;
    af.numChannels = 1;
    af.numFrames   = 44100 * 5;  // 5 s of noise for low spectral variance
    af.samples.assign(1, std::vector<float>(static_cast<size_t>(af.numFrames)));
    uint32_t seed = 0xDEADBEEFu;
    for (float& s : af.samples[0])
        s = lcgSample(seed) * 0.5f;  // keep within ±0.5

    const auto corr = mt::computeCodecCorrection(af);
    // White noise has similar energy at all frequencies → Air correction should be small
    CHECK(corr[6] < 2.f);   // at most 2 dB variance from flat
    CHECK(corr[5] < 1.5f);  // upper Highs: within clamp
    CHECK(corr[4] < 0.5f);  // HiMids: within clamp
    // Low bands: always 0
    for (size_t i = 0; i < 4; ++i)
        CHECK(corr[i] == 0.f);
}

TEST_CASE("computeCodecCorrection: corrections are always non-negative") {
    // Even if the Air band has MORE energy than baseline (e.g. HF-boosted signal),
    // correction must be clamped at 0 — we never subtract.
    auto af = makeMono(44100, 2.f);
    // Strong HF content — add sines above 16 kHz, weak content at 8-12 kHz
    addSine(af, 8000.f, 0.01f);
    addSine(af, 17000.f, 0.5f);
    addSine(af, 19000.f, 0.5f);

    const auto corr = mt::computeCodecCorrection(af);
    for (float c : corr)
        CHECK(c >= 0.f);
}

TEST_CASE("computeCodecCorrection: stereo input → same result as mono of same content") {
    // Stereo mono-summing should not change the correction estimate materially.
    auto mono = makeMono(44100, 2.f);
    for (float f : {1000.f, 4000.f, 8000.f, 10000.f})
        addSine(mono, f, 0.1f);

    // Build stereo version with identical L and R
    mt::AudioFile stereo;
    stereo.sampleRate  = 44100;
    stereo.numChannels = 2;
    stereo.numFrames   = mono.numFrames;
    stereo.samples = {mono.samples[0], mono.samples[0]};

    const auto corrMono   = mt::computeCodecCorrection(mono);
    const auto corrStereo = mt::computeCodecCorrection(stereo);

    for (size_t i = 0; i < 7; ++i)
        CHECK(corrMono[i] == doctest::Approx(corrStereo[i]).epsilon(0.01f));
}
