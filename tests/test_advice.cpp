#include "mastertweak/advice.hpp"
#include "mastertweak/preset.hpp"

#include <doctest.h>

// Build a snapshot where all bands are at the preset targets (zero error)
static mt::AnalysisSnapshot makeOnTargetSnapshot(const mt::PresetData& preset) {
    mt::AnalysisSnapshot snap;
    for (size_t i = 0; i < static_cast<size_t>(mt::AnalysisSnapshot::kNumBands); ++i) {
        // avgRmsDb == peakRmsDb == bandRmsDb[i]  →  refDb == bandRmsDb[i]  →  eqGain == 0
        snap.bands[i].avgRmsDb  = preset.bandRmsDb[i];
        snap.bands[i].peakRmsDb = preset.bandRmsDb[i];
        // Correlation exactly at the floor
        snap.bands[i].correlation = preset.bandMinCorr[i];
        // Crest exactly at target
        snap.bands[i].crestDb = preset.bandTransientDb[i];
    }
    snap.overallAvgDb  = preset.overallRmsDb;
    snap.overallPeakDb = preset.overallRmsDb;
    snap.overallCorr   = preset.overallMinCorr;
    return snap;
}

static mt::PresetData makeTestPreset() {
    mt::PresetData p;
    p.name = "Test";
    for (size_t i = 0; i < static_cast<size_t>(mt::PresetData::kNumBands); ++i) {
        p.bandRmsDb[i]      = -20.f;
        p.bandMinCorr[i]    =  0.6f;
        p.bandTransientDb[i] = 10.f;
    }
    p.overallRmsDb   = -16.f;
    p.overallMinCorr =  0.6f;
    return p;
}

TEST_CASE("zero-error snapshot → all EQ gains are 0") {
    const auto preset = makeTestPreset();
    const auto snap   = makeOnTargetSnapshot(preset);
    const auto advice = mt::deriveAdvice(snap, preset);

    for (const auto& eq : advice.eq)
        CHECK(eq.gainDb == doctest::Approx(0.f));
}

TEST_CASE("+6 dB excess in one band → EQ gain is -6 dB") {
    auto preset = makeTestPreset();
    auto snap   = makeOnTargetSnapshot(preset);
    // Band 3 (Mids) is 6 dB too hot
    snap.bands[3].avgRmsDb  = -14.f;
    snap.bands[3].peakRmsDb = -14.f;

    const auto advice = mt::deriveAdvice(snap, preset);
    CHECK(advice.eq[3].gainDb == doctest::Approx(-6.f));
    // Other bands should be flat
    CHECK(advice.eq[0].gainDb == doctest::Approx(0.f));
    CHECK(advice.eq[6].gainDb == doctest::Approx(0.f));
}

TEST_CASE("deficit < 0.5 dB → EQ gain rounded to 0") {
    auto preset = makeTestPreset();
    auto snap   = makeOnTargetSnapshot(preset);
    snap.bands[2].avgRmsDb  = -20.3f;  // only 0.3 dB off
    snap.bands[2].peakRmsDb = -20.3f;

    const auto advice = mt::deriveAdvice(snap, preset);
    CHECK(advice.eq[2].gainDb == doctest::Approx(0.f));
}

TEST_CASE("on-target → multiband comp ratio ≈ 1.1 (minimum, no excess)") {
    const auto preset = makeTestPreset();
    const auto snap   = makeOnTargetSnapshot(preset);
    const auto advice = mt::deriveAdvice(snap, preset);
    for (const auto& c : advice.mbComp)
        CHECK(c.ratio == doctest::Approx(1.1f).epsilon(0.01f));
}

TEST_CASE("excess level → multiband comp ratio increases") {
    auto preset = makeTestPreset();
    auto snap   = makeOnTargetSnapshot(preset);
    snap.bands[1].avgRmsDb  = -12.f;  // 8 dB excess in Lows
    snap.bands[1].peakRmsDb = -12.f;

    const auto advice = mt::deriveAdvice(snap, preset);
    // ratio = clamp(1 + 8*0.25, 1.1, 8) = clamp(3.0, 1.1, 8) = 3.0
    CHECK(advice.mbComp[1].ratio == doctest::Approx(3.f).epsilon(0.05f));
}

TEST_CASE("mixbus comp threshold = overallRmsDb - 6") {
    const auto preset = makeTestPreset();
    const auto snap   = makeOnTargetSnapshot(preset);
    const auto advice = mt::deriveAdvice(snap, preset);
    CHECK(advice.mixbusComp.thresholdDb == doctest::Approx(preset.overallRmsDb - 6.f));
}

TEST_CASE("Sub and Air EQ bands are marked as shelves") {
    const auto preset = makeTestPreset();
    const auto snap   = makeOnTargetSnapshot(preset);
    // Force non-zero gain so shelf flag matters
    auto snap2 = snap;
    snap2.bands[0].avgRmsDb = -14.f;  // Sub is too hot
    snap2.bands[0].peakRmsDb = -14.f;
    snap2.bands[6].avgRmsDb = -14.f;  // Air is too hot
    snap2.bands[6].peakRmsDb = -14.f;

    const auto advice = mt::deriveAdvice(snap2, preset);
    CHECK(advice.eq[0].isShelf == true);   // Sub
    CHECK(advice.eq[6].isShelf == true);   // Air
    CHECK(advice.eq[3].isShelf == false);  // Mids
}

TEST_CASE("limiter ceiling is -1 dBTP") {
    const auto advice = mt::deriveAdvice(makeOnTargetSnapshot(makeTestPreset()), makeTestPreset());
    CHECK(advice.limiter.ceilingDb == doctest::Approx(-1.f));
}
