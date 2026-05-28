#include "mastertweak/dsp/gain_stager.hpp"

#include <doctest.h>

#include <cmath>
#include <numbers>
#include <vector>

using mt::dsp::GainStager;
using mt::dsp::StageLevelReport;

static constexpr int kSr     = 44100;
static constexpr int kFrames = kSr;  // 1 second

// Build a stereo sine buffer at a given peak amplitude.
static std::vector<std::vector<float>> makeSine(float amplitude) {
    std::vector<std::vector<float>> buf(2, std::vector<float>(static_cast<size_t>(kFrames)));
    for (int f = 0; f < kFrames; ++f) {
        const float s = amplitude * std::sin(
            2.f * std::numbers::pi_v<float> * 1000.f * static_cast<float>(f) / kSr);
        buf[0][static_cast<size_t>(f)] = s;
        buf[1][static_cast<size_t>(f)] = s;
    }
    return buf;
}

// ── measureRmsDb ─────────────────────────────────────────────────────────────

TEST_CASE("GainStager::measureRmsDb returns correct value for known sine") {
    const float amp = std::pow(10.f, -18.f / 20.f);
    auto buf = makeSine(amp);
    const float rms = GainStager::measureRmsDb(buf, kFrames);
    const float expected = 20.f * std::log10(amp / std::sqrt(2.f));
    CHECK(rms == doctest::Approx(expected).epsilon(0.5f));
}

TEST_CASE("GainStager::measureRmsDb returns -100 for silence") {
    auto buf = makeSine(0.f);
    CHECK(GainStager::measureRmsDb(buf, kFrames) <= -99.f);
}

// ── measurePeakDb ─────────────────────────────────────────────────────────────

TEST_CASE("GainStager::measurePeakDb returns correct value for known sine") {
    const float amp = std::pow(10.f, -6.f / 20.f);
    auto buf = makeSine(amp);
    const float peak = GainStager::measurePeakDb(buf, kFrames);
    CHECK(peak == doctest::Approx(-6.f).epsilon(0.1f));
}

// ── restoreRms ───────────────────────────────────────────────────────────────

TEST_CASE("GainStager::restoreRms brings RMS to target") {
    const float amp = std::pow(10.f, -30.f / 20.f);
    auto buf = makeSine(amp);

    GainStager gs;
    const float targetRms = -18.f;
    auto report = gs.restoreRms(buf, kFrames, targetRms);

    CHECK(report.outputRmsDb == doctest::Approx(targetRms).epsilon(0.5f));
    CHECK(report.trimDb > 0.f);
    CHECK(report.inputRmsDb < report.outputRmsDb);
}

TEST_CASE("GainStager::restoreRms clamps trim to +12 dB max") {
    const float amp = std::pow(10.f, -50.f / 20.f);
    auto buf = makeSine(amp);

    GainStager gs;
    auto report = gs.restoreRms(buf, kFrames, -18.f);

    CHECK(report.trimDb == doctest::Approx(12.f).epsilon(0.01f));
}

TEST_CASE("GainStager::restoreRms clamps trim to -12 dB min") {
    // Sine at amplitude 1.0 → peak ≈ 0 dBFS, RMS ≈ -3 dBFS
    // Target -18 dBFS → needs ~-15 dB, clamped to -12
    const float amp = 1.f;
    auto buf = makeSine(amp);

    GainStager gs;
    auto report = gs.restoreRms(buf, kFrames, -18.f);

    CHECK(report.trimDb == doctest::Approx(-12.f).epsilon(0.01f));
}

TEST_CASE("GainStager::restoreRms does nothing on silence") {
    auto buf = makeSine(0.f);
    GainStager gs;
    auto report = gs.restoreRms(buf, kFrames, -18.f);
    CHECK(report.trimDb == 0.f);
}

// ── trimPeak ─────────────────────────────────────────────────────────────────

TEST_CASE("GainStager::trimPeak reduces signal exceeding threshold") {
    const float amp = std::pow(10.f, 3.f / 20.f);  // +3 dBFS peak
    auto buf = makeSine(amp);

    GainStager gs;
    auto report = gs.trimPeak(buf, kFrames, -3.f);

    CHECK(report.outputPeakDb == doctest::Approx(-3.f).epsilon(0.1f));
    CHECK(report.trimDb < 0.f);
}

TEST_CASE("GainStager::trimPeak does not boost signal below threshold") {
    const float amp = std::pow(10.f, -6.f / 20.f);  // -6 dBFS peak
    auto buf = makeSine(amp);
    const auto bufCopy = buf;

    GainStager gs;
    auto report = gs.trimPeak(buf, kFrames, -3.f);

    CHECK(report.trimDb == 0.f);
    CHECK(report.outputPeakDb == doctest::Approx(report.inputPeakDb).epsilon(0.01f));
    for (int ch = 0; ch < 2; ++ch)
        for (int f = 0; f < kFrames; ++f)
            CHECK(buf[static_cast<size_t>(ch)][static_cast<size_t>(f)]
                  == bufCopy[static_cast<size_t>(ch)][static_cast<size_t>(f)]);
}

TEST_CASE("GainStager::trimPeak report fields are populated correctly") {
    const float amp = std::pow(10.f, 3.f / 20.f);
    auto buf = makeSine(amp);

    GainStager gs;
    auto report = gs.trimPeak(buf, kFrames, -3.f);

    CHECK(report.inputPeakDb  > -99.f);
    CHECK(report.inputRmsDb   > -99.f);
    CHECK(report.outputPeakDb > -99.f);
    CHECK(report.outputRmsDb  > -99.f);
    CHECK(report.trimDb != 0.f);
}
