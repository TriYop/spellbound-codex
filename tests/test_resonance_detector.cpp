#include "mastertweak/analysis.hpp"
#include "mastertweak/dsp/resonance_eq.hpp"
#include "mastertweak/io.hpp"

#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

// ---------------------------------------------------------------------------
// Test 1: White noise only → 0 resonances (or at most 2 statistical peaks)
// ---------------------------------------------------------------------------
TEST_CASE("white noise only → 0 resonances (or at most 2 statistical)") {
    const int sr        = 44100;
    const int numFrames = sr * 2;  // 2 seconds

    mt::AudioFile audio;
    audio.sampleRate  = sr;
    audio.numChannels = 2;
    audio.numFrames   = numFrames;
    audio.bitDepth    = 24;
    audio.samples.resize(2, std::vector<float>(static_cast<size_t>(numFrames)));

    uint32_t seed = 42;
    auto rng = [&]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return (static_cast<float>(seed >> 16) / 32768.f) - 1.f;
    };

    for (int f = 0; f < numFrames; ++f) {
        float s = rng();
        audio.samples[0][static_cast<size_t>(f)] = s;
        audio.samples[1][static_cast<size_t>(f)] = rng();
    }

    const auto peaks = mt::detectResonances(audio);
    // White noise may have occasional statistical peaks; allow a loose bound
    CHECK(peaks.size() <= 2);
}

// ---------------------------------------------------------------------------
// Test 2: White noise + narrow sine → resonance detected near target frequency
// ---------------------------------------------------------------------------
TEST_CASE("white noise + 4 kHz sine → resonance detected near 4000 Hz") {
    const int   sr        = 44100;
    const int   numFrames = sr * 2;  // 2 seconds
    const float targetHz  = 4000.f;

    mt::AudioFile audio;
    audio.sampleRate  = sr;
    audio.numChannels = 2;
    audio.numFrames   = numFrames;
    audio.bitDepth    = 24;
    audio.samples.resize(2, std::vector<float>(static_cast<size_t>(numFrames)));

    uint32_t seed = 42;
    auto rng = [&]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return (static_cast<float>(seed >> 16) / 32768.f) - 1.f;
    };

    for (int f = 0; f < numFrames; ++f) {
        float noise0 = rng();
        float noise1 = rng();
        float sine   = 0.1f * std::sin(2.f * std::numbers::pi_v<float> * targetHz
                                        * static_cast<float>(f) / static_cast<float>(sr));
        audio.samples[0][static_cast<size_t>(f)] = noise0 + sine;
        audio.samples[1][static_cast<size_t>(f)] = noise1 + sine;
    }

    const auto peaks = mt::detectResonances(audio);

    REQUIRE_FALSE(peaks.empty());

    // Find closest peak to targetHz
    const mt::ResonancePeak* best = nullptr;
    float bestDist = std::numeric_limits<float>::max();
    for (const auto& p : peaks) {
        float dist = std::abs(p.freqHz - targetHz);
        if (dist < bestDist) {
            bestDist = dist;
            best     = &p;
        }
    }

    REQUIRE(best != nullptr);
    CHECK(best->freqHz >= targetHz - 200.f);
    CHECK(best->freqHz <= targetHz + 200.f);
    CHECK(best->gainDb < 0.f);
    CHECK(best->q >= 3.f);
}

// ---------------------------------------------------------------------------
// Test 3: ResonanceEq with one enabled peak attenuates at target frequency
// ---------------------------------------------------------------------------
TEST_CASE("ResonanceEq enabled peak attenuates target frequency by ≥10%") {
    const int   sr        = 44100;
    const int   numFrames = 4096;
    const float fc        = 2000.f;

    std::vector<std::vector<float>> buf(1, std::vector<float>(static_cast<size_t>(numFrames)));
    for (int f = 0; f < numFrames; ++f)
        buf[0][static_cast<size_t>(f)] = std::cos(2.f * std::numbers::pi_v<float> * fc
                                                   * static_cast<float>(f) / static_cast<float>(sr));

    // Measure RMS before
    float sumBefore = 0.f;
    for (float s : buf[0])
        sumBefore += s * s;
    const float rmsBefore = std::sqrt(sumBefore / static_cast<float>(numFrames));

    // Apply ResonanceEq with one enabled peak at fc
    mt::dsp::ResonanceEq eq;
    eq.prepare(static_cast<float>(sr), 1);
    eq.setResonances({{fc, 8.f, -12.f, true}});
    eq.process(buf, numFrames);

    // Measure RMS after
    float sumAfter = 0.f;
    for (float s : buf[0])
        sumAfter += s * s;
    const float rmsAfter = std::sqrt(sumAfter / static_cast<float>(numFrames));

    CHECK(rmsAfter < rmsBefore * 0.9f);
}

// ---------------------------------------------------------------------------
// Test 4: ResonanceEq with enabled=false → output is bitwise identical to input
// ---------------------------------------------------------------------------
TEST_CASE("ResonanceEq disabled peak → output identical to input") {
    const int   sr        = 44100;
    const int   numFrames = 4096;
    const float fc        = 2000.f;

    std::vector<std::vector<float>> buf(1, std::vector<float>(static_cast<size_t>(numFrames)));
    for (int f = 0; f < numFrames; ++f)
        buf[0][static_cast<size_t>(f)] = std::cos(2.f * std::numbers::pi_v<float> * fc
                                                   * static_cast<float>(f) / static_cast<float>(sr));

    // Keep a copy of the original
    const std::vector<float> original = buf[0];

    // Apply ResonanceEq with disabled peak
    mt::dsp::ResonanceEq eq;
    eq.prepare(static_cast<float>(sr), 1);
    eq.setResonances({{fc, 8.f, -12.f, false}});
    eq.process(buf, numFrames);

    for (int i = 0; i < numFrames; ++i)
        CHECK(buf[0][static_cast<size_t>(i)] == original[static_cast<size_t>(i)]);
}

// ---------------------------------------------------------------------------
// Test 5: detectResonances on silence → 0 resonances, no crash
// ---------------------------------------------------------------------------
TEST_CASE("detectResonances on silence → 0 resonances") {
    mt::AudioFile silence;
    silence.sampleRate  = 44100;
    silence.numChannels = 2;
    silence.numFrames   = 44100;
    silence.bitDepth    = 24;
    silence.samples.resize(2, std::vector<float>(44100, 0.f));

    const auto peaks = mt::detectResonances(silence);
    CHECK(peaks.empty());
}
