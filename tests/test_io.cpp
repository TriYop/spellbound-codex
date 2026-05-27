#include "mastertweak/io.hpp"

#include <doctest.h>

#include <cmath>
#include <filesystem>
#include <numbers>

namespace fs = std::filesystem;

// Generate a simple stereo sine wave AudioFile in memory
static mt::AudioFile makeSine(float freqHz, float ampLinear, int sr, float durationSec) {
    mt::AudioFile f;
    f.sampleRate  = sr;
    f.numChannels = 2;
    f.numFrames   = static_cast<int>(durationSec * static_cast<float>(sr));
    f.bitDepth    = 24;
    f.samples.resize(2, std::vector<float>(static_cast<size_t>(f.numFrames)));
    for (int i = 0; i < f.numFrames; ++i) {
        const float s = ampLinear * std::sin(2.f * std::numbers::pi_v<float> * freqHz
                                              * static_cast<float>(i) / static_cast<float>(sr));
        f.samples[0][static_cast<size_t>(i)] = s;
        f.samples[1][static_cast<size_t>(i)] = s;
    }
    return f;
}

TEST_CASE("write + read round-trip: WAV 24-bit") {
    const auto tmp = (fs::temp_directory_path() / "mt_test_rtrip.wav").string();
    const auto original = makeSine(1000.f, 0.5f, 44100, 0.5f);

    std::string err;
    const bool ok = mt::writeAudioFile(tmp, original, {24, false}, &err);
    REQUIRE_MESSAGE(ok, err);

    const auto loaded = mt::readAudioFile(tmp, &err);
    REQUIRE_MESSAGE(loaded.has_value(), err);

    CHECK(loaded->sampleRate  == original.sampleRate);
    CHECK(loaded->numChannels == original.numChannels);
    CHECK(loaded->numFrames   == original.numFrames);

    // 24-bit quantisation introduces up to ~1 LSB error (~120 dB down); allow ±1e-4
    for (int i = 0; i < original.numFrames; i += 100) {
        CHECK(std::abs(loaded->samples[0][static_cast<size_t>(i)] -
                       original.samples[0][static_cast<size_t>(i)]) < 1e-4f);
    }
    fs::remove(tmp);
}

TEST_CASE("write + read round-trip: FLAC 24-bit") {
    const auto tmp = (fs::temp_directory_path() / "mt_test_rtrip.flac").string();
    const auto original = makeSine(440.f, 0.3f, 48000, 0.3f);

    std::string err;
    REQUIRE_MESSAGE(mt::writeAudioFile(tmp, original, {24, true}, &err), err);
    const auto loaded = mt::readAudioFile(tmp, &err);
    REQUIRE_MESSAGE(loaded.has_value(), err);
    CHECK(loaded->numChannels == 2);
    CHECK(loaded->sampleRate  == 48000);
    fs::remove(tmp);
}

TEST_CASE("read non-existent file returns nullopt") {
    std::string err;
    const auto r = mt::readAudioFile("/tmp/does_not_exist_mt.wav", &err);
    CHECK_FALSE(r.has_value());
    CHECK_FALSE(err.empty());
}
