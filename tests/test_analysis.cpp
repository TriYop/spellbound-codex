#include "mastertweak/analysis.hpp"
#include "mastertweak/io.hpp"

#include <doctest.h>

#include <cmath>
#include <numbers>

// Build a stereo sine AudioFile with the given frequency, amplitude, and duration
static mt::AudioFile makeStereoSine(float freqHz, float ampLinear,
                                     int sr = 44100, float durSec = 2.f) {
    mt::AudioFile f;
    f.sampleRate  = sr;
    f.numChannels = 2;
    f.numFrames   = static_cast<int>(static_cast<float>(sr) * durSec);
    f.bitDepth    = 24;
    f.samples.resize(2, std::vector<float>(static_cast<size_t>(f.numFrames)));
    for (int i = 0; i < f.numFrames; ++i) {
        const float s = ampLinear * std::sin(2.f * std::numbers::pi_v<float> * freqHz
                                              * static_cast<float>(i) / static_cast<float>(sr));
        f.samples[0][static_cast<size_t>(i)] = s;
        f.samples[1][static_cast<size_t>(i)] = s;  // equal L/R = perfect mono compatibility
    }
    return f;
}

// Expected RMS of a sine with amplitude A: A / √2
static float sineRmsDb(float ampLinear) {
    return 20.f * std::log10(ampLinear / std::sqrt(2.f));
}

TEST_CASE("1 kHz sine lands in Mids band (index 3) and not in Sub (index 0)") {
    // 1 kHz is between the 500 Hz and 2 kHz crossovers → Mids band (index 3)
    const auto audio = makeStereoSine(1000.f, 0.3f);
    const auto snap  = mt::analyseFile(audio);

    // Mids band should be well above -60 dBFS
    CHECK(snap.bands[3].avgRmsDb > -30.f);
    // Sub band (0–80 Hz) should be essentially silent
    CHECK(snap.bands[0].avgRmsDb < -50.f);
    // Air band (16 kHz+) should be essentially silent
    CHECK(snap.bands[6].avgRmsDb < -50.f);
}

TEST_CASE("RMS level is within ±2 dB of expected sine RMS") {
    // A -6 dBFS sine: amplitude = 10^(-6/20) ≈ 0.5012
    const float amp = std::pow(10.f, -6.f / 20.f);
    const auto  audio = makeStereoSine(1000.f, amp, 44100, 3.f);
    const auto  snap  = mt::analyseFile(audio);

    const float expected = sineRmsDb(amp);  // ≈ −9 dBFS
    // avgRmsDb for the Mids band should be close to the expected sine RMS
    // (some energy leaks into adjacent bands due to filter roll-off; allow ±3 dB)
    CHECK(snap.bands[3].avgRmsDb > expected - 3.f);
    CHECK(snap.bands[3].avgRmsDb < expected + 1.f);  // can't be louder than the signal
}

TEST_CASE("equal L/R sine has correlation ≈ 1") {
    const auto audio = makeStereoSine(440.f, 0.5f);
    const auto snap  = mt::analyseFile(audio);
    // Overall and per-band correlation should be very close to 1.0
    CHECK(snap.overallCorr > 0.99f);
    CHECK(snap.bands[1].correlation > 0.99f);  // 440 Hz in Lows band
}

TEST_CASE("silence gives -100 dBFS overall") {
    mt::AudioFile silence;
    silence.sampleRate  = 44100;
    silence.numChannels = 2;
    silence.numFrames   = 44100;
    silence.samples.resize(2, std::vector<float>(44100, 0.f));

    const auto snap = mt::analyseFile(silence);
    CHECK(snap.overallAvgDb  <= -99.f);
    CHECK(snap.overallPeakDb <= -99.f);
}

TEST_CASE("mono input is treated as dual-mono (correlation = 1)") {
    mt::AudioFile mono;
    mono.sampleRate  = 44100;
    mono.numChannels = 1;
    mono.numFrames   = 44100;
    mono.samples.resize(1, std::vector<float>(44100));
    for (int i = 0; i < 44100; ++i)
        mono.samples[0][static_cast<size_t>(i)] =
            0.3f * std::sin(2.f * std::numbers::pi_v<float> * 1000.f
                            * static_cast<float>(i) / 44100.f);

    const auto snap = mt::analyseFile(mono);
    CHECK(snap.overallCorr > 0.99f);
    CHECK(snap.overallAvgDb > -30.f);
}

TEST_CASE("percentile descriptors: P10 in quiet zone, P95 in loud zone, P10 < P50 < P95") {
    // First half of signal is loud (0.3 amp ≈ -13.5 dBFS sine RMS).
    // Second half is quiet (0.03 amp ≈ -33.5 dBFS sine RMS).
    // 1 kHz lands in the Mids band (index 3, crossovers 500 Hz – 2 kHz).
    const int   sr        = 44100;
    const float durSec    = 4.f;
    const int   numFrames = static_cast<int>(static_cast<float>(sr) * durSec);
    const int   halfFrames = numFrames / 2;

    mt::AudioFile f;
    f.sampleRate  = sr;
    f.numChannels = 2;
    f.numFrames   = numFrames;
    f.bitDepth    = 24;
    f.samples.resize(2, std::vector<float>(static_cast<size_t>(numFrames)));
    for (int i = 0; i < numFrames; ++i) {
        const float amp = (i < halfFrames) ? 0.3f : 0.03f;
        const float s = amp * std::sin(2.f * std::numbers::pi_v<float> * 1000.f
                                        * static_cast<float>(i) / static_cast<float>(sr));
        f.samples[0][static_cast<size_t>(i)] = s;
        f.samples[1][static_cast<size_t>(i)] = s;
    }

    const auto snap = mt::analyseFile(f);
    const auto& b   = snap.bands[3];  // Mids band

    // Ordering invariants
    CHECK(b.p10RmsDb <= b.p50RmsDb);
    CHECK(b.p50RmsDb <= b.p95RmsDb);
    // Loud section (amp 0.3, ~-13.5 dBFS) dominates P95; allow ±5 dB for filter leakage
    CHECK(b.p95RmsDb > -20.f);
    // Quiet section (amp 0.03, ~-33.5 dBFS) is captured by P10
    CHECK(b.p10RmsDb < -25.f);
    // At least 10 dB separates the two extremes
    CHECK(b.p10RmsDb < b.p95RmsDb - 10.f);
}
