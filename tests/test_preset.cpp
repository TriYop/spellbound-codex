#include "mastertweak/preset.hpp"

#include <doctest.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static const std::string kSampleXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<MixAdvicePreset name="Test Preset"
                 description="A synthetic preset for unit testing">
    <BandRmsDb       sub="-30" lows="-20" lomid="-16" mids="-16" himid="-18" highs="-22" air="-30"/>
    <BandMinCorr     sub="0.75" lows="0.65" lomid="0.52" mids="0.48" himid="0.42" highs="0.32" air="0.25"/>
    <BandTransientDb sub="6"   lows="14"  lomid="14"  mids="14"  himid="18"  highs="16"  air="12"/>
    <Overall         rmsDb="-12" minCorr="0.48"/>
</MixAdvicePreset>
)";

// Write sample XML to a temp file, return path
static std::string writeTmpXml(const std::string& xml, const std::string& stem = "mt_test_preset") {
    const auto path = (fs::temp_directory_path() / (stem + ".xml")).string();
    std::ofstream f(path);
    f << xml;
    return path;
}

TEST_CASE("loadPreset: well-formed XML parses correctly") {
    const std::string path = writeTmpXml(kSampleXml);

    std::string err;
    const auto p = mt::loadPreset(path, &err);
    REQUIRE_MESSAGE(p.has_value(), err);

    CHECK(p->name        == "Test Preset");
    CHECK(p->description == "A synthetic preset for unit testing");

    // Band array values
    CHECK(p->bandRmsDb[0]      == doctest::Approx(-30.f));
    CHECK(p->bandRmsDb[3]      == doctest::Approx(-16.f));   // Mids
    CHECK(p->bandMinCorr[0]    == doctest::Approx(0.75f));
    CHECK(p->bandTransientDb[4] == doctest::Approx(18.f));   // Hi-Mid
    CHECK(p->overallRmsDb      == doctest::Approx(-12.f));
    CHECK(p->overallMinCorr    == doctest::Approx(0.48f));

    fs::remove(path);
}

TEST_CASE("loadPreset: missing file returns nullopt") {
    std::string err;
    const auto p = mt::loadPreset("/tmp/no_such_preset_mt.xml", &err);
    CHECK_FALSE(p.has_value());
    CHECK_FALSE(err.empty());
}

TEST_CASE("loadPreset: wrong root element returns nullopt") {
    const std::string bad = R"(<NotAPreset name="x"/>)";
    const std::string path = writeTmpXml(bad, "mt_bad_root");
    std::string err;
    const auto p = mt::loadPreset(path, &err);
    CHECK_FALSE(p.has_value());
    fs::remove(path);
}

TEST_CASE("loadPresetsFromDir: loads multiple XMLs from a directory") {
    const auto dir = fs::temp_directory_path() / "mt_preset_dir_test";
    fs::create_directories(dir);

    // Write two preset files
    const std::string xml2 = R"(<?xml version="1.0"?>
<MixAdvicePreset name="Preset B" description="b">
    <BandRmsDb       sub="-25" lows="-20" lomid="-18" mids="-18" himid="-20" highs="-24" air="-28"/>
    <BandMinCorr     sub="0.9"  lows="0.8"  lomid="0.7"  mids="0.6"  himid="0.5"  highs="0.4"  air="0.3"/>
    <BandTransientDb sub="4"    lows="8"    lomid="10"   mids="12"   himid="14"   highs="12"   air="8"/>
    <Overall         rmsDb="-14" minCorr="0.55"/>
</MixAdvicePreset>)";

    {
        std::ofstream(dir / "preset_a.xml") << kSampleXml;
        std::ofstream(dir / "preset_b.xml") << xml2;
        std::ofstream(dir / "not_a_preset.txt") << "ignore me";
    }

    const auto presets = mt::loadPresetsFromDir(dir.string());
    REQUIRE(presets.size() == 2);

    std::vector<std::string> names;
    for (auto& p : presets) names.push_back(p.name);
    CHECK((names[0] == "Test Preset" || names[1] == "Test Preset"));
    CHECK((names[0] == "Preset B"    || names[1] == "Preset B"));

    fs::remove_all(dir);
}

TEST_CASE("loadPreset: loads a real MixAdvice preset if available") {
    const std::string path =
        "/home/yvan/Projects/AudioPlugins/TrueSight/Presets/fest-noz.xml";
    if (!fs::exists(path)) return;  // skip if TrueSight is not checked out alongside this repo

    std::string err;
    const auto p = mt::loadPreset(path, &err);
    REQUIRE_MESSAGE(p.has_value(), err);
    CHECK(p->name == "Fest-Noz");
    CHECK(p->overallRmsDb == doctest::Approx(-17.f));
}
