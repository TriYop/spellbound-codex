#include "mastertweak/advice.hpp"
#include "mastertweak/preset.hpp"

#include <doctest.h>

// Build a snapshot where all bands are at the preset targets (zero error)
static mt::AnalysisSnapshot makeOnTargetSnapshot(const mt::PresetData& preset) {
    mt::AnalysisSnapshot snap;
    for (size_t i = 0; i < static_cast<size_t>(mt::AnalysisSnapshot::kNumBands); ++i) {
        snap.bands[i].avgRmsDb   = preset.bandRmsDb[i];
        snap.bands[i].peakRmsDb  = preset.bandRmsDb[i];
        snap.bands[i].p50RmsDb   = preset.bandRmsDb[i];
        snap.bands[i].p95RmsDb   = preset.bandRmsDb[i];
        snap.bands[i].correlation = preset.bandMinCorr[i];
        snap.bands[i].crestDb    = preset.bandTransientDb[i];
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
    snap.bands[3].p50RmsDb  = -14.f;
    snap.bands[3].p95RmsDb  = -14.f;

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
    snap.bands[2].p50RmsDb  = -20.3f;
    snap.bands[2].p95RmsDb  = -20.3f;

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
    snap.bands[1].p50RmsDb  = -12.f;
    snap.bands[1].p95RmsDb  = -12.f;

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
    snap2.bands[0].avgRmsDb  = -14.f;  // Sub is too hot
    snap2.bands[0].peakRmsDb = -14.f;
    snap2.bands[0].p50RmsDb  = -14.f;
    snap2.bands[0].p95RmsDb  = -14.f;
    snap2.bands[6].avgRmsDb  = -14.f;  // Air is too hot
    snap2.bands[6].peakRmsDb = -14.f;
    snap2.bands[6].p50RmsDb  = -14.f;
    snap2.bands[6].p95RmsDb  = -14.f;

    const auto advice = mt::deriveAdvice(snap2, preset);
    CHECK(advice.eq[0].isShelf == true);   // Sub
    CHECK(advice.eq[6].isShelf == true);   // Air
    CHECK(advice.eq[3].isShelf == false);  // Mids
}

TEST_CASE("limiter ceiling is -1 dBTP") {
    const auto advice = mt::deriveAdvice(makeOnTargetSnapshot(makeTestPreset()), makeTestPreset());
    CHECK(advice.limiter.ceilingDb == doctest::Approx(-1.f));
}

// Regression guard for core/src/advice.cpp's fromCommonAdvice() field-by-field mapping.
// Every input below is a distinct value so a transposition anywhere in that ~30-field
// mapping (e.g. swapping attackMs/releaseMs, or ratio/makeupDb) would change one of
// these asserted values -- fields not already exercised by the test cases above
// (eq[i].q/freqHz, mbComp[i].thresholdDb/attackMs/releaseMs, width[i].width,
// mixbusComp.ratio/attackMs/releaseMs/makeupDb, saturator.driveDb).
// Expected values hand-derived from AudioPluginsCommon::analysis::deriveAdvice's
// formula (Common/src/analysis/AdviceSet.cpp), band index 3 ("Mids").
TEST_CASE("deriveAdvice: field-by-field regression guard against mapping transpositions") {
    mt::PresetData preset;
    preset.name = "Regression";
    preset.bandRmsDb[3]       = -18.f;
    preset.bandMinCorr[3]     =   0.5f;
    preset.bandTransientDb[3] =   9.f;
    preset.overallRmsDb   = -14.f;
    preset.overallMinCorr =   0.55f;

    mt::AnalysisSnapshot snap;
    snap.bands[3].p50RmsDb    = -10.f;
    snap.bands[3].p95RmsDb    =  -6.f;
    snap.bands[3].correlation =   0.9f;
    snap.bands[3].crestDb     =   5.f;
    snap.overallAvgDb  = -10.f;
    snap.overallPeakDb =  -6.f;

    const auto advice = mt::deriveAdvice(snap, preset);

    // Band 3 EQ: refDb = (p50 + p95)/2 = -8; gain = clamp(bandRmsDb - refDb) = -10 dB;
    // |gain|=10 >= 9 -> q = 2.0; freqHz = BandConfig::bandCenterHz[Mids] = 1000 Hz.
    CHECK(advice.eq[3].gainDb == doctest::Approx(-10.f));
    CHECK(advice.eq[3].q      == doctest::Approx(2.f));
    CHECK(advice.eq[3].freqHz == doctest::Approx(1000.f));

    // Band 3 multiband comp: excess = refDb - bandRmsDb = 10; threshold = bandRmsDb - 3;
    // targetCrest = bandTransientDb = 9 -> attack bracket "9 > 8" = 5 ms;
    // releaseMs = per-band release table[3] = 100 ms.
    CHECK(advice.mbComp[3].thresholdDb == doctest::Approx(-21.f));
    CHECK(advice.mbComp[3].attackMs    == doctest::Approx(5.f));
    CHECK(advice.mbComp[3].releaseMs   == doctest::Approx(100.f));

    // Band 3 width: corrDelta = correlation - bandMinCorr = 0.4; width = clamp(1 + 0.4*0.5) = 1.2
    CHECK(advice.width[3].width == doctest::Approx(1.2f));

    // Mixbus comp: overallDb = (overallAvgDb + overallPeakDb)/2 = -8;
    // overallExcess = overallDb - overallRmsDb = 6.
    CHECK(advice.mixbusComp.ratio     == doctest::Approx(2.9f));
    CHECK(advice.mixbusComp.attackMs  == doctest::Approx(15.f));
    CHECK(advice.mixbusComp.releaseMs == doctest::Approx(130.f));
    // makeupDb = clamp(max(0, overallDb - threshold) * (1 - 1/ratio) + max(0, overallRmsDb - overallDb), 0, 18)
    //          = clamp(12 * (1 - 1/2.9) + 0, 0, 18) ≈ 7.862069
    CHECK(advice.mixbusComp.makeupDb == doctest::Approx(7.862069f).epsilon(0.001));

    // Saturator: crestDeficit = avg over 7 bands of (crestDb - bandTransientDb);
    // only band 3 is non-zero here: (5 - 9)/7 = -0.571429;
    // driveDb = clamp(-crestDeficit * 0.4, 0, 6) ≈ 0.228571
    CHECK(advice.saturator.driveDb == doctest::Approx(0.228571f).epsilon(0.001));
}
