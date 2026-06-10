#include "mastertweak/dsp/lufs_analyser.hpp"
#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/preset.hpp"
#include <doctest.h>
#include <cmath>
#include <numbers>
#include <vector>

// ── LufsAnalyser::measureWithLra() ───────────────────────────────────────────

TEST_CASE("measureWithLra: silence returns floor values") {
    mt::dsp::LufsAnalyser la;
    la.prepare(48000.f, 2);
    const int frames = static_cast<int>(10.f * 48000.f);
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(frames), 0.f));
    const auto m = la.measureWithLra(buf, frames);
    CHECK(m.integratedLufs == doctest::Approx(-70.f));
    CHECK(m.lra == 0.f);
}

TEST_CASE("measureWithLra: constant-level signal gives LRA near zero") {
    const float sr = 48000.f;
    const int frames = static_cast<int>(10.f * sr);
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(frames)));
    for (int f = 0; f < frames; ++f) {
        const float s = 0.1f * std::sin(2.f * std::numbers::pi_v<float> * 440.f
                                         * static_cast<float>(f) / sr);
        buf[0][static_cast<size_t>(f)] = s;
        buf[1][static_cast<size_t>(f)] = s;
    }
    mt::dsp::LufsAnalyser la;
    la.prepare(sr, 2);
    const auto m = la.measureWithLra(buf, frames);
    CHECK(m.lra < 1.f);
    // A -20 dBFS sine (amplitude 0.1) should produce ~ -21 LUFS
    CHECK(m.integratedLufs > -25.f);
    CHECK(m.integratedLufs < -18.f);
}

TEST_CASE("measureWithLra: two-level signal gives LRA in expected range") {
    // 15 s quiet (-30 dBFS) + 15 s loud (-10 dBFS) => LRA ≈ 20 LU
    const float sr    = 48000.f;
    const int segLen  = static_cast<int>(15.f * sr);
    const int frames  = 2 * segLen;
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(frames)));
    const float ampQ = 0.03162f;   // 10^(-30/20)
    const float ampL = 0.3162f;    // 10^(-10/20)
    for (int f = 0; f < segLen; ++f) {
        const float s = ampQ * std::sin(2.f * std::numbers::pi_v<float> * 440.f
                                         * static_cast<float>(f) / sr);
        buf[0][static_cast<size_t>(f)] = s;
        buf[1][static_cast<size_t>(f)] = s;
    }
    for (int f = segLen; f < frames; ++f) {
        const float s = ampL * std::sin(2.f * std::numbers::pi_v<float> * 440.f
                                         * static_cast<float>(f - segLen) / sr);
        buf[0][static_cast<size_t>(f)] = s;
        buf[1][static_cast<size_t>(f)] = s;
    }
    mt::dsp::LufsAnalyser la;
    la.prepare(sr, 2);
    const auto m = la.measureWithLra(buf, frames);
    CHECK(m.lra > 17.f);
    CHECK(m.lra < 22.f);
}

TEST_CASE("measureWithLra: signal shorter than 3 s gives LRA = 0") {
    // 2 s non-silent signal: not enough to fill even one 3 s LRA block → lra must be 0.f
    const float sr    = 48000.f;
    const int frames  = static_cast<int>(2.f * sr);
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(frames)));
    for (int f = 0; f < frames; ++f) {
        const float s = 0.1f * std::sin(2.f * std::numbers::pi_v<float> * 440.f
                                         * static_cast<float>(f) / sr);
        buf[0][static_cast<size_t>(f)] = s;
        buf[1][static_cast<size_t>(f)] = s;
    }
    mt::dsp::LufsAnalyser la;
    la.prepare(sr, 2);
    const auto m = la.measureWithLra(buf, frames);
    CHECK(m.lra == 0.f);
}

// ── deriveAdvice() LRA offset ─────────────────────────────────────────────────

static mt::PresetData makeLraTestPreset() {
    mt::PresetData p;
    p.name = "LRA Test";
    for (size_t i = 0; i < 7; ++i) {
        p.bandRmsDb[i]       = -20.f;
        p.bandMinCorr[i]     =  0.6f;
        p.bandTransientDb[i] = 10.f;
    }
    p.overallRmsDb   = -16.f;  // baseline targetLufsApprox = -16 + 3 = -13
    p.overallMinCorr =  0.6f;
    return p;
}

static mt::AnalysisSnapshot makeSnapWithLra(const mt::PresetData& preset, float lra) {
    mt::AnalysisSnapshot snap;
    for (size_t i = 0; i < 7; ++i) {
        snap.bands[i].avgRmsDb    = preset.bandRmsDb[i];
        snap.bands[i].p50RmsDb    = preset.bandRmsDb[i];
        snap.bands[i].p95RmsDb    = preset.bandRmsDb[i];
        snap.bands[i].correlation = preset.bandMinCorr[i];
        snap.bands[i].crestDb     = preset.bandTransientDb[i];
    }
    snap.overallAvgDb  = preset.overallRmsDb;
    snap.overallPeakDb = preset.overallRmsDb;
    snap.overallCorr   = preset.overallMinCorr;
    snap.lraLu         = lra;
    return snap;
}

TEST_CASE("deriveAdvice: low LRA (2 LU) lowers targetLufsApprox by 3 LU") {
    const auto preset = makeLraTestPreset();
    const auto snap   = makeSnapWithLra(preset, 2.f);
    // offset = clamp((2 - 12) * 0.30, -3, 3) = -3
    // targetLufsApprox = -13 + (-3) = -16
    const auto advice = mt::deriveAdvice(snap, preset);
    CHECK(advice.limiter.targetLufsApprox == doctest::Approx(-16.f));
}

TEST_CASE("deriveAdvice: neutral LRA (12 LU) leaves targetLufsApprox unchanged") {
    const auto preset = makeLraTestPreset();
    const auto snap   = makeSnapWithLra(preset, 12.f);
    // offset = clamp(0, -3, 3) = 0
    // targetLufsApprox = -13
    const auto advice = mt::deriveAdvice(snap, preset);
    CHECK(advice.limiter.targetLufsApprox == doctest::Approx(-13.f));
}

TEST_CASE("deriveAdvice: high LRA (22 LU) raises targetLufsApprox by 3 LU") {
    const auto preset = makeLraTestPreset();
    const auto snap   = makeSnapWithLra(preset, 22.f);
    // offset = clamp((22 - 12) * 0.30, -3, 3) = 3
    // targetLufsApprox = -13 + 3 = -10
    const auto advice = mt::deriveAdvice(snap, preset);
    CHECK(advice.limiter.targetLufsApprox == doctest::Approx(-10.f));
}
