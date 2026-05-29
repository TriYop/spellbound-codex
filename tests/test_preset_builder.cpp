#include "preset_builder/services/stats_service.hpp"

#include <doctest.h>

#include <array>

using pb::StatsService;
using pb::Track;
using pb::PresetStats;

static Track makeTrack(std::array<float, 7> bandRms,
                       std::array<float, 7> bandCorr,
                       std::array<float, 7> bandTransient,
                       float overallRms, float overallCorr)
{
    Track t;
    t.analysis.bandRmsDb       = bandRms;
    t.analysis.bandCorr        = bandCorr;
    t.analysis.bandTransientDb = bandTransient;
    t.analysis.overallRmsDb    = overallRms;
    t.analysis.overallCorr     = overallCorr;
    return t;
}

TEST_CASE("StatsService::compute returns zero stats for empty input") {
    StatsService svc;
    auto stats = svc.compute({});
    CHECK(stats.bandRmsDb[0]   == doctest::Approx(0.f));
    CHECK(stats.overallRmsDb   == doctest::Approx(0.f));
    CHECK(stats.overallCorrMin == doctest::Approx(0.f));
}

TEST_CASE("StatsService::compute single track: stats equal track values") {
    StatsService svc;
    Track t = makeTrack(
        {-20, -18, -16, -14, -12, -10, -8},
        {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f},
        {6, 5, 4, 3, 2, 1, 0},
        -18.f, 0.85f);

    auto stats = svc.compute({t});

    for (int i = 0; i < 7; ++i) {
        CHECK(stats.bandRmsDb[static_cast<size_t>(i)]
              == doctest::Approx(t.analysis.bandRmsDb[static_cast<size_t>(i)]));
        CHECK(stats.bandCorrMin[static_cast<size_t>(i)]
              == doctest::Approx(t.analysis.bandCorr[static_cast<size_t>(i)]));
        CHECK(stats.bandTransientDb[static_cast<size_t>(i)]
              == doctest::Approx(t.analysis.bandTransientDb[static_cast<size_t>(i)]));
    }
    CHECK(stats.overallRmsDb   == doctest::Approx(-18.f));
    CHECK(stats.overallCorrMin == doctest::Approx(0.85f));
}

TEST_CASE("StatsService::compute three tracks: mean, p10, median") {
    StatsService svc;

    // bandRmsDb band0: mean of -20, -18, -16 = -18
    // bandCorr  band0: p10 of 0.3, 0.5, 0.8 → sorted[floor(0.1*3)=0] = 0.3
    // bandTrans band0: median of 5, 3, 7 → sorted[3,5,7] middle = 5
    // overallRmsDb: mean of -10, -20, -15 = -15
    // overallCorr p10: sorted [0.4, 0.7, 0.9], idx=0 → 0.4
    Track t0 = makeTrack({-20,0,0,0,0,0,0}, {0.3f,0,0,0,0,0,0}, {5,0,0,0,0,0,0}, -10.f, 0.9f);
    Track t1 = makeTrack({-18,0,0,0,0,0,0}, {0.5f,0,0,0,0,0,0}, {3,0,0,0,0,0,0}, -20.f, 0.4f);
    Track t2 = makeTrack({-16,0,0,0,0,0,0}, {0.8f,0,0,0,0,0,0}, {7,0,0,0,0,0,0}, -15.f, 0.7f);

    auto stats = svc.compute({t0, t1, t2});

    CHECK(stats.bandRmsDb[0]       == doctest::Approx(-18.f));
    CHECK(stats.bandCorrMin[0]     == doctest::Approx(0.3f));
    CHECK(stats.bandTransientDb[0] == doctest::Approx(5.f));
    CHECK(stats.overallRmsDb       == doctest::Approx(-15.f));
    CHECK(stats.overallCorrMin     == doctest::Approx(0.4f));
}

TEST_CASE("StatsService::compute four tracks: even-n median averages two middles") {
    StatsService svc;
    std::array<float, 7> z{};
    // bandTransientDb band0: sorted [3,5,7,9] → median = (5+7)/2 = 6
    Track t0 = makeTrack(z, z, {3,0,0,0,0,0,0}, 0.f, 0.f);
    Track t1 = makeTrack(z, z, {9,0,0,0,0,0,0}, 0.f, 0.f);
    Track t2 = makeTrack(z, z, {5,0,0,0,0,0,0}, 0.f, 0.f);
    Track t3 = makeTrack(z, z, {7,0,0,0,0,0,0}, 0.f, 0.f);

    auto stats = svc.compute({t0, t1, t2, t3});

    CHECK(stats.bandTransientDb[0] == doctest::Approx(6.f));
}

#include "preset_builder/services/export_service.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs_pb = std::filesystem;

static std::string readFileStr(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

TEST_CASE("ExportService::exportXml writes MixAdvice-compatible XML") {
    pb::Preset preset;
    preset.name        = "Test Preset";
    preset.description = "Unit test preset";

    pb::PresetStats stats;
    stats.bandRmsDb       = {-20.f, -18.f, -16.f, -14.f, -12.f, -10.f, -8.f};
    stats.bandCorrMin     = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f};
    stats.bandTransientDb = {6.f, 5.f, 4.f, 3.f, 2.f, 1.f, 0.f};
    stats.overallRmsDb    = -18.f;
    stats.overallCorrMin  =  0.7f;

    const auto tmp = (fs_pb::temp_directory_path() / "pb_test_export.xml").string();

    pb::ExportService svc;
    svc.exportXml(preset, stats, tmp);

    const std::string xml = readFileStr(tmp);
    fs_pb::remove(tmp);

    CHECK(xml.find("name=\"Test Preset\"")             != std::string::npos);
    CHECK(xml.find("description=\"Unit test preset\"") != std::string::npos);
    CHECK(xml.find("<bandRmsDb>")                      != std::string::npos);
    CHECK(xml.find("<bandMinCorr>")                    != std::string::npos);
    CHECK(xml.find("<bandTransientDb>")                != std::string::npos);
    CHECK(xml.find("<overallRmsDb>")                   != std::string::npos);
    CHECK(xml.find("<overallMinCorr>")                 != std::string::npos);
    CHECK(xml.find("-20.")                             != std::string::npos);
}

TEST_CASE("ExportService::exportXml: XML special chars in name are escaped") {
    pb::Preset preset;
    preset.name        = "Rock & Roll";
    preset.description = "Test \"quotes\"";

    pb::PresetStats stats;  // zero-filled

    const auto tmp = (fs_pb::temp_directory_path() / "pb_test_escape.xml").string();
    pb::ExportService svc;
    svc.exportXml(preset, stats, tmp);

    const std::string xml = readFileStr(tmp);
    fs_pb::remove(tmp);

    CHECK(xml.find("&amp;")  != std::string::npos);
    CHECK(xml.find("&quot;") != std::string::npos);
}

#include "preset_builder/services/ingest_service.hpp"

TEST_CASE("IngestService::parseFilenameMetadata: 'Artist - Title.wav'") {
    auto m = pb::IngestService::parseFilenameMetadata("The Artist - My Song.wav");
    REQUIRE(m.artist.has_value());
    REQUIRE(m.title.has_value());
    CHECK(*m.artist == "The Artist");
    CHECK(*m.title  == "My Song");
    CHECK(m.source  == pb::MetadataSource::filename);
}

TEST_CASE("IngestService::parseFilenameMetadata: em-dash separator") {
    // UTF-8 em-dash: 0xE2 0x80 0x93
    auto m = pb::IngestService::parseFilenameMetadata("DJ Name \xe2\x80\x93 Track Name.flac");
    REQUIRE(m.artist.has_value());
    REQUIRE(m.title.has_value());
    CHECK(*m.artist == "DJ Name");
    CHECK(*m.title  == "Track Name");
}

TEST_CASE("IngestService::parseFilenameMetadata: no separator gives title only") {
    auto m = pb::IngestService::parseFilenameMetadata("MySong.aiff");
    CHECK(!m.artist.has_value());
    REQUIRE(m.title.has_value());
    CHECK(*m.title == "MySong");
    CHECK(m.source == pb::MetadataSource::filename);
}

TEST_CASE("IngestService::parseFilenameMetadata: full path is handled") {
    auto m = pb::IngestService::parseFilenameMetadata("/home/user/music/Artist - Song.wav");
    REQUIRE(m.artist.has_value());
    CHECK(*m.artist == "Artist");
    REQUIRE(m.title.has_value());
    CHECK(*m.title == "Song");
}

#include "mastertweak/io.hpp"

#include <cmath>
#include <filesystem>
#include <map>

namespace fs_ingest = std::filesystem;

// ── Stubs ─────────────────────────────────────────────────────────────────────

struct StubTrackRepo : pb::TrackRepository {
    std::map<std::string, pb::Track> store;

    std::optional<pb::Track> find(const pb::TrackId& id) const override {
        auto it = store.find(id.hash);
        return it != store.end() ? std::optional{it->second} : std::nullopt;
    }
    std::optional<pb::Track> findByPath(const std::string&) const override {
        return std::nullopt;
    }
    std::vector<pb::Track> search(const pb::TrackFilter&) const override {
        std::vector<pb::Track> v;
        for (const auto& [k, t] : store) v.push_back(t);
        return v;
    }
    void save(const pb::Track& t) override { store[t.id.hash] = t; }
    void remove(const pb::TrackId& id) override { store.erase(id.hash); }
};

struct AlwaysFailMetadataProvider : pb::MetadataProvider {
    std::optional<pb::TrackMetadata> lookup(const std::string&) override {
        return std::nullopt;
    }
};

struct AlwaysSucceedMetadataProvider : pb::MetadataProvider {
    std::optional<pb::TrackMetadata> lookup(const std::string&) override {
        pb::TrackMetadata m;
        m.title  = "AcoustID Title";
        m.artist = "AcoustID Artist";
        m.source = pb::MetadataSource::acoustid;
        return m;
    }
};

static std::string writeSineWav(const std::string& path,
                                float freqHz = 440.f,
                                int sr = 44100,
                                float durationSec = 0.5f)
{
    mt::AudioFile f;
    f.sampleRate  = sr;
    f.numChannels = 2;
    f.numFrames   = static_cast<int>(durationSec * static_cast<float>(sr));
    f.bitDepth    = 24;
    f.samples.resize(2, std::vector<float>(static_cast<size_t>(f.numFrames)));
    for (int i = 0; i < f.numFrames; ++i) {
        const float s = 0.5f * std::sin(
            2.f * 3.14159265f * freqHz * static_cast<float>(i) / static_cast<float>(sr));
        f.samples[0][static_cast<size_t>(i)] = s;
        f.samples[1][static_cast<size_t>(i)] = s;
    }
    const std::string ext = fs_ingest::path(path).extension().string();
    const bool isFlac = (ext == ".flac");
    mt::writeAudioFile(path, f, {24, isFlac});
    return path;
}

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("IngestService::ingest single file: added=1, track stored") {
    const auto wav = writeSineWav(
        (fs_ingest::temp_directory_path() / "pb_ingest_test.wav").string());

    StubTrackRepo repo;
    AlwaysSucceedMetadataProvider meta;
    pb::IngestService svc;

    auto report = svc.ingest(wav, repo, meta);

    CHECK(report.added   == 1);
    CHECK(report.skipped == 0);
    CHECK(report.failed  == 0);
    CHECK(repo.store.size() == 1);

    const auto& stored = repo.store.begin()->second;
    CHECK(stored.metadata.source == pb::MetadataSource::acoustid);
    REQUIRE(stored.metadata.title.has_value());
    CHECK(*stored.metadata.title == "AcoustID Title");

    fs_ingest::remove(wav);
}

TEST_CASE("IngestService::ingest same file twice: second call skipped") {
    const auto wav = writeSineWav(
        (fs_ingest::temp_directory_path() / "pb_ingest_dup.wav").string());

    StubTrackRepo repo;
    AlwaysSucceedMetadataProvider meta;
    pb::IngestService svc;

    svc.ingest(wav, repo, meta);
    auto report2 = svc.ingest(wav, repo, meta);

    CHECK(report2.added   == 0);
    CHECK(report2.skipped == 1);
    CHECK(repo.store.size() == 1);

    fs_ingest::remove(wav);
}

TEST_CASE("IngestService::ingest: metadata fallback on provider failure") {
    const auto wavPath = (fs_ingest::temp_directory_path() /
                          "Test Artist - My Track.wav").string();
    writeSineWav(wavPath);

    StubTrackRepo repo;
    AlwaysFailMetadataProvider meta;
    pb::IngestService svc;

    auto report = svc.ingest(wavPath, repo, meta);

    CHECK(report.added == 1);
    REQUIRE(repo.store.size() == 1);
    const auto& stored = repo.store.begin()->second;
    CHECK(stored.metadata.source == pb::MetadataSource::filename);
    REQUIRE(stored.metadata.artist.has_value());
    CHECK(*stored.metadata.artist == "Test Artist");
    REQUIRE(stored.metadata.title.has_value());
    CHECK(*stored.metadata.title  == "My Track");

    fs_ingest::remove(wavPath);
}

TEST_CASE("IngestService::ingest directory: scans recursively") {
    const auto dir = fs_ingest::temp_directory_path() / "pb_ingest_dir";
    fs_ingest::create_directories(dir / "sub");

    writeSineWav((dir / "track1.wav").string());
    writeSineWav((dir / "sub" / "track2.flac").string());
    // Write a non-audio file that should be ignored
    { std::ofstream f((dir / "README.txt").string()); f << "ignore me"; }

    StubTrackRepo repo;
    AlwaysSucceedMetadataProvider meta;
    pb::IngestService svc;

    auto report = svc.ingest(dir.string(), repo, meta);

    CHECK(report.added  == 2);
    CHECK(report.failed == 0);
    CHECK(repo.store.size() == 2);

    fs_ingest::remove_all(dir);
}
