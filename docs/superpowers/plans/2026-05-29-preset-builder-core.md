# Preset Builder Core — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `preset_builder_core` — a Qt-free static C++ library with domain model, ports, services, and SQLite adapters for ingesting audio tracks, computing preset statistics, and exporting MixAdvice-compatible XML.

**Architecture:** New `preset_builder/` subdirectory at repo root. Domain model (`pb::Track`, `pb::Preset`) + abstract ports (`TrackRepository`, `PresetRepository`, `MetadataProvider`) + three stateless services (`StatsService`, `ExportService`, `IngestService`) + SQLite adapters (`Database` RAII + two repository implementations). `preset_builder_core` links `mastertweak_core` (for `SevenBandAnalyser` + file I/O) and system SQLite3. No Qt dependency.

**Tech Stack:** C++20, CMake/Ninja, system SQLite3 (`libsqlite3-dev`), picosha2.h (vendored SHA-256), doctest (existing), libsndfile (transitive via mastertweak_core).

> **Spec note — `PresetStats` gap fixed here:** The design spec omits `overallCorrMin` from `PresetStats`. This plan adds it (p10 of `track.overallCorr` across tracks) so the exported XML can include `<overallMinCorr>` as required by the MixAdvice schema.

---

## File Map

| Action | Path | Purpose |
|--------|------|---------|
| Create | `preset_builder/CMakeLists.txt` | Static lib `preset_builder_core` |
| Create | `preset_builder/include/preset_builder/domain/track.hpp` | `Track`, `TrackId`, `TrackMetadata`, `TrackAnalysis`, `TrackFilter` |
| Create | `preset_builder/include/preset_builder/domain/preset.hpp` | `Preset`, `PresetId`, `PresetStats` |
| Create | `preset_builder/include/preset_builder/ports/track_repository.hpp` | `TrackRepository` abstract interface |
| Create | `preset_builder/include/preset_builder/ports/preset_repository.hpp` | `PresetRepository` abstract interface |
| Create | `preset_builder/include/preset_builder/ports/metadata_provider.hpp` | `MetadataProvider` abstract interface |
| Create | `preset_builder/include/preset_builder/services/stats_service.hpp` | `StatsService` |
| Create | `preset_builder/include/preset_builder/services/export_service.hpp` | `ExportService` |
| Create | `preset_builder/include/preset_builder/services/ingest_service.hpp` | `IngestService` |
| Create | `preset_builder/include/preset_builder/adapters/database.hpp` | `Database` RAII (sqlite3 connection + schema init) |
| Create | `preset_builder/include/preset_builder/adapters/sqlite_track_repository.hpp` | `SqliteTrackRepository` |
| Create | `preset_builder/include/preset_builder/adapters/sqlite_preset_repository.hpp` | `SqlitePresetRepository` |
| Create | `preset_builder/src/services/stats_service.cpp` | Implementation |
| Create | `preset_builder/src/services/export_service.cpp` | Implementation |
| Create | `preset_builder/src/services/ingest_service.cpp` | Implementation |
| Create | `preset_builder/src/adapters/database.cpp` | DB open + schema creation |
| Create | `preset_builder/src/adapters/sqlite_track_repository.cpp` | Implementation |
| Create | `preset_builder/src/adapters/sqlite_preset_repository.cpp` | Implementation |
| Create | `third_party/picosha2.h` | SHA-256 (vendored, MIT) |
| Create | `tests/test_preset_builder.cpp` | doctest unit + integration tests |
| Modify | `CMakeLists.txt` | Add `add_subdirectory(preset_builder)` |
| Modify | `tests/CMakeLists.txt` | Add `test_preset_builder.cpp`, link `preset_builder_core` |

---

### Task 1: Scaffold — prerequisites, directories, CMakeLists, empty test file

**Files:**
- Create: `preset_builder/CMakeLists.txt`
- Create: `third_party/picosha2.h`
- Create: `tests/test_preset_builder.cpp` (empty shell)
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Install system SQLite3**

```bash
sudo apt install libsqlite3-dev
```

Expected: installs without error.

- [ ] **Step 2: Download picosha2.h into `third_party/`**

```bash
curl -L "https://raw.githubusercontent.com/okdshin/PicoSHA2/master/picosha2.h" \
     -o /home/yvan/Projects/AudioPlugins/MasterTweak/third_party/picosha2.h
```

Expected: file created, ~20 KB.

- [ ] **Step 3: Create the directory skeleton**

```bash
mkdir -p /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/include/preset_builder/domain
mkdir -p /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/include/preset_builder/ports
mkdir -p /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/include/preset_builder/services
mkdir -p /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/include/preset_builder/adapters
mkdir -p /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/src/services
mkdir -p /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/src/adapters
```

- [ ] **Step 4: Write `preset_builder/CMakeLists.txt`**

```cmake
find_package(SQLite3 REQUIRED)

add_library(preset_builder_core STATIC
    src/services/stats_service.cpp
    src/services/export_service.cpp
    src/services/ingest_service.cpp
    src/adapters/database.cpp
    src/adapters/sqlite_track_repository.cpp
    src/adapters/sqlite_preset_repository.cpp
)

target_include_directories(preset_builder_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(preset_builder_core PUBLIC
    mastertweak_core
    SQLite::SQLite3
)
```

- [ ] **Step 5: Add `preset_builder` subdirectory to root `CMakeLists.txt`**

In `/home/yvan/Projects/AudioPlugins/MasterTweak/CMakeLists.txt`, add after `add_subdirectory(core)`:

```cmake
add_subdirectory(preset_builder)
```

- [ ] **Step 6: Write empty `tests/test_preset_builder.cpp`**

```cpp
#include <doctest.h>

// Preset builder tests — added task by task.
```

- [ ] **Step 7: Update `tests/CMakeLists.txt`**

```cmake
add_executable(mastertweak_tests
    test_main.cpp
    test_sanity.cpp
    test_io.cpp
    test_analysis.cpp
    test_preset.cpp
    test_advice.cpp
    test_dsp.cpp
    test_pipeline.cpp
    test_target_level.cpp
    test_gain_stager.cpp
    test_preset_builder.cpp
)

target_link_libraries(mastertweak_tests
    PRIVATE
        mastertweak_core
        preset_builder_core
        doctest
)

add_test(NAME mastertweak_tests COMMAND mastertweak_tests)
```

- [ ] **Step 8: Create stub `.cpp` source files so the lib compiles with no sources missing**

Create each of the six `.cpp` files with just an `#include` comment so CMake does not complain about empty translation units:

```bash
echo "// stub" > /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/src/services/stats_service.cpp
echo "// stub" > /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/src/services/export_service.cpp
echo "// stub" > /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/src/services/ingest_service.cpp
echo "// stub" > /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/src/adapters/database.cpp
echo "// stub" > /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/src/adapters/sqlite_track_repository.cpp
echo "// stub" > /home/yvan/Projects/AudioPlugins/MasterTweak/preset_builder/src/adapters/sqlite_preset_repository.cpp
```

- [ ] **Step 9: Configure and build**

```bash
cmake -B /home/yvan/Projects/AudioPlugins/MasterTweak/build \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      /home/yvan/Projects/AudioPlugins/MasterTweak
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings. The existing 35 tests still pass.

- [ ] **Step 10: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add preset_builder/CMakeLists.txt \
        third_party/picosha2.h \
        tests/test_preset_builder.cpp \
        tests/CMakeLists.txt \
        CMakeLists.txt
git commit -m "feat: scaffold preset_builder_core (empty lib + SQLite3 + picosha2)"
```

---

### Task 2: Domain model headers

**Files:**
- Create: `preset_builder/include/preset_builder/domain/track.hpp`
- Create: `preset_builder/include/preset_builder/domain/preset.hpp`

- [ ] **Step 1: Write `preset_builder/include/preset_builder/domain/track.hpp`**

```cpp
#pragma once

#include <array>
#include <optional>
#include <string>

namespace pb {

struct TrackId {
    std::string hash;  // SHA-256 hex (64 chars)
    bool operator==(const TrackId&) const = default;
};

enum class MetadataSource { acoustid, filename };

struct TrackMetadata {
    std::optional<std::string> title;
    std::optional<std::string> artist;
    std::optional<std::string> album;
    std::optional<std::string> genre;
    std::optional<int>         year;
    MetadataSource             source = MetadataSource::filename;
};

struct TrackAnalysis {
    std::array<float, 7> bandRmsDb{};       // per-band long-term RMS (dBFS)
    std::array<float, 7> bandCorr{};        // per-band L/R Pearson correlation
    std::array<float, 7> bandTransientDb{}; // per-band crest factor (dB)
    float                overallRmsDb = 0.f;
    float                overallCorr  = 1.f;
};

// All fields optional; only provided fields become WHERE clauses (AND).
// Empty TrackFilter means "all tracks".
struct TrackFilter {
    std::optional<std::string> title;
    std::optional<std::string> artist;
    std::optional<std::string> genre;
};

struct Track {
    TrackId       id;
    std::string   path;
    TrackMetadata metadata;
    TrackAnalysis analysis;
    std::string   addedAt;  // ISO 8601 (UTC)
};

} // namespace pb
```

- [ ] **Step 2: Write `preset_builder/include/preset_builder/domain/preset.hpp`**

```cpp
#pragma once

#include "preset_builder/domain/track.hpp"

#include <array>
#include <string>
#include <vector>

namespace pb {

struct PresetId {
    std::string uuid;
    bool operator==(const PresetId&) const = default;
};

// Statistics computed on demand from a Preset's Track collection — never persisted.
struct PresetStats {
    std::array<float, 7> bandRmsDb{};       // mean of bandRmsDb across tracks
    std::array<float, 7> bandCorrMin{};     // p10  of bandCorr   across tracks
    std::array<float, 7> bandTransientDb{}; // median of bandTransientDb across tracks
    float                overallRmsDb   = 0.f;  // mean   of overallRmsDb
    float                overallCorrMin = 0.f;  // p10    of overallCorr
};

struct Preset {
    PresetId             id;
    std::string          name;
    std::string          description;
    std::vector<TrackId> trackIds;
    std::string          createdAt;  // ISO 8601 (UTC)
};

} // namespace pb
```

- [ ] **Step 3: Build to verify headers compile**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build (stub `.cpp` files now have visible headers — no include yet, but CMake target is reachable).

- [ ] **Step 4: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add preset_builder/include/preset_builder/domain/track.hpp \
        preset_builder/include/preset_builder/domain/preset.hpp
git commit -m "feat: add pb domain model (Track, Preset, TrackFilter, PresetStats)"
```

---

### Task 3: Port interfaces

**Files:**
- Create: `preset_builder/include/preset_builder/ports/track_repository.hpp`
- Create: `preset_builder/include/preset_builder/ports/preset_repository.hpp`
- Create: `preset_builder/include/preset_builder/ports/metadata_provider.hpp`

- [ ] **Step 1: Write `preset_builder/include/preset_builder/ports/track_repository.hpp`**

```cpp
#pragma once

#include "preset_builder/domain/track.hpp"

#include <optional>
#include <vector>

namespace pb {

class TrackRepository {
public:
    virtual ~TrackRepository() = default;

    virtual std::optional<Track> find(const TrackId& id) const = 0;
    virtual std::optional<Track> findByPath(const std::string& path) const = 0;

    // Returns all tracks matching the filter (AND of provided fields).
    // An empty TrackFilter returns all tracks.
    virtual std::vector<Track>   search(const TrackFilter& filter) const = 0;

    virtual void save(const Track& track) = 0;       // insert or update
    virtual void remove(const TrackId& id) = 0;
};

} // namespace pb
```

- [ ] **Step 2: Write `preset_builder/include/preset_builder/ports/preset_repository.hpp`**

```cpp
#pragma once

#include "preset_builder/domain/preset.hpp"
#include "preset_builder/domain/track.hpp"

#include <optional>
#include <vector>

namespace pb {

class PresetRepository {
public:
    virtual ~PresetRepository() = default;

    virtual std::optional<Preset> find(const PresetId& id) const = 0;
    virtual std::vector<Preset>   listAll() const = 0;

    // JOIN across preset_tracks + tracks — returns full Track objects.
    virtual std::vector<Track>    tracksFor(const PresetId& id) const = 0;

    virtual void save(const Preset& preset) = 0;     // insert or update
    virtual void remove(const PresetId& id) = 0;
};

} // namespace pb
```

- [ ] **Step 3: Write `preset_builder/include/preset_builder/ports/metadata_provider.hpp`**

```cpp
#pragma once

#include "preset_builder/domain/track.hpp"

#include <optional>
#include <string>

namespace pb {

// Port: implemented in the GUI layer (AcoustIdMetadataProvider) so the core
// lib stays Qt-free. IngestService calls this and falls back to filename
// parsing when it returns nullopt.
class MetadataProvider {
public:
    virtual ~MetadataProvider() = default;
    virtual std::optional<TrackMetadata> lookup(const std::string& audioFilePath) = 0;
};

} // namespace pb
```

- [ ] **Step 4: Build**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add preset_builder/include/preset_builder/ports/
git commit -m "feat: add pb port interfaces (TrackRepository, PresetRepository, MetadataProvider)"
```

---

### Task 4: `StatsService` (TDD)

**Files:**
- Create: `preset_builder/include/preset_builder/services/stats_service.hpp`
- Create: `preset_builder/src/services/stats_service.cpp`
- Modify: `tests/test_preset_builder.cpp`

- [ ] **Step 1: Write the failing tests in `tests/test_preset_builder.cpp`**

```cpp
#include "preset_builder/services/stats_service.hpp"

#include <doctest.h>

#include <array>
#include <cmath>

using pb::StatsService;
using pb::Track;
using pb::TrackAnalysis;
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
    CHECK(stats.bandRmsDb[0]       == doctest::Approx(0.f));
    CHECK(stats.overallRmsDb       == doctest::Approx(0.f));
    CHECK(stats.overallCorrMin     == doctest::Approx(0.f));
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

    // Band 0 only; other bands set to 0 for brevity.
    std::array<float, 7> z{};

    // bandRmsDb band0: mean of -20, -18, -16 = -18
    // bandCorr  band0: p10 of 0.3, 0.5, 0.8 = sorted[floor(0.1*3)=0] = 0.3
    // bandTrans band0: median of 5, 3, 7 = sorted[3,5,7] middle = 5
    // overallRmsDb: mean of -10, -20, -15 = -15
    // overallCorr p10: sorted [0.4, 0.7, 0.9], p10 idx=0 → 0.4
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
    // bandTransientDb band0: sorted [3,5,7,9] → median = (5+7)/2 = 6
    std::array<float, 7> z{};
    Track t0 = makeTrack(z, z, {3,0,0,0,0,0,0}, 0.f, 0.f);
    Track t1 = makeTrack(z, z, {9,0,0,0,0,0,0}, 0.f, 0.f);
    Track t2 = makeTrack(z, z, {5,0,0,0,0,0,0}, 0.f, 0.f);
    Track t3 = makeTrack(z, z, {7,0,0,0,0,0,0}, 0.f, 0.f);

    auto stats = svc.compute({t0, t1, t2, t3});

    CHECK(stats.bandTransientDb[0] == doctest::Approx(6.f));
}
```

- [ ] **Step 2: Run to see the tests fail**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: compile error — `preset_builder/services/stats_service.hpp` not found. That is the expected failure.

- [ ] **Step 3: Write `preset_builder/include/preset_builder/services/stats_service.hpp`**

```cpp
#pragma once

#include "preset_builder/domain/preset.hpp"
#include "preset_builder/domain/track.hpp"

#include <vector>

namespace pb {

class StatsService {
public:
    // Returns zero-filled PresetStats for an empty collection.
    PresetStats compute(const std::vector<Track>& tracks) const;
};

} // namespace pb
```

- [ ] **Step 4: Write `preset_builder/src/services/stats_service.cpp`**

```cpp
#include "preset_builder/services/stats_service.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace pb {

PresetStats StatsService::compute(const std::vector<Track>& tracks) const {
    if (tracks.empty()) return {};

    const auto n  = static_cast<double>(tracks.size());
    const auto ni = tracks.size();
    PresetStats s;

    for (int bi = 0; bi < 7; ++bi) {
        const auto b = static_cast<size_t>(bi);

        // Mean for bandRmsDb
        double sum = 0.0;
        for (const auto& t : tracks) sum += t.analysis.bandRmsDb[b];
        s.bandRmsDb[b] = static_cast<float>(sum / n);

        // p10 for bandCorrMin
        std::vector<float> corrs;
        corrs.reserve(ni);
        for (const auto& t : tracks) corrs.push_back(t.analysis.bandCorr[b]);
        std::sort(corrs.begin(), corrs.end());
        s.bandCorrMin[b] = corrs[static_cast<size_t>(std::floor(0.1 * n))];

        // Median for bandTransientDb
        std::vector<float> trans;
        trans.reserve(ni);
        for (const auto& t : tracks) trans.push_back(t.analysis.bandTransientDb[b]);
        std::sort(trans.begin(), trans.end());
        const size_t mid = ni / 2;
        s.bandTransientDb[b] = (ni % 2 == 1)
            ? trans[mid]
            : (trans[mid - 1] + trans[mid]) / 2.f;
    }

    // Mean for overallRmsDb
    double sumRms = 0.0;
    for (const auto& t : tracks) sumRms += t.analysis.overallRmsDb;
    s.overallRmsDb = static_cast<float>(sumRms / n);

    // p10 for overallCorrMin
    std::vector<float> corrs;
    corrs.reserve(ni);
    for (const auto& t : tracks) corrs.push_back(t.analysis.overallCorr);
    std::sort(corrs.begin(), corrs.end());
    s.overallCorrMin = corrs[static_cast<size_t>(std::floor(0.1 * n))];

    return s;
}

} // namespace pb
```

- [ ] **Step 5: Build and run tests**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1 && \
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass (including the 4 new StatsService tests).

- [ ] **Step 6: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add preset_builder/include/preset_builder/services/stats_service.hpp \
        preset_builder/src/services/stats_service.cpp \
        tests/test_preset_builder.cpp
git commit -m "feat: add StatsService (mean/p10/median across track collection)"
```

---

### Task 5: `ExportService` (TDD)

**Files:**
- Create: `preset_builder/include/preset_builder/services/export_service.hpp`
- Create: `preset_builder/src/services/export_service.cpp`
- Modify: `tests/test_preset_builder.cpp`

- [ ] **Step 1: Add tests to `tests/test_preset_builder.cpp`**

Append after the StatsService tests:

```cpp
#include "preset_builder/services/export_service.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

static std::string readFile(const std::string& path) {
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

    const auto tmp = (fs::temp_directory_path() / "pb_test_export.xml").string();

    pb::ExportService svc;
    svc.exportXml(preset, stats, tmp);

    const std::string xml = readFile(tmp);
    fs::remove(tmp);

    CHECK(xml.find("name=\"Test Preset\"")                   != std::string::npos);
    CHECK(xml.find("description=\"Unit test preset\"")        != std::string::npos);
    CHECK(xml.find("<bandRmsDb>")                             != std::string::npos);
    CHECK(xml.find("<bandMinCorr>")                           != std::string::npos);
    CHECK(xml.find("<bandTransientDb>")                       != std::string::npos);
    CHECK(xml.find("<overallRmsDb>")                          != std::string::npos);
    CHECK(xml.find("<overallMinCorr>")                        != std::string::npos);
    // Spot-check a value that appears in the bandRmsDb array
    CHECK(xml.find("-20.")                                    != std::string::npos);
}

TEST_CASE("ExportService::exportXml: XML special chars in name are escaped") {
    pb::Preset preset;
    preset.name        = "Rock & Roll";
    preset.description = "Test \"quotes\"";

    pb::PresetStats stats;  // zero-filled is fine for this test

    const auto tmp = (fs::temp_directory_path() / "pb_test_escape.xml").string();
    pb::ExportService svc;
    svc.exportXml(preset, stats, tmp);

    const std::string xml = readFile(tmp);
    fs::remove(tmp);

    CHECK(xml.find("&amp;")  != std::string::npos);
    CHECK(xml.find("&quot;") != std::string::npos);
}
```

- [ ] **Step 2: Run to see compile failure**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: compile error — `export_service.hpp` not found.

- [ ] **Step 3: Write `preset_builder/include/preset_builder/services/export_service.hpp`**

```cpp
#pragma once

#include "preset_builder/domain/preset.hpp"

#include <string>

namespace pb {

class ExportService {
public:
    // Writes MixAdvice-compatible XML to outputPath.
    // Throws std::runtime_error if the file cannot be written.
    void exportXml(const Preset& preset,
                   const PresetStats& stats,
                   const std::string& outputPath) const;
};

} // namespace pb
```

- [ ] **Step 4: Write `preset_builder/src/services/export_service.cpp`**

```cpp
#include "preset_builder/services/export_service.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace pb {

namespace {

std::string xmlEscape(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (const char c : s) {
        switch (c) {
            case '&':  r += "&amp;";  break;
            case '"':  r += "&quot;"; break;
            case '<':  r += "&lt;";   break;
            case '>':  r += "&gt;";   break;
            default:   r += c;        break;
        }
    }
    return r;
}

std::string formatBand(const std::array<float, 7>& v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    for (int i = 0; i < 7; ++i) {
        if (i > 0) oss << ' ';
        oss << v[static_cast<size_t>(i)];
    }
    return oss.str();
}

} // namespace

void ExportService::exportXml(const Preset& preset,
                              const PresetStats& stats,
                              const std::string& outputPath) const {
    std::ofstream out(outputPath);
    if (!out) throw std::runtime_error("ExportService: cannot write to " + outputPath);

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<preset"
        << " name=\""        << xmlEscape(preset.name)        << "\""
        << " description=\"" << xmlEscape(preset.description) << "\""
        << ">\n";
    out << std::fixed << std::setprecision(4);
    out << "  <bandRmsDb>"       << formatBand(stats.bandRmsDb)       << "</bandRmsDb>\n";
    out << "  <bandMinCorr>"     << formatBand(stats.bandCorrMin)     << "</bandMinCorr>\n";
    out << "  <bandTransientDb>" << formatBand(stats.bandTransientDb) << "</bandTransientDb>\n";
    out << "  <overallRmsDb>"    << stats.overallRmsDb                << "</overallRmsDb>\n";
    out << "  <overallMinCorr>"  << stats.overallCorrMin              << "</overallMinCorr>\n";
    out << "</preset>\n";
}

} // namespace pb
```

- [ ] **Step 5: Build and run tests**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1 && \
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass (including the 2 new ExportService tests).

- [ ] **Step 6: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add preset_builder/include/preset_builder/services/export_service.hpp \
        preset_builder/src/services/export_service.cpp \
        tests/test_preset_builder.cpp
git commit -m "feat: add ExportService (MixAdvice XML with band arrays + XML escaping)"
```

---

### Task 6: `IngestService` — filename metadata parser (TDD unit)

**Files:**
- Create: `preset_builder/include/preset_builder/services/ingest_service.hpp`
- Create: `preset_builder/src/services/ingest_service.cpp` (partial — parser only)
- Modify: `tests/test_preset_builder.cpp`

- [ ] **Step 1: Add filename parser tests to `tests/test_preset_builder.cpp`**

Append:

```cpp
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
```

- [ ] **Step 2: Run to see compile failure**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: compile error — `ingest_service.hpp` not found.

- [ ] **Step 3: Write `preset_builder/include/preset_builder/services/ingest_service.hpp`**

```cpp
#pragma once

#include "preset_builder/domain/track.hpp"
#include "preset_builder/ports/metadata_provider.hpp"
#include "preset_builder/ports/track_repository.hpp"
#include "mastertweak/pipeline.hpp"

#include <functional>
#include <string>
#include <vector>

namespace pb {

struct IngestReport {
    int added   = 0;
    int skipped = 0;   // already in DB (same SHA-256 hash)
    int failed  = 0;
    std::vector<std::pair<std::string, std::string>> errors;  // {path, message}
};

class IngestService {
public:
    // Ingest a single file or a directory (recursive scan for WAV/FLAC/AIFF/AIF).
    // Skips files already in the repository (by SHA-256 hash).
    // Falls back to parseFilenameMetadata() when MetadataProvider returns nullopt.
    // progress: optional mt::ProgressCallback (fraction in [0,1], stage label).
    IngestReport ingest(const std::string& path,
                        TrackRepository&   repo,
                        MetadataProvider&  metadata,
                        mt::ProgressCallback progress = {}) const;

    // Extract artist and title from a filename (stem only, path prefix stripped).
    // Splits on " - " or " – " (UTF-8 em-dash).
    // Returns TrackMetadata with source == MetadataSource::filename.
    static TrackMetadata parseFilenameMetadata(const std::string& filePath);
};

} // namespace pb
```

- [ ] **Step 4: Write the parser implementation in `preset_builder/src/services/ingest_service.cpp`**

```cpp
#include "preset_builder/services/ingest_service.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace pb {

TrackMetadata IngestService::parseFilenameMetadata(const std::string& filePath) {
    TrackMetadata m;
    m.source = MetadataSource::filename;

    // Strip directory and extension
    fs::path p(filePath);
    std::string stem = p.stem().string();

    // Try splitting on " - " (ASCII hyphen) or " – " (UTF-8 em-dash: 0xE2 0x80 0x93)
    const std::string sepAscii  = " - ";
    const std::string sepEmDash = " \xe2\x80\x93 ";

    auto trySplit = [&](const std::string& sep) -> bool {
        const auto pos = stem.find(sep);
        if (pos == std::string::npos) return false;
        m.artist = stem.substr(0, pos);
        m.title  = stem.substr(pos + sep.size());
        return true;
    };

    if (!trySplit(sepAscii) && !trySplit(sepEmDash))
        m.title = stem;  // no separator — use whole stem as title

    return m;
}

IngestReport IngestService::ingest(const std::string& /*path*/,
                                   TrackRepository&   /*repo*/,
                                   MetadataProvider&  /*metadata*/,
                                   mt::ProgressCallback /*progress*/) const {
    // Implemented in Task 7.
    return {};
}

} // namespace pb
```

- [ ] **Step 5: Build and run tests**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1 && \
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass (4 new parser tests).

- [ ] **Step 6: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add preset_builder/include/preset_builder/services/ingest_service.hpp \
        preset_builder/src/services/ingest_service.cpp \
        tests/test_preset_builder.cpp
git commit -m "feat: add IngestService::parseFilenameMetadata (artist/title from stem)"
```

---

### Task 7: `IngestService` — full ingest (TDD integration)

**Files:**
- Modify: `preset_builder/src/services/ingest_service.cpp`
- Modify: `tests/test_preset_builder.cpp`

- [ ] **Step 1: Add integration tests to `tests/test_preset_builder.cpp`**

Append:

```cpp
#include "mastertweak/io.hpp"

#include <filesystem>
#include <map>

namespace fs = std::filesystem;

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

// Write a stereo sine WAV to path; returns path.
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
    mt::writeAudioFile(path, f, {24, false});
    return path;
}

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("IngestService::ingest single file: added=1, track stored") {
    const auto wav = writeSineWav(
        (fs::temp_directory_path() / "pb_ingest_test.wav").string());

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
    CHECK(stored.metadata.title.has_value());
    CHECK(*stored.metadata.title == "AcoustID Title");

    fs::remove(wav);
}

TEST_CASE("IngestService::ingest same file twice: second call skipped") {
    const auto wav = writeSineWav(
        (fs::temp_directory_path() / "pb_ingest_dup.wav").string());

    StubTrackRepo repo;
    AlwaysSucceedMetadataProvider meta;
    pb::IngestService svc;

    svc.ingest(wav, repo, meta);
    auto report2 = svc.ingest(wav, repo, meta);

    CHECK(report2.added   == 0);
    CHECK(report2.skipped == 1);
    CHECK(repo.store.size() == 1);

    fs::remove(wav);
}

TEST_CASE("IngestService::ingest: metadata fallback on provider failure") {
    // File named "Test Artist - My Track.wav" — parser should extract artist/title
    const auto wavPath = (fs::temp_directory_path() /
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

    fs::remove(wavPath);
}

TEST_CASE("IngestService::ingest directory: scans recursively") {
    const auto dir = fs::temp_directory_path() / "pb_ingest_dir";
    fs::create_directories(dir / "sub");

    writeSineWav((dir / "track1.wav").string());
    writeSineWav((dir / "sub" / "track2.flac").string());
    // Write a non-audio file that should be ignored
    std::ofstream((dir / "README.txt").string()) << "ignore me";

    StubTrackRepo repo;
    AlwaysSucceedMetadataProvider meta;
    pb::IngestService svc;

    auto report = svc.ingest(dir.string(), repo, meta);

    CHECK(report.added == 2);
    CHECK(report.failed == 0);
    CHECK(repo.store.size() == 2);

    fs::remove_all(dir);
}
```

- [ ] **Step 2: Run to see tests fail**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1 && \
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: the 4 new ingest integration tests fail (the stub `ingest()` returns `{}`).

- [ ] **Step 3: Replace the stub `ingest()` in `preset_builder/src/services/ingest_service.cpp`**

Replace the entire file with the full implementation:

```cpp
#include "preset_builder/services/ingest_service.hpp"

#include "mastertweak/analysis.hpp"
#include "mastertweak/io.hpp"

#include "picosha2.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace pb {

// ─── helpers ──────────────────────────────────────────────────────────────────

static std::string sha256File(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    std::vector<unsigned char> hash(picosha2::k_digest_size);
    picosha2::hash256(ifs, hash.begin(), hash.end());
    return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}

static std::string utcNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static TrackAnalysis toTrackAnalysis(const mt::AnalysisSnapshot& snap) {
    TrackAnalysis a;
    for (int i = 0; i < 7; ++i) {
        const auto si = static_cast<size_t>(i);
        a.bandRmsDb[si]       = snap.bands[si].avgRmsDb;
        a.bandCorr[si]        = snap.bands[si].correlation;
        a.bandTransientDb[si] = snap.bands[si].crestDb;
    }
    a.overallRmsDb = snap.overallAvgDb;
    a.overallCorr  = snap.overallCorr;
    return a;
}

static bool isAudioExtension(const fs::path& p) {
    const std::string ext = p.extension().string();
    return ext == ".wav" || ext == ".flac" || ext == ".aiff" || ext == ".aif";
}

static std::vector<fs::path> collectAudioFiles(const std::string& root) {
    std::vector<fs::path> files;
    if (fs::is_regular_file(root)) {
        if (isAudioExtension(root)) files.push_back(root);
        return files;
    }
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && isAudioExtension(entry.path()))
            files.push_back(entry.path());
    }
    return files;
}

// ─── public API ───────────────────────────────────────────────────────────────

TrackMetadata IngestService::parseFilenameMetadata(const std::string& filePath) {
    TrackMetadata m;
    m.source = MetadataSource::filename;

    fs::path p(filePath);
    std::string stem = p.stem().string();

    const std::string sepAscii  = " - ";
    const std::string sepEmDash = " \xe2\x80\x93 ";

    auto trySplit = [&](const std::string& sep) -> bool {
        const auto pos = stem.find(sep);
        if (pos == std::string::npos) return false;
        m.artist = stem.substr(0, pos);
        m.title  = stem.substr(pos + sep.size());
        return true;
    };

    if (!trySplit(sepAscii) && !trySplit(sepEmDash))
        m.title = stem;

    return m;
}

IngestReport IngestService::ingest(const std::string& path,
                                   TrackRepository&   repo,
                                   MetadataProvider&  metaProvider,
                                   mt::ProgressCallback progress) const {
    IngestReport report;
    const auto files = collectAudioFiles(path);
    const auto total = static_cast<float>(files.size());

    for (size_t i = 0; i < files.size(); ++i) {
        const std::string filePath = files[i].string();

        if (progress)
            progress(static_cast<float>(i) / total, "Ingesting: " + files[i].filename().string());

        // 1. Hash
        std::string hash;
        try {
            hash = sha256File(filePath);
        } catch (...) {
            ++report.failed;
            report.errors.emplace_back(filePath, "Failed to hash file");
            continue;
        }

        // 2. Skip if already in DB
        if (repo.find(TrackId{hash})) {
            ++report.skipped;
            continue;
        }

        // 3. Analyse
        std::string err;
        const auto audio = mt::readAudioFile(filePath, &err);
        if (!audio) {
            ++report.failed;
            report.errors.emplace_back(filePath, "Read failed: " + err);
            continue;
        }
        const auto snap     = mt::analyseFile(*audio);
        const auto analysis = toTrackAnalysis(snap);

        // 4. Metadata (AcoustID → filename fallback)
        auto meta = metaProvider.lookup(filePath);
        if (!meta) meta = parseFilenameMetadata(filePath);

        // 5. Persist
        Track track;
        track.id       = TrackId{hash};
        track.path     = filePath;
        track.metadata = *meta;
        track.analysis = analysis;
        track.addedAt  = utcNow();
        repo.save(track);

        ++report.added;
    }

    if (progress) progress(1.f, "Done");
    return report;
}

} // namespace pb
```

- [ ] **Step 4: Tell CMake where to find picosha2.h by updating `preset_builder/CMakeLists.txt`**

Add `third_party` to the include path so `#include "picosha2.h"` resolves:

```cmake
find_package(SQLite3 REQUIRED)

add_library(preset_builder_core STATIC
    src/services/stats_service.cpp
    src/services/export_service.cpp
    src/services/ingest_service.cpp
    src/adapters/database.cpp
    src/adapters/sqlite_track_repository.cpp
    src/adapters/sqlite_preset_repository.cpp
)

target_include_directories(preset_builder_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(preset_builder_core PUBLIC
    mastertweak_core
    SQLite::SQLite3
)
```

- [ ] **Step 5: Build and run tests**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1 && \
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass (4 new ingest integration tests).

- [ ] **Step 6: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add preset_builder/src/services/ingest_service.cpp \
        preset_builder/CMakeLists.txt \
        tests/test_preset_builder.cpp
git commit -m "feat: implement IngestService::ingest (hash, skip-dup, analyse, metadata fallback)"
```

---

### Task 8: `Database` RAII + `SqliteTrackRepository` (TDD, in-memory DB)

**Files:**
- Create: `preset_builder/include/preset_builder/adapters/database.hpp`
- Create: `preset_builder/src/adapters/database.cpp`
- Create: `preset_builder/include/preset_builder/adapters/sqlite_track_repository.hpp`
- Create: `preset_builder/src/adapters/sqlite_track_repository.cpp`
- Modify: `tests/test_preset_builder.cpp`

- [ ] **Step 1: Write `preset_builder/include/preset_builder/adapters/database.hpp`**

```cpp
#pragma once

#include <string>

struct sqlite3;  // forward declaration — consumers get full API via database.cpp

namespace pb {

// RAII wrapper for a SQLite3 database connection.
// Constructor opens the connection and runs CREATE TABLE IF NOT EXISTS for all tables.
// Use ":memory:" as path for in-memory databases (tests).
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* handle() const;

private:
    sqlite3* db_ = nullptr;

    void createSchema();
};

} // namespace pb
```

- [ ] **Step 2: Write `preset_builder/src/adapters/database.cpp`**

```cpp
#include "preset_builder/adapters/database.hpp"

#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace pb {

static void exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("Database: SQL error: " + msg);
    }
}

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        const std::string msg = db_ ? sqlite3_errmsg(db_) : "cannot open";
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Database: cannot open '" + path + "': " + msg);
    }
    createSchema();
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

sqlite3* Database::handle() const { return db_; }

void Database::createSchema() {
    exec(db_, "PRAGMA foreign_keys = ON;");
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS tracks (
            id       TEXT PRIMARY KEY,
            path     TEXT,
            added_at TEXT
        );
    )");
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS track_metadata (
            track_id TEXT PRIMARY KEY REFERENCES tracks(id) ON DELETE CASCADE,
            title    TEXT,
            artist   TEXT,
            album    TEXT,
            genre    TEXT,
            year     INTEGER,
            source   TEXT
        );
    )");
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS track_analysis (
            track_id           TEXT PRIMARY KEY REFERENCES tracks(id) ON DELETE CASCADE,
            band_rms_db        TEXT,
            band_corr          TEXT,
            band_transient_db  TEXT,
            overall_rms_db     REAL,
            overall_corr       REAL
        );
    )");
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS presets (
            id          TEXT PRIMARY KEY,
            name        TEXT,
            description TEXT,
            created_at  TEXT
        );
    )");
    exec(db_, R"(
        CREATE TABLE IF NOT EXISTS preset_tracks (
            preset_id TEXT REFERENCES presets(id) ON DELETE CASCADE,
            track_id  TEXT REFERENCES tracks(id)  ON DELETE CASCADE,
            PRIMARY KEY (preset_id, track_id)
        );
    )");
}

} // namespace pb
```

- [ ] **Step 3: Write `preset_builder/include/preset_builder/adapters/sqlite_track_repository.hpp`**

```cpp
#pragma once

#include "preset_builder/adapters/database.hpp"
#include "preset_builder/ports/track_repository.hpp"

namespace pb {

class SqliteTrackRepository : public TrackRepository {
public:
    explicit SqliteTrackRepository(Database& db);

    std::optional<Track> find(const TrackId& id) const override;
    std::optional<Track> findByPath(const std::string& path) const override;
    std::vector<Track>   search(const TrackFilter& filter) const override;
    void                 save(const Track& track) override;
    void                 remove(const TrackId& id) override;

private:
    Database& db_;

    static Track rowToTrack(sqlite3_stmt* stmt);
};

} // namespace pb
```

- [ ] **Step 4: Add SqliteTrackRepository tests to `tests/test_preset_builder.cpp`**

Append:

```cpp
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
    CHECK(found->id.hash              == "hash001");
    CHECK(found->path                 == "/tmp/song.wav");
    CHECK(found->metadata.artist      == track.metadata.artist);
    CHECK(found->metadata.title       == track.metadata.title);
    CHECK(found->metadata.genre       == track.metadata.genre);
    CHECK(found->analysis.bandRmsDb[0] == doctest::Approx(-20.f));
    CHECK(found->analysis.bandCorr[0]  == doctest::Approx(0.9f));
    CHECK(found->analysis.overallCorr  == doctest::Approx(0.85f));
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
```

- [ ] **Step 5: Run to see tests fail**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: compile error — `sqlite_track_repository.hpp` not found.

- [ ] **Step 6: Write `preset_builder/src/adapters/sqlite_track_repository.cpp`**

```cpp
#include "preset_builder/adapters/sqlite_track_repository.hpp"

#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace pb {

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string floatArrayToJson(const std::array<float, 7>& a) {
    std::ostringstream oss;
    oss << "[";
    for (int i = 0; i < 7; ++i) {
        if (i > 0) oss << ",";
        oss << a[static_cast<size_t>(i)];
    }
    oss << "]";
    return oss.str();
}

static std::array<float, 7> jsonToFloatArray(const std::string& s) {
    std::array<float, 7> a{};
    // Strip brackets
    std::string inner = s.substr(1, s.size() - 2);
    std::istringstream iss(inner);
    std::string tok;
    int i = 0;
    while (std::getline(iss, tok, ',') && i < 7)
        a[static_cast<size_t>(i++)] = std::stof(tok);
    return a;
}

static std::string optStr(const std::optional<std::string>& o) {
    return o ? *o : "";
}

static std::string sourceStr(MetadataSource s) {
    return s == MetadataSource::acoustid ? "acoustid" : "filename";
}

static MetadataSource sourceFrom(const std::string& s) {
    return s == "acoustid" ? MetadataSource::acoustid : MetadataSource::filename;
}

// ── SqliteTrackRepository ────────────────────────────────────────────────────

SqliteTrackRepository::SqliteTrackRepository(Database& db) : db_(db) {}

void SqliteTrackRepository::save(const Track& t) {
    sqlite3* db = db_.handle();
    sqlite3_stmt* st = nullptr;

    // Insert or replace in tracks
    sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO tracks(id, path, added_at) VALUES(?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, t.id.hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, t.path.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, t.addedAt.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);

    // Insert or replace in track_metadata
    sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO track_metadata"
        "(track_id,title,artist,album,genre,year,source) VALUES(?,?,?,?,?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, t.id.hash.c_str(),                -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, optStr(t.metadata.title).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, optStr(t.metadata.artist).c_str(),-1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, optStr(t.metadata.album).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, optStr(t.metadata.genre).c_str(), -1, SQLITE_TRANSIENT);
    if (t.metadata.year)
        sqlite3_bind_int(st, 6, *t.metadata.year);
    else
        sqlite3_bind_null(st, 6);
    sqlite3_bind_text(st, 7, sourceStr(t.metadata.source).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);

    // Insert or replace in track_analysis
    sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO track_analysis"
        "(track_id,band_rms_db,band_corr,band_transient_db,overall_rms_db,overall_corr)"
        " VALUES(?,?,?,?,?,?);",
        -1, &st, nullptr);
    const auto rmsJson   = floatArrayToJson(t.analysis.bandRmsDb);
    const auto corrJson  = floatArrayToJson(t.analysis.bandCorr);
    const auto transJson = floatArrayToJson(t.analysis.bandTransientDb);
    sqlite3_bind_text(st, 1, t.id.hash.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, rmsJson.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, corrJson.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, transJson.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 5, static_cast<double>(t.analysis.overallRmsDb));
    sqlite3_bind_double(st, 6, static_cast<double>(t.analysis.overallCorr));
    sqlite3_step(st);
    sqlite3_finalize(st);
}

static Track stmtToTrack(sqlite3_stmt* st) {
    Track t;
    t.id.hash = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
    t.path    = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
    t.addedAt = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));

    // metadata columns 3-9
    auto col = [&](int i) -> std::string {
        const unsigned char* v = sqlite3_column_text(st, i);
        return v ? reinterpret_cast<const char*>(v) : "";
    };
    auto optCol = [&](int i) -> std::optional<std::string> {
        const unsigned char* v = sqlite3_column_text(st, i);
        if (!v || *v == '\0') return std::nullopt;
        return std::string(reinterpret_cast<const char*>(v));
    };

    t.metadata.title  = optCol(3);
    t.metadata.artist = optCol(4);
    t.metadata.album  = optCol(5);
    t.metadata.genre  = optCol(6);
    if (sqlite3_column_type(st, 7) != SQLITE_NULL)
        t.metadata.year = sqlite3_column_int(st, 7);
    t.metadata.source = sourceFrom(col(8));

    // analysis columns 9-13
    t.analysis.bandRmsDb       = jsonToFloatArray(col(9));
    t.analysis.bandCorr        = jsonToFloatArray(col(10));
    t.analysis.bandTransientDb = jsonToFloatArray(col(11));
    t.analysis.overallRmsDb    = static_cast<float>(sqlite3_column_double(st, 12));
    t.analysis.overallCorr     = static_cast<float>(sqlite3_column_double(st, 13));
    return t;
}

static const char* kSelectJoin =
    "SELECT t.id, t.path, t.added_at,"
    "       m.title, m.artist, m.album, m.genre, m.year, m.source,"
    "       a.band_rms_db, a.band_corr, a.band_transient_db,"
    "       a.overall_rms_db, a.overall_corr"
    "  FROM tracks t"
    "  LEFT JOIN track_metadata m ON m.track_id = t.id"
    "  LEFT JOIN track_analysis a ON a.track_id = t.id";

std::optional<Track> SqliteTrackRepository::find(const TrackId& id) const {
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kSelectJoin) + " WHERE t.id = ?;";
    sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_text(st, 1, id.hash.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<Track> result;
    if (sqlite3_step(st) == SQLITE_ROW) result = stmtToTrack(st);
    sqlite3_finalize(st);
    return result;
}

std::optional<Track> SqliteTrackRepository::findByPath(const std::string& path) const {
    sqlite3_stmt* st = nullptr;
    std::string sql = std::string(kSelectJoin) + " WHERE t.path = ?;";
    sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &st, nullptr);
    sqlite3_bind_text(st, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<Track> result;
    if (sqlite3_step(st) == SQLITE_ROW) result = stmtToTrack(st);
    sqlite3_finalize(st);
    return result;
}

std::vector<Track> SqliteTrackRepository::search(const TrackFilter& filter) const {
    std::string sql = std::string(kSelectJoin);
    std::vector<std::string> clauses;
    if (filter.title)  clauses.push_back("LOWER(m.title)  LIKE '%' || LOWER(?) || '%'");
    if (filter.artist) clauses.push_back("LOWER(m.artist) LIKE '%' || LOWER(?) || '%'");
    if (filter.genre)  clauses.push_back("LOWER(m.genre)  LIKE '%' || LOWER(?) || '%'");

    if (!clauses.empty()) {
        sql += " WHERE ";
        for (size_t i = 0; i < clauses.size(); ++i) {
            if (i > 0) sql += " AND ";
            sql += clauses[i];
        }
    }
    sql += ";";

    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &st, nullptr);

    int bindIdx = 1;
    if (filter.title)  sqlite3_bind_text(st, bindIdx++, filter.title->c_str(),  -1, SQLITE_TRANSIENT);
    if (filter.artist) sqlite3_bind_text(st, bindIdx++, filter.artist->c_str(), -1, SQLITE_TRANSIENT);
    if (filter.genre)  sqlite3_bind_text(st, bindIdx++, filter.genre->c_str(),  -1, SQLITE_TRANSIENT);

    std::vector<Track> results;
    while (sqlite3_step(st) == SQLITE_ROW) results.push_back(stmtToTrack(st));
    sqlite3_finalize(st);
    return results;
}

void SqliteTrackRepository::remove(const TrackId& id) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_.handle(),
        "DELETE FROM tracks WHERE id = ?;", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, id.hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

} // namespace pb
```

- [ ] **Step 7: Build and run tests**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1 && \
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass (7 new SqliteTrackRepository tests).

- [ ] **Step 8: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add preset_builder/include/preset_builder/adapters/database.hpp \
        preset_builder/src/adapters/database.cpp \
        preset_builder/include/preset_builder/adapters/sqlite_track_repository.hpp \
        preset_builder/src/adapters/sqlite_track_repository.cpp \
        tests/test_preset_builder.cpp
git commit -m "feat: add Database RAII + SqliteTrackRepository (save/find/search/remove)"
```

---

### Task 9: `SqlitePresetRepository` (TDD, in-memory DB)

**Files:**
- Create: `preset_builder/include/preset_builder/adapters/sqlite_preset_repository.hpp`
- Create: `preset_builder/src/adapters/sqlite_preset_repository.cpp`
- Modify: `tests/test_preset_builder.cpp`

- [ ] **Step 1: Add tests to `tests/test_preset_builder.cpp`**

Append:

```cpp
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

    // Re-load the preset and check trackIds are persisted
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
```

- [ ] **Step 2: Run to see compile failure**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: compile error — `sqlite_preset_repository.hpp` not found.

- [ ] **Step 3: Write `preset_builder/include/preset_builder/adapters/sqlite_preset_repository.hpp`**

```cpp
#pragma once

#include "preset_builder/adapters/database.hpp"
#include "preset_builder/ports/preset_repository.hpp"

namespace pb {

class SqlitePresetRepository : public PresetRepository {
public:
    explicit SqlitePresetRepository(Database& db);

    std::optional<Preset> find(const PresetId& id) const override;
    std::vector<Preset>   listAll() const override;
    std::vector<Track>    tracksFor(const PresetId& id) const override;
    void                  save(const Preset& preset) override;
    void                  remove(const PresetId& id) override;

private:
    Database& db_;
};

} // namespace pb
```

- [ ] **Step 4: Write `preset_builder/src/adapters/sqlite_preset_repository.cpp`**

```cpp
#include "preset_builder/adapters/sqlite_preset_repository.hpp"

#include <sqlite3.h>
#include <sstream>

namespace pb {

SqlitePresetRepository::SqlitePresetRepository(Database& db) : db_(db) {}

static Preset stmtToPreset(sqlite3_stmt* st) {
    Preset p;
    p.id.uuid     = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
    p.name        = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
    p.description = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
    p.createdAt   = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
    return p;
}

static void loadTrackIds(sqlite3* db, Preset& p) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT track_id FROM preset_tracks WHERE preset_id = ?;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, p.id.uuid.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
        TrackId tid;
        tid.hash = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
        p.trackIds.push_back(tid);
    }
    sqlite3_finalize(st);
}

std::optional<Preset> SqlitePresetRepository::find(const PresetId& id) const {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_.handle(),
        "SELECT id, name, description, created_at FROM presets WHERE id = ?;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, id.uuid.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<Preset> result;
    if (sqlite3_step(st) == SQLITE_ROW) {
        result = stmtToPreset(st);
        loadTrackIds(db_.handle(), *result);
    }
    sqlite3_finalize(st);
    return result;
}

std::vector<Preset> SqlitePresetRepository::listAll() const {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_.handle(),
        "SELECT id, name, description, created_at FROM presets;",
        -1, &st, nullptr);

    std::vector<Preset> results;
    while (sqlite3_step(st) == SQLITE_ROW) {
        auto p = stmtToPreset(st);
        loadTrackIds(db_.handle(), p);
        results.push_back(std::move(p));
    }
    sqlite3_finalize(st);
    return results;
}

std::vector<Track> SqlitePresetRepository::tracksFor(const PresetId& id) const {
    // Use SqliteTrackRepository's join logic via a direct join query.
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT t.id, t.path, t.added_at,"
        "       m.title, m.artist, m.album, m.genre, m.year, m.source,"
        "       a.band_rms_db, a.band_corr, a.band_transient_db,"
        "       a.overall_rms_db, a.overall_corr"
        "  FROM tracks t"
        "  INNER JOIN preset_tracks pt ON pt.track_id = t.id"
        "  LEFT JOIN track_metadata m  ON m.track_id  = t.id"
        "  LEFT JOIN track_analysis a  ON a.track_id  = t.id"
        "  WHERE pt.preset_id = ?;";

    sqlite3_prepare_v2(db_.handle(), sql, -1, &st, nullptr);
    sqlite3_bind_text(st, 1, id.uuid.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<Track> tracks;
    while (sqlite3_step(st) == SQLITE_ROW) {
        // Build Track directly from the 14 columns (same layout as kSelectJoin).
        Track t;
        t.id.hash = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
        t.path    = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        t.addedAt = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));

        auto col = [&](int i) -> std::string {
            const unsigned char* v = sqlite3_column_text(st, i);
            return v ? reinterpret_cast<const char*>(v) : "";
        };
        auto optCol = [&](int i) -> std::optional<std::string> {
            const unsigned char* v = sqlite3_column_text(st, i);
            if (!v || *v == '\0') return std::nullopt;
            return std::string(reinterpret_cast<const char*>(v));
        };
        auto jsonToArr = [](const std::string& s) -> std::array<float, 7> {
            std::array<float, 7> a{};
            if (s.size() < 2) return a;
            std::string inner = s.substr(1, s.size() - 2);
            std::istringstream iss(inner);
            std::string tok;
            int i = 0;
            while (std::getline(iss, tok, ',') && i < 7)
                a[static_cast<size_t>(i++)] = std::stof(tok);
            return a;
        };

        t.metadata.title  = optCol(3);
        t.metadata.artist = optCol(4);
        t.metadata.album  = optCol(5);
        t.metadata.genre  = optCol(6);
        if (sqlite3_column_type(st, 7) != SQLITE_NULL)
            t.metadata.year = sqlite3_column_int(st, 7);
        t.metadata.source = col(8) == "acoustid"
            ? MetadataSource::acoustid : MetadataSource::filename;

        t.analysis.bandRmsDb       = jsonToArr(col(9));
        t.analysis.bandCorr        = jsonToArr(col(10));
        t.analysis.bandTransientDb = jsonToArr(col(11));
        t.analysis.overallRmsDb    = static_cast<float>(sqlite3_column_double(st, 12));
        t.analysis.overallCorr     = static_cast<float>(sqlite3_column_double(st, 13));

        tracks.push_back(t);
    }
    sqlite3_finalize(st);
    return tracks;
}

void SqlitePresetRepository::save(const Preset& preset) {
    sqlite3* db = db_.handle();
    sqlite3_stmt* st = nullptr;

    sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO presets(id, name, description, created_at)"
        " VALUES(?,?,?,?);",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, preset.id.uuid.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, preset.name.c_str(),           -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, preset.description.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, preset.createdAt.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);

    // Replace track associations
    sqlite3_prepare_v2(db,
        "DELETE FROM preset_tracks WHERE preset_id = ?;",
        -1, &st, nullptr);
    sqlite3_bind_text(st, 1, preset.id.uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);

    for (const auto& tid : preset.trackIds) {
        sqlite3_prepare_v2(db,
            "INSERT OR IGNORE INTO preset_tracks(preset_id, track_id) VALUES(?,?);",
            -1, &st, nullptr);
        sqlite3_bind_text(st, 1, preset.id.uuid.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, tid.hash.c_str(),        -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

void SqlitePresetRepository::remove(const PresetId& id) {
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db_.handle(),
        "DELETE FROM presets WHERE id = ?;", -1, &st, nullptr);
    sqlite3_bind_text(st, 1, id.uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

} // namespace pb
```

- [ ] **Step 5: Build and run all tests**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1 && \
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass (5 new SqlitePresetRepository tests, all previous still pass).

- [ ] **Step 6: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add preset_builder/include/preset_builder/adapters/sqlite_preset_repository.hpp \
        preset_builder/src/adapters/sqlite_preset_repository.cpp \
        tests/test_preset_builder.cpp
git commit -m "feat: add SqlitePresetRepository (save/find/listAll/tracksFor/remove)"
```

---

### Task 10: Final verification + TODO update

**Files:**
- Modify: `TODO.md`

- [ ] **Step 1: Run full build and test suite**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1 && \
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: clean build, zero warnings, all tests pass (35 existing + ~25 new preset builder tests).

- [ ] **Step 2: Update `TODO.md`**

Find:
```
- Copy MixAdvice preset editor feature into MasterTweak.
```

Replace with:
```
- Copy MixAdvice preset editor feature into MasterTweak.
  - ~~Sub-project A: `preset_builder_core` static lib (domain model, StatsService, ExportService, IngestService, SQLite adapters)~~ **DONE**
  - Sub-project B: Qt6 Preset Builder dialog (Ingest / Browse+Tag / Create Preset screens) — spec + plan pending.
```

- [ ] **Step 3: Final commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add TODO.md
git commit -m "docs: mark preset_builder_core (Sub-project A) as done in TODO"
```
