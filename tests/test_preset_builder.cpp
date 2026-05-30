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
#include "mastertweak/preset.hpp"

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
    CHECK(xml.find("<MixAdvicePreset")                 != std::string::npos);
    CHECK(xml.find("<BandRmsDb")                       != std::string::npos);
    CHECK(xml.find("<BandMinCorr")                     != std::string::npos);
    CHECK(xml.find("<BandTransientDb")                 != std::string::npos);
    CHECK(xml.find("<Overall")                         != std::string::npos);
    CHECK(xml.find("rmsDb=")                           != std::string::npos);
    CHECK(xml.find("minCorr=")                         != std::string::npos);
    CHECK(xml.find("sub=\"-20.")                       != std::string::npos);
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

TEST_CASE("ExportService::exportXml round-trips through mt::loadPreset") {
    pb::Preset preset;
    preset.name        = "RoundTripTest";
    preset.description = "verifies schema compatibility";

    pb::PresetStats stats;
    stats.bandRmsDb       = {-20.f, -18.f, -16.f, -14.f, -12.f, -10.f, -8.f};
    stats.bandCorrMin     = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f};
    stats.bandTransientDb = {6.f, 5.f, 4.f, 3.f, 2.f, 1.f, 0.f};
    stats.overallRmsDb    = -18.f;
    stats.overallCorrMin  =  0.7f;

    const auto tmp = (fs_pb::temp_directory_path() / "pb_roundtrip.xml").string();
    pb::ExportService svc;
    svc.exportXml(preset, stats, tmp);

    std::string err;
    const auto loaded = mt::loadPreset(tmp, &err);
    fs_pb::remove(tmp);

    REQUIRE_MESSAGE(loaded.has_value(), err);
    CHECK(loaded->name        == "RoundTripTest");
    CHECK(loaded->overallRmsDb == doctest::Approx(-18.f));
    CHECK(loaded->bandRmsDb[0] == doctest::Approx(-20.f));
    CHECK(loaded->bandMinCorr[0] == doctest::Approx(0.9f));
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

TEST_CASE("IngestService::ingest: parallel batch — all files ingested correctly") {
    const auto dir = fs_ingest::temp_directory_path() / "pb_ingest_parallel";
    fs_ingest::create_directories(dir);
    constexpr int N = 8;
    for (int k = 0; k < N; ++k)
        writeSineWav((dir / ("track_" + std::to_string(k) + ".wav")).string(),
                     200.f + static_cast<float>(k) * 100.f);

    StubTrackRepo repo;
    AlwaysSucceedMetadataProvider meta;
    pb::IngestService svc;

    auto report = svc.ingest(dir.string(), repo, meta);

    CHECK(report.added   == N);
    CHECK(report.skipped == 0);
    CHECK(report.failed  == 0);
    CHECK(static_cast<int>(repo.store.size()) == N);

    fs_ingest::remove_all(dir);
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

#include "preset_builder/adapters/database.hpp"
#include "preset_builder/adapters/sqlite_track_repository.hpp"

static pb::Track makeSampleTrack(const std::string& hash = "abc123",
                                 const std::string& path = "/tmp/song.wav") {
    pb::Track t;
    t.id      = pb::TrackId{hash};
    t.path    = path;
    t.addedAt = "2026-05-29T12:00:00Z";
    t.metadata.artist = "Test Artist";
    t.metadata.title  = "Test Song";
    t.metadata.genre  = "Rock";
    t.metadata.source = pb::MetadataSource::filename;
    t.analysis.bandRmsDb[0]       = -20.f;
    t.analysis.bandCorr[0]        =  0.9f;
    t.analysis.bandTransientDb[0] =  6.f;
    t.analysis.overallRmsDb       = -18.f;
    t.analysis.overallCorr        =  0.85f;
    return t;
}

TEST_CASE("SqliteTrackRepository: save and find by id") {
    pb::Database db(":memory:");
    pb::SqliteTrackRepository repo(db);

    const auto track = makeSampleTrack("hash001");
    repo.save(track);

    const auto found = repo.find(pb::TrackId{"hash001"});
    REQUIRE(found.has_value());
    CHECK(found->id.hash               == "hash001");
    CHECK(found->path                  == "/tmp/song.wav");
    CHECK(found->metadata.artist       == track.metadata.artist);
    CHECK(found->metadata.title        == track.metadata.title);
    CHECK(found->metadata.genre        == track.metadata.genre);
    CHECK(found->analysis.bandRmsDb[0]  == doctest::Approx(-20.f));
    CHECK(found->analysis.bandCorr[0]   == doctest::Approx(0.9f));
    CHECK(found->analysis.overallCorr   == doctest::Approx(0.85f));
}

TEST_CASE("SqliteTrackRepository: find returns nullopt for unknown id") {
    pb::Database db(":memory:");
    pb::SqliteTrackRepository repo(db);
    CHECK(!repo.find(pb::TrackId{"nope"}).has_value());
}

TEST_CASE("SqliteTrackRepository: findByPath") {
    pb::Database db(":memory:");
    pb::SqliteTrackRepository repo(db);

    repo.save(makeSampleTrack("h1", "/music/a.wav"));
    const auto found = repo.findByPath("/music/a.wav");
    REQUIRE(found.has_value());
    CHECK(found->id.hash == "h1");
}

TEST_CASE("SqliteTrackRepository: search by artist") {
    pb::Database db(":memory:");
    pb::SqliteTrackRepository repo(db);

    auto t1 = makeSampleTrack("h1", "/a.wav"); t1.metadata.artist = "Beatles";
    auto t2 = makeSampleTrack("h2", "/b.wav"); t2.metadata.artist = "Rolling Stones";
    repo.save(t1);
    repo.save(t2);

    pb::TrackFilter filter;
    filter.artist = "beatles";  // case-insensitive
    const auto results = repo.search(filter);
    REQUIRE(results.size() == 1);
    CHECK(results[0].id.hash == "h1");
}

TEST_CASE("SqliteTrackRepository: search by artist AND genre") {
    pb::Database db(":memory:");
    pb::SqliteTrackRepository repo(db);

    auto t1 = makeSampleTrack("h1"); t1.metadata.artist = "Metallica"; t1.metadata.genre = "Metal";
    auto t2 = makeSampleTrack("h2", "/b.wav"); t2.metadata.artist = "Metallica"; t2.metadata.genre = "Rock";
    repo.save(t1);
    repo.save(t2);

    pb::TrackFilter filter;
    filter.artist = "metallica";
    filter.genre  = "metal";
    const auto results = repo.search(filter);
    REQUIRE(results.size() == 1);
    CHECK(results[0].id.hash == "h1");
}

TEST_CASE("SqliteTrackRepository: empty filter returns all tracks") {
    pb::Database db(":memory:");
    pb::SqliteTrackRepository repo(db);
    repo.save(makeSampleTrack("h1", "/a.wav"));
    repo.save(makeSampleTrack("h2", "/b.wav"));
    CHECK(repo.search({}).size() == 2);
}

TEST_CASE("SqliteTrackRepository: remove") {
    pb::Database db(":memory:");
    pb::SqliteTrackRepository repo(db);
    repo.save(makeSampleTrack("h1"));
    repo.remove(pb::TrackId{"h1"});
    CHECK(!repo.find(pb::TrackId{"h1"}).has_value());
}

#include "preset_builder/adapters/sqlite_preset_repository.hpp"

static pb::Preset makePreset(const std::string& uuid = "uuid-1",
                              const std::string& name = "My Preset") {
    pb::Preset p;
    p.id          = pb::PresetId{uuid};
    p.name        = name;
    p.description = "Test description";
    p.createdAt   = "2026-05-29T12:00:00Z";
    return p;
}

TEST_CASE("SqlitePresetRepository: save and find by id") {
    pb::Database db(":memory:");
    pb::SqlitePresetRepository repo(db);

    auto preset = makePreset("uuid-42", "Rock Preset");
    repo.save(preset);

    const auto found = repo.find(pb::PresetId{"uuid-42"});
    REQUIRE(found.has_value());
    CHECK(found->name        == "Rock Preset");
    CHECK(found->description == "Test description");
}

TEST_CASE("SqlitePresetRepository: find returns nullopt for unknown id") {
    pb::Database db(":memory:");
    pb::SqlitePresetRepository repo(db);
    CHECK(!repo.find(pb::PresetId{"missing"}).has_value());
}

TEST_CASE("SqlitePresetRepository: listAll") {
    pb::Database db(":memory:");
    pb::SqlitePresetRepository repo(db);
    repo.save(makePreset("u1", "P1"));
    repo.save(makePreset("u2", "P2"));
    CHECK(repo.listAll().size() == 2);
}

TEST_CASE("SqlitePresetRepository: save stores trackIds and tracksFor returns them") {
    pb::Database db(":memory:");
    pb::SqliteTrackRepository  trackRepo(db);
    pb::SqlitePresetRepository presetRepo(db);

    auto t1 = makeSampleTrack("th1", "/a.wav");
    auto t2 = makeSampleTrack("th2", "/b.wav");
    trackRepo.save(t1);
    trackRepo.save(t2);

    pb::Preset preset = makePreset("p1");
    preset.trackIds   = {pb::TrackId{"th1"}, pb::TrackId{"th2"}};
    presetRepo.save(preset);

    const auto tracks = presetRepo.tracksFor(pb::PresetId{"p1"});
    REQUIRE(tracks.size() == 2);

    const auto loaded = presetRepo.find(pb::PresetId{"p1"});
    REQUIRE(loaded.has_value());
    CHECK(loaded->trackIds.size() == 2);
}

TEST_CASE("SqlitePresetRepository: remove deletes preset and preset_tracks") {
    pb::Database db(":memory:");
    pb::SqliteTrackRepository  trackRepo(db);
    pb::SqlitePresetRepository presetRepo(db);

    trackRepo.save(makeSampleTrack("th1"));
    pb::Preset p = makePreset("p1");
    p.trackIds   = {pb::TrackId{"th1"}};
    presetRepo.save(p);

    presetRepo.remove(pb::PresetId{"p1"});

    CHECK(!presetRepo.find(pb::PresetId{"p1"}).has_value());
    CHECK(presetRepo.listAll().empty());
}
