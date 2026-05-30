#include <doctest.h>
#include "../gui/MidiMapping.h"

using namespace gui;

// ── ccToValue ────────────────────────────────────────────────────────────────

TEST_CASE("ccToValue: 0 maps to min") {
    CHECK(ccToValue(0, -12.0, 12.0) == doctest::Approx(-12.0));
}

TEST_CASE("ccToValue: 127 maps to max") {
    CHECK(ccToValue(127, -12.0, 12.0) == doctest::Approx(12.0));
}

TEST_CASE("ccToValue: 64 maps proportionally") {
    const double v = ccToValue(64, 0.0, 6.0);
    CHECK(v == doctest::Approx(64.0 / 127.0 * 6.0));
}

TEST_CASE("ccToValue: clamps below 0") {
    CHECK(ccToValue(-5, 0.0, 1.0) == doctest::Approx(0.0));
}

TEST_CASE("ccToValue: clamps above 127") {
    CHECK(ccToValue(200, 0.0, 1.0) == doctest::Approx(1.0));
}

// ── valueToCC ────────────────────────────────────────────────────────────────

TEST_CASE("valueToCC: min maps to 0") {
    CHECK(valueToCC(-12.0, -12.0, 12.0) == 0);
}

TEST_CASE("valueToCC: max maps to 127") {
    CHECK(valueToCC(12.0, -12.0, 12.0) == 127);
}

TEST_CASE("valueToCC: degenerate range returns 0") {
    CHECK(valueToCC(5.0, 5.0, 5.0) == 0);
}

// ── Roundtrip ─────────────────────────────────────────────────────────────────

TEST_CASE("ccToValue / valueToCC roundtrip over EQ range") {
    const double min = -12.0, max = 12.0;
    for (int cc = 0; cc <= 127; cc += 13) {
        const double v    = ccToValue(cc, min, max);
        const int    back = valueToCC(v, min, max);
        CHECK(back == cc);
    }
}

TEST_CASE("ccToValue / valueToCC roundtrip over mixbus threshold range") {
    const double min = -40.0, max = 0.0;
    for (int cc = 0; cc <= 127; cc += 7) {
        const double v    = ccToValue(cc, min, max);
        const int    back = valueToCC(v, min, max);
        CHECK(back == cc);
    }
}

// ── nanoKontrol2 defaults ────────────────────────────────────────────────────

TEST_CASE("nanoKontrol2: EQ band 0 is CC 0") {
    const auto m = MidiMap::nanoKontrol2();
    CHECK(m.ccFor(MidiParam::EqBand0).cc == 0);
}

TEST_CASE("nanoKontrol2: EQ band 6 is CC 6") {
    const auto m = MidiMap::nanoKontrol2();
    CHECK(m.ccFor(MidiParam::EqBand6).cc == 6);
}

TEST_CASE("nanoKontrol2: SatDrive is CC 16") {
    const auto m = MidiMap::nanoKontrol2();
    CHECK(m.ccFor(MidiParam::SatDrive).cc == 16);
}

TEST_CASE("nanoKontrol2: MixbusThresh is CC 17") {
    const auto m = MidiMap::nanoKontrol2();
    CHECK(m.ccFor(MidiParam::MixbusThresh).cc == 17);
}

TEST_CASE("nanoKontrol2: LimCeiling is CC 19") {
    const auto m = MidiMap::nanoKontrol2();
    CHECK(m.ccFor(MidiParam::LimCeiling).cc == 19);
}

TEST_CASE("nanoKontrol2: transport Play stored as 41") {
    const auto m = MidiMap::nanoKontrol2();
    CHECK(m.noteBindings[0].note == 41);
}

TEST_CASE("nanoKontrol2: transport Stop stored as 42") {
    const auto m = MidiMap::nanoKontrol2();
    CHECK(m.noteBindings[1].note == 42);
}

// ── xTouchMini defaults ──────────────────────────────────────────────────────

TEST_CASE("xTouchMini: EQ band 0 is CC 1") {
    const auto m = MidiMap::xTouchMini();
    CHECK(m.ccFor(MidiParam::EqBand0).cc == 1);
}

TEST_CASE("xTouchMini: EQ band 6 is CC 7") {
    const auto m = MidiMap::xTouchMini();
    CHECK(m.ccFor(MidiParam::EqBand6).cc == 7);
}

TEST_CASE("xTouchMini: SatDrive is CC 8") {
    const auto m = MidiMap::xTouchMini();
    CHECK(m.ccFor(MidiParam::SatDrive).cc == 8);
}

TEST_CASE("xTouchMini: transport Play is Note 89") {
    const auto m = MidiMap::xTouchMini();
    CHECK(m.noteBindings[0].note == 89);
}

TEST_CASE("xTouchMini: transport Stop is Note 90") {
    const auto m = MidiMap::xTouchMini();
    CHECK(m.noteBindings[1].note == 90);
}

// ── defaultMap ───────────────────────────────────────────────────────────────

TEST_CASE("defaultMap CC bindings equal nanoKontrol2") {
    const auto d = MidiMap::defaultMap();
    const auto n = MidiMap::nanoKontrol2();
    for (int i = 0; i < kMidiParamCcCount; ++i)
        CHECK(d.ccBindings[i].cc == n.ccBindings[i].cc);
}
