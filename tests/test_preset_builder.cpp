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
