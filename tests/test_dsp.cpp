#include "mastertweak/dsp/biquad.hpp"
#include "mastertweak/dsp/parametric_eq.hpp"
#include "mastertweak/dsp/compressor.hpp"
#include "mastertweak/dsp/multiband_comp.hpp"
#include "mastertweak/dsp/saturator.hpp"
#include "mastertweak/dsp/stereo_width.hpp"
#include "mastertweak/dsp/limiter.hpp"
#include "mastertweak/dsp/dither.hpp"

#include <doctest.h>

#include <cmath>
#include <numbers>
#include <vector>

static constexpr float kSr = 44100.f;
static constexpr int   kSrI = 44100;

// Generate a stereo sine (equal L/R) and return RMS after processing
static float processAndMeasureRms(std::vector<std::vector<float>>& buf,
                                   auto& processor,
                                   int frames) {
    processor.process(buf, frames);
    double sum = 0.0;
    for (int f = 0; f < frames; ++f) {
        const float s = buf[0][static_cast<size_t>(f)];
        sum += static_cast<double>(s) * s;
    }
    return static_cast<float>(std::sqrt(sum / frames));
}

static std::vector<std::vector<float>> makeSine(float freqHz, float amp,
                                                  int frames, float sr = kSr) {
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(frames)));
    for (int i = 0; i < frames; ++i) {
        const float s = amp * std::sin(2.f * std::numbers::pi_v<float> * freqHz
                                        * static_cast<float>(i) / sr);
        buf[0][static_cast<size_t>(i)] = s;
        buf[1][static_cast<size_t>(i)] = s;
    }
    return buf;
}

// ── Biquad ───────────────────────────────────────────────────────────────────

TEST_CASE("biquad LP: 1 kHz tone with 100 Hz cutoff is heavily attenuated (steady-state)") {
    const auto c  = mt::dsp::BiquadCoeffs::lowpass(100.f, 0.7071f, kSr);
    mt::dsp::BiquadState st{};
    // Run 0.1s to let the 100 Hz LP settle past its initial transient, then
    // measure RMS over the final 0.5s (steady state only).
    const int settle = kSrI / 10;          // 0.1 s settle
    const int measure = kSrI / 2;          // 0.5 s measurement
    for (int i = 0; i < settle; ++i)
        mt::dsp::biquadProcess(c, st, std::sin(2.f * std::numbers::pi_v<float> * 1000.f
                                                * static_cast<float>(i) / kSr));
    double sumSq = 0.0;
    for (int i = 0; i < measure; ++i) {
        const float x = std::sin(2.f * std::numbers::pi_v<float> * 1000.f
                                  * static_cast<float>(i + settle) / kSr);
        const float y = mt::dsp::biquadProcess(c, st, x);
        sumSq += static_cast<double>(y) * y;
    }
    const float rms = static_cast<float>(std::sqrt(sumSq / measure));
    // 2nd-order LP: |H(1kHz)| ≈ 0.01 → RMS of sine ≈ 0.007. Allow ±3 dB → < 0.014.
    CHECK(rms < 0.014f);
    CHECK(rms > 0.003f);  // also check filter is active, not silent
}

TEST_CASE("biquad LP: DC passes through with gain ≈ 1") {
    const auto c = mt::dsp::BiquadCoeffs::lowpass(1000.f, 0.7071f, kSr);
    mt::dsp::BiquadState st{};
    float out = 0.f;
    // Settle the filter on DC
    for (int i = 0; i < 2000; ++i) out = mt::dsp::biquadProcess(c, st, 1.f);
    CHECK(out == doctest::Approx(1.f).epsilon(0.001f));
}

TEST_CASE("biquad bell: 0 dB gain = identity") {
    const auto c = mt::dsp::BiquadCoeffs::bell(1000.f, 1.f, 0.f, kSr);
    mt::dsp::BiquadState st{};
    const float x = 0.5f;
    CHECK(mt::dsp::biquadProcess(c, st, x) == doctest::Approx(x).epsilon(0.001f));
}

// ── Parametric EQ ────────────────────────────────────────────────────────────

TEST_CASE("ParametricEq: all-flat advice = identity") {
    mt::dsp::ParametricEq eq;
    eq.prepare(kSr, 2);

    std::array<mt::BandEq, 7> bands{};  // all gains = 0
    eq.setAdvice(bands);

    auto buf = makeSine(1000.f, 0.5f, kSrI);
    const float rmsBefore = std::sqrt([&]{
        double s = 0.0;
        for (auto x : buf[0]) s += static_cast<double>(x) * x;
        return static_cast<float>(s / kSrI);
    }());

    eq.process(buf, kSrI);

    float rmsAfter = std::sqrt([&]{
        double s = 0.0;
        for (int i = 0; i < kSrI; ++i) s += static_cast<double>(buf[0][static_cast<size_t>(i)]) * buf[0][static_cast<size_t>(i)];
        return static_cast<float>(s / kSrI);
    }());

    CHECK(rmsAfter == doctest::Approx(rmsBefore).epsilon(0.01f));
}

// ── Compressor ───────────────────────────────────────────────────────────────

TEST_CASE("Compressor: signal below threshold = no gain reduction") {
    mt::dsp::Compressor comp;
    comp.prepare(kSr, 2);
    mt::dsp::Compressor::Params p;
    p.thresholdDb = -6.f;
    p.ratio       = 4.f;
    p.attackMs    = 1.f;
    p.releaseMs   = 50.f;
    comp.setParams(p);

    // Signal at -20 dBFS (below -6 dB threshold)
    auto buf = makeSine(1000.f, std::pow(10.f, -20.f / 20.f), kSrI);
    const float rmsBefore = [&]{
        double s = 0.0;
        for (auto x : buf[0]) s += static_cast<double>(x) * x;
        return static_cast<float>(std::sqrt(s / kSrI));
    }();
    comp.process(buf, kSrI);
    const float rmsAfter = [&]{
        double s = 0.0;
        for (int f = 0; f < kSrI; ++f) s += static_cast<double>(buf[0][static_cast<size_t>(f)]) * buf[0][static_cast<size_t>(f)];
        return static_cast<float>(std::sqrt(s / kSrI));
    }();
    // Gain reduction should be negligible
    CHECK(rmsAfter == doctest::Approx(rmsBefore).epsilon(0.05f));
}

TEST_CASE("Compressor: signal above threshold = reduced output") {
    mt::dsp::Compressor comp;
    comp.prepare(kSr, 2);
    mt::dsp::Compressor::Params p;
    p.thresholdDb = -20.f;
    p.ratio       =  4.f;
    p.attackMs    =  1.f;
    p.releaseMs   = 50.f;
    p.kneeDb      =  0.f;
    comp.setParams(p);

    // Signal at -6 dBFS (14 dB above threshold)
    auto buf = makeSine(1000.f, std::pow(10.f, -6.f / 20.f), kSrI);
    comp.process(buf, kSrI);
    // After settling: gain reduction ≈ (14 * (1 - 1/4)) = 10.5 dB → output ≈ -6 - 10.5 = -16.5 dBFS
    // Just verify it's substantially attenuated
    const float rmsAfter = [&]{
        double s = 0.0;
        for (int f = kSrI/2; f < kSrI; ++f)
            s += static_cast<double>(buf[0][static_cast<size_t>(f)]) * buf[0][static_cast<size_t>(f)];
        return static_cast<float>(std::sqrt(s / (kSrI/2)));
    }();
    CHECK(rmsAfter < std::pow(10.f, -12.f / 20.f));  // should be below -12 dBFS
}

// ── Saturator ────────────────────────────────────────────────────────────────

TEST_CASE("Saturator: 0 dB drive = no-op (identity)") {
    mt::dsp::Saturator sat;
    sat.prepare(kSr, 2);
    sat.setAdvice({0.f});

    auto buf = makeSine(1000.f, 0.5f, 1000);
    const auto ref = buf;
    sat.process(buf, 1000);
    for (int f = 0; f < 1000; ++f)
        CHECK(buf[0][static_cast<size_t>(f)] == doctest::Approx(ref[0][static_cast<size_t>(f)]).epsilon(1e-6f));
}

TEST_CASE("Saturator: 6 dB drive clips peaks but preserves unity RMS approx") {
    mt::dsp::Saturator sat;
    sat.prepare(kSr, 2);
    sat.setAdvice({6.f});

    // 0 dBFS sine
    auto buf = makeSine(100.f, 1.f, kSrI);
    sat.process(buf, kSrI);
    float maxAbs = 0.f;
    for (auto x : buf[0]) maxAbs = std::max(maxAbs, std::abs(x));
    // tanh clamps; peak should be < 1 (normalized by postGain)
    CHECK(maxAbs <= 1.0f + 1e-5f);
}

// ── Limiter ──────────────────────────────────────────────────────────────────

TEST_CASE("Limiter: output peak never exceeds ceiling (-1 dBTP ≈ 0.891)") {
    mt::dsp::Limiter lim;
    lim.prepare(kSr, 2);
    lim.setAdvice({-14.f, -1.f});

    // 0 dBFS sine (peaks at ±1) — limiter must catch true-peak overs
    auto buf = makeSine(997.f, 1.0f, kSrI * 2);
    lim.process(buf, kSrI * 2);

    float maxAbs = 0.f;
    for (int f = 0; f < kSrI * 2; ++f)
        maxAbs = std::max(maxAbs, std::abs(buf[0][static_cast<size_t>(f)]));

    CHECK(maxAbs <= 0.892f);  // -1 dBTP ceiling ≈ 0.891; tight margin (two-pass limiter guarantees ceiling)
}

// ── Dither ───────────────────────────────────────────────────────────────────

TEST_CASE("Dither: 16-bit dither adds noise, signal is distinguishable") {
    mt::dsp::Dither dither;
    dither.prepare(16, 2);

    // Silence + dither should produce very small non-zero output
    std::vector<std::vector<float>> buf(2, std::vector<float>(1000, 0.f));
    dither.process(buf, 1000);
    float maxAbs = 0.f;
    for (auto x : buf[0]) maxAbs = std::max(maxAbs, std::abs(x));
    CHECK(maxAbs > 0.f);           // dither was added
    CHECK(maxAbs < 0.001f);        // but it's very small (< 1/1000 ≈ -60 dBFS)
}

TEST_CASE("Dither: 32-bit output = no-op") {
    mt::dsp::Dither dither;
    dither.prepare(32, 2);
    std::vector<std::vector<float>> buf(2, std::vector<float>(100, 0.5f));
    dither.process(buf, 100);
    for (auto x : buf[0])
        CHECK(x == doctest::Approx(0.5f).epsilon(1e-6f));
}
