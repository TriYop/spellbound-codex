# Preset Builder Dialog — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the Qt6 Preset Builder dialog (Sub-project B) — a non-modal three-tab window for ingesting audio tracks into a shared SQLite library, browsing/tagging them, and exporting MixAdvice-compatible preset XMLs.

**Architecture:** `PresetBuilderCtx` (plain struct, no QObject) owns `Database`, both repositories, and `AcoustIdMetadataProvider`. Three `QWidget` tab classes (`IngestTab`, `BrowseTab`, `CreatePresetTab`) each receive a `PresetBuilderCtx&`. Worker threads (`IngestWorker`, `StatsWorker`) follow the existing `QThread` subclass pattern. `PresetBuilderDialog` wires everything and relays `presetExported` to `MainWindow` so `PresetSelector` refreshes.

**Tech Stack:** C++20, Qt6 (Widgets + Network + Core), `preset_builder_core` (Sub-project A), `QProcess` for fpcalc, `QNetworkAccessManager` for AcoustID API.

---

## File Map

| Action | Path | Purpose |
|--------|------|---------|
| Create | `gui/utils.h` | Free function `sanitizePresetName(std::string)` |
| Create | `gui/AcoustIdMetadataProvider.h` | `MetadataProvider` Qt implementation declaration |
| Create | `gui/AcoustIdMetadataProvider.cpp` | fpcalc subprocess + AcoustID HTTP lookup |
| Create | `gui/PresetBuilderCtx.h` | Plain struct owning DB + repos + provider |
| Create | `gui/PresetBuilderDialog.h` | `QDialog` declaration |
| Create | `gui/PresetBuilderDialog.cpp` | Creates ctx, tab widget, wires cross-tab signals |
| Create | `gui/IngestTab.h` | Ingest tab + `IngestWorker` declarations |
| Create | `gui/IngestTab.cpp` | Drop zone, directory picker, progress, report |
| Create | `gui/BrowseTab.h` | Browse/tag tab declaration |
| Create | `gui/BrowseTab.cpp` | Search fields, table with inline edit, delete |
| Create | `gui/CreatePresetTab.h` | Create Preset tab + `StatsWorker` declarations |
| Create | `gui/CreatePresetTab.cpp` | Track selection, stats preview, export |
| Modify | `gui/PresetSelector.h` | Add `manageBtn_` + `manageRequested()` signal |
| Modify | `gui/PresetSelector.cpp` | Add "Manage presets…" button, wire signal |
| Modify | `gui/MainWindow.h` | Add `presetBuilderDialog_` member |
| Modify | `gui/MainWindow.cpp` | Lazy-construct dialog, extract `sanitizePresetName`, connect signals |
| Modify | `gui/CMakeLists.txt` | Add 12 new sources, link `preset_builder_core` + `Qt6::Network` |
| Modify | `CMakeLists.txt` | Add `Network` to `find_package(Qt6 ...)` |

---

### Task 1: Infrastructure — CMakeLists, Qt6::Network, `gui/utils.h`

**Files:**
- Create: `gui/utils.h`
- Modify: `CMakeLists.txt`
- Modify: `gui/CMakeLists.txt`
- Modify: `gui/MainWindow.h`
- Modify: `gui/MainWindow.cpp`

- [ ] **Step 1: Create `gui/utils.h`** with `sanitizePresetName` extracted from `MainWindow`

```cpp
// gui/utils.h
#pragma once

#include <cctype>
#include <string>

namespace gui {

// Convert an arbitrary string into a filename-safe lowercase stem (max 24 chars).
inline std::string sanitizePresetName(const std::string& name) {
    std::string s;
    s.reserve(name.size());
    for (char ch : name) {
        const auto c = static_cast<unsigned char>(ch);
        s += std::isalnum(c) ? static_cast<char>(std::tolower(c)) : '_';
    }
    std::string out;
    bool prevUnder = false;
    for (char c : s) {
        if (c == '_') { if (!prevUnder) out += c; prevUnder = true; }
        else          { out += c; prevUnder = false; }
    }
    auto start = out.find_first_not_of('_');
    if (start == std::string::npos) return "preset";
    out = out.substr(start);
    auto end = out.find_last_not_of('_');
    if (end != std::string::npos) out = out.substr(0, end + 1);
    if (out.size() > 24) out.resize(24);
    auto end2 = out.find_last_not_of('_');
    if (end2 != std::string::npos) out = out.substr(0, end2 + 1);
    return out.empty() ? "preset" : out;
}

} // namespace gui
```

- [ ] **Step 2: Add `Network` to `find_package(Qt6 ...)` in root `CMakeLists.txt`**

Find the line:
```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
```
Replace with:
```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
```

- [ ] **Step 3: Update `gui/CMakeLists.txt`** — add all new sources, link `preset_builder_core` + `Qt6::Network`

```cmake
qt_add_executable(MasterTweak
    main.cpp
    MainWindow.h
    MainWindow.cpp
    utils.h
    AudioControl.h
    AudioControl.cpp
    RotaryKnob.h
    RotaryKnob.cpp
    VerticalFader.h
    VerticalFader.cpp
    ChainPanel.h
    ChainPanel.cpp
    PresetSelector.h
    PresetSelector.cpp
    TransportWidget.h
    TransportWidget.cpp
    TargetLevelCombo.h
    TargetLevelCombo.cpp
    VUMeterWidget.h
    VUMeterWidget.cpp
    AcoustIdMetadataProvider.h
    AcoustIdMetadataProvider.cpp
    PresetBuilderCtx.h
    PresetBuilderDialog.h
    PresetBuilderDialog.cpp
    IngestTab.h
    IngestTab.cpp
    BrowseTab.h
    BrowseTab.cpp
    CreatePresetTab.h
    CreatePresetTab.cpp
)

target_link_libraries(MasterTweak
    PRIVATE
        mastertweak_core
        preset_builder_core
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        miniaudio
)

set_target_properties(MasterTweak PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
```

- [ ] **Step 4: Remove `sanitizePresetName` from `gui/MainWindow.h`**

In `gui/MainWindow.h`, remove the private declaration:
```cpp
    static std::string sanitizePresetName(const std::string& name);
```

- [ ] **Step 5: Update `gui/MainWindow.cpp`**

Add `#include "utils.h"` after the other local includes. Then replace the `sanitizePresetName` static method body (around line 324) with a call to the free function — change `MainWindow::sanitizePresetName(...)` calls to `gui::sanitizePresetName(...)`. Remove the static method definition entirely.

In `makeDefaultOutputPath`:
```cpp
    const std::string suffix = currentPreset_ ? gui::sanitizePresetName(currentPreset_->name) : "master";
```

- [ ] **Step 6: Create stub `.cpp` files for the new widgets** so the build doesn't fail on missing sources yet

```bash
echo '#include "AcoustIdMetadataProvider.h"' > /home/yvan/Projects/AudioPlugins/MasterTweak/gui/AcoustIdMetadataProvider.cpp
echo '#include "PresetBuilderDialog.h"'       > /home/yvan/Projects/AudioPlugins/MasterTweak/gui/PresetBuilderDialog.cpp
echo '#include "IngestTab.h"'                 > /home/yvan/Projects/AudioPlugins/MasterTweak/gui/IngestTab.cpp
echo '#include "BrowseTab.h"'                 > /home/yvan/Projects/AudioPlugins/MasterTweak/gui/BrowseTab.cpp
echo '#include "CreatePresetTab.h"'           > /home/yvan/Projects/AudioPlugins/MasterTweak/gui/CreatePresetTab.cpp
```

Create the stub headers (just `#pragma once` for now — they'll be replaced in later tasks):

```bash
printf '#pragma once\n' > /home/yvan/Projects/AudioPlugins/MasterTweak/gui/AcoustIdMetadataProvider.h
printf '#pragma once\n' > /home/yvan/Projects/AudioPlugins/MasterTweak/gui/PresetBuilderCtx.h
printf '#pragma once\n' > /home/yvan/Projects/AudioPlugins/MasterTweak/gui/PresetBuilderDialog.h
printf '#pragma once\n' > /home/yvan/Projects/AudioPlugins/MasterTweak/gui/IngestTab.h
printf '#pragma once\n' > /home/yvan/Projects/AudioPlugins/MasterTweak/gui/BrowseTab.h
printf '#pragma once\n' > /home/yvan/Projects/AudioPlugins/MasterTweak/gui/CreatePresetTab.h
```

- [ ] **Step 7: Configure and build**

```bash
cmake -B /home/yvan/Projects/AudioPlugins/MasterTweak/build \
      -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      /home/yvan/Projects/AudioPlugins/MasterTweak 2>&1
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: clean build, zero warnings, all tests pass.

- [ ] **Step 8: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/utils.h gui/CMakeLists.txt gui/MainWindow.h gui/MainWindow.cpp \
        CMakeLists.txt \
        gui/AcoustIdMetadataProvider.h gui/AcoustIdMetadataProvider.cpp \
        gui/PresetBuilderCtx.h \
        gui/PresetBuilderDialog.h gui/PresetBuilderDialog.cpp \
        gui/IngestTab.h gui/IngestTab.cpp \
        gui/BrowseTab.h gui/BrowseTab.cpp \
        gui/CreatePresetTab.h gui/CreatePresetTab.cpp
git commit -m "feat: scaffold preset builder dialog files + Qt6::Network + utils.h"
```

---

### Task 2: `AcoustIdMetadataProvider`

**Files:**
- Modify: `gui/AcoustIdMetadataProvider.h`
- Modify: `gui/AcoustIdMetadataProvider.cpp`

- [ ] **Step 1: Write `gui/AcoustIdMetadataProvider.h`**

```cpp
#pragma once

#include "preset_builder/ports/metadata_provider.hpp"

#include <optional>
#include <string>

namespace gui {

// Implements pb::MetadataProvider using fpcalc (fingerprinting) + AcoustID API.
// Best-effort: silently returns nullopt if fpcalc is not on PATH or network fails.
// Called from the IngestWorker thread; creates a fresh QNetworkAccessManager per call.
class AcoustIdMetadataProvider : public pb::MetadataProvider {
public:
    std::optional<pb::TrackMetadata> lookup(const std::string& audioFilePath) override;

private:
    struct Fingerprint { std::string fp; int duration = 0; };
    static std::optional<Fingerprint> runFpcalc(const std::string& path);
    static std::optional<pb::TrackMetadata> queryAcoustId(const Fingerprint& fp);
};

} // namespace gui
```

- [ ] **Step 2: Write `gui/AcoustIdMetadataProvider.cpp`**

```cpp
#include "AcoustIdMetadataProvider.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace gui {

// Free AcoustID test client key — replace with a registered application key for production.
// Register at: https://acoustid.org/new-application
static constexpr const char* kAcoustIdKey = "8XaBELgH";

std::optional<AcoustIdMetadataProvider::Fingerprint>
AcoustIdMetadataProvider::runFpcalc(const std::string& path) {
    QProcess proc;
    proc.start("fpcalc", {"-json", QString::fromStdString(path)});
    if (!proc.waitForFinished(30000) || proc.exitCode() != 0)
        return std::nullopt;

    const QJsonDocument doc = QJsonDocument::fromJson(proc.readAllStandardOutput());
    if (doc.isNull()) return std::nullopt;

    const QJsonObject obj = doc.object();
    Fingerprint fp;
    fp.fp       = obj["fingerprint"].toString().toStdString();
    fp.duration = obj["duration"].toInt(0);
    if (fp.fp.empty() || fp.duration <= 0) return std::nullopt;
    return fp;
}

std::optional<pb::TrackMetadata>
AcoustIdMetadataProvider::queryAcoustId(const Fingerprint& fp) {
    QNetworkAccessManager nam;

    QUrlQuery query;
    query.addQueryItem("client",      kAcoustIdKey);
    query.addQueryItem("fingerprint", QString::fromStdString(fp.fp));
    query.addQueryItem("duration",    QString::number(fp.duration));
    query.addQueryItem("meta",        "recordings+releasegroups");

    QUrl url("https://api.acoustid.org/v2/lookup");
    url.setQuery(query);

    QNetworkRequest req(url);
    QNetworkReply* reply = nam.get(req);

    // Spin a local event loop until the reply arrives or 5 s timeout.
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply,  &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout,         &loop, &QEventLoop::quit);
    timer.start(5000);
    loop.exec();

    if (!reply->isFinished() || reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return std::nullopt;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    if (doc.isNull()) return std::nullopt;

    const QJsonObject root = doc.object();
    if (root["status"].toString() != "ok") return std::nullopt;

    const QJsonArray results = root["results"].toArray();
    if (results.isEmpty()) return std::nullopt;

    const QJsonArray recordings = results[0].toObject()["recordings"].toArray();
    if (recordings.isEmpty()) return std::nullopt;

    const QJsonObject rec = recordings[0].toObject();

    pb::TrackMetadata meta;
    meta.source = pb::MetadataSource::acoustid;
    meta.title  = rec["title"].toString().toStdString();
    if (meta.title && meta.title->empty()) meta.title.reset();

    const QJsonArray artists = rec["artists"].toArray();
    if (!artists.isEmpty()) {
        meta.artist = artists[0].toObject()["name"].toString().toStdString();
        if (meta.artist && meta.artist->empty()) meta.artist.reset();
    }

    const QJsonArray releaseGroups = rec["releasegroups"].toArray();
    if (!releaseGroups.isEmpty()) {
        const QJsonObject rg = releaseGroups[0].toObject();
        meta.album = rg["title"].toString().toStdString();
        if (meta.album && meta.album->empty()) meta.album.reset();
    }

    return meta;
}

std::optional<pb::TrackMetadata>
AcoustIdMetadataProvider::lookup(const std::string& audioFilePath) {
    const auto fp = runFpcalc(audioFilePath);
    if (!fp) return std::nullopt;
    return queryAcoustId(*fp);
}

} // namespace gui
```

- [ ] **Step 3: Build**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings.

- [ ] **Step 4: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/AcoustIdMetadataProvider.h gui/AcoustIdMetadataProvider.cpp
git commit -m "feat: add AcoustIdMetadataProvider (fpcalc + AcoustID HTTP, best-effort)"
```

---

### Task 3: `PresetBuilderCtx` + `PresetBuilderDialog` skeleton

**Files:**
- Modify: `gui/PresetBuilderCtx.h`
- Modify: `gui/PresetBuilderDialog.h`
- Modify: `gui/PresetBuilderDialog.cpp`

- [ ] **Step 1: Write `gui/PresetBuilderCtx.h`**

```cpp
#pragma once

#include "AcoustIdMetadataProvider.h"
#include "preset_builder/adapters/database.hpp"
#include "preset_builder/adapters/sqlite_preset_repository.hpp"
#include "preset_builder/adapters/sqlite_track_repository.hpp"

#include <string>

namespace gui {

// Plain aggregate owning all preset-builder infrastructure.
// Members are initialised in declaration order: db first, then the repositories that reference it.
struct PresetBuilderCtx {
    pb::Database               db;
    pb::SqliteTrackRepository  tracks;
    pb::SqlitePresetRepository presets;
    AcoustIdMetadataProvider   metadata;

    explicit PresetBuilderCtx(const std::string& dbPath)
        : db(dbPath), tracks(db), presets(db) {}
};

} // namespace gui
```

- [ ] **Step 2: Write `gui/PresetBuilderDialog.h`**

```cpp
#pragma once

#include <QDialog>
#include <memory>
#include <string>

namespace gui {

class PresetBuilderCtx;
class IngestTab;
class BrowseTab;
class CreatePresetTab;

class PresetBuilderDialog : public QDialog {
    Q_OBJECT
public:
    explicit PresetBuilderDialog(const std::string& executableDir,
                                 QWidget* parent = nullptr);

signals:
    // Emitted when a preset is exported to ~/.config/MixAdvice/Presets/ so
    // PresetSelector can refresh.
    void presetExported();

private:
    void buildUi();
    void onPresetExported();

    std::string              execDir_;
    std::unique_ptr<PresetBuilderCtx> ctx_;

    IngestTab*       ingestTab_  = nullptr;
    BrowseTab*       browseTab_  = nullptr;
    CreatePresetTab* createTab_  = nullptr;
};

} // namespace gui
```

- [ ] **Step 3: Write `gui/PresetBuilderDialog.cpp`**

```cpp
#include "PresetBuilderDialog.h"
#include "BrowseTab.h"
#include "CreatePresetTab.h"
#include "IngestTab.h"
#include "PresetBuilderCtx.h"

#include <QDir>
#include <QTabWidget>
#include <QVBoxLayout>

namespace gui {

PresetBuilderDialog::PresetBuilderDialog(const std::string& executableDir,
                                         QWidget* parent)
    : QDialog(parent, Qt::Window)  // Qt::Window = resizable, non-modal
    , execDir_(executableDir)
{
    setWindowTitle("Preset Builder");
    resize(900, 640);

    // Ensure the MixAdvice config directory exists.
    const QString configDir = QDir::homePath() + "/.config/MixAdvice";
    QDir().mkpath(configDir);
    const std::string dbPath = (configDir + "/preset_builder.db").toStdString();

    ctx_ = std::make_unique<PresetBuilderCtx>(dbPath);

    buildUi();
}

void PresetBuilderDialog::buildUi() {
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);

    auto* tabs = new QTabWidget(this);

    ingestTab_  = new IngestTab(*ctx_, this);
    browseTab_  = new BrowseTab(*ctx_, this);
    createTab_  = new CreatePresetTab(*ctx_, this);

    tabs->addTab(ingestTab_,  "Ingest");
    tabs->addTab(browseTab_,  "Browse / Tag");
    tabs->addTab(createTab_,  "Create Preset");

    vbox->addWidget(tabs);

    // Cross-tab wiring
    connect(ingestTab_,  &IngestTab::libraryChanged,
            browseTab_,  &BrowseTab::refresh);
    connect(ingestTab_,  &IngestTab::libraryChanged,
            createTab_,  &CreatePresetTab::refreshTrackList);
    connect(browseTab_,  &BrowseTab::libraryChanged,
            createTab_,  &CreatePresetTab::refreshTrackList);
    connect(createTab_,  &CreatePresetTab::presetExported,
            this,        &PresetBuilderDialog::onPresetExported);

    // Initial population
    browseTab_->refresh();
    createTab_->refreshTrackList();
}

void PresetBuilderDialog::onPresetExported() {
    emit presetExported();
}

} // namespace gui
```

- [ ] **Step 4: Write stub `gui/IngestTab.h`** (temporary — replaced in Task 4)

```cpp
#pragma once

#include <QWidget>

namespace gui {
struct PresetBuilderCtx;

class IngestTab : public QWidget {
    Q_OBJECT
public:
    explicit IngestTab(PresetBuilderCtx& ctx, QWidget* parent = nullptr);
signals:
    void libraryChanged();
};
} // namespace gui
```

- [ ] **Step 5: Write stub `gui/BrowseTab.h`** (temporary — replaced in Task 5)

```cpp
#pragma once

#include <QWidget>

namespace gui {
struct PresetBuilderCtx;

class BrowseTab : public QWidget {
    Q_OBJECT
public:
    explicit BrowseTab(PresetBuilderCtx& ctx, QWidget* parent = nullptr);
public slots:
    void refresh();
signals:
    void libraryChanged();
};
} // namespace gui
```

- [ ] **Step 6: Write stub `gui/CreatePresetTab.h`** (temporary — replaced in Task 6)

```cpp
#pragma once

#include <QWidget>

namespace gui {
struct PresetBuilderCtx;

class CreatePresetTab : public QWidget {
    Q_OBJECT
public:
    explicit CreatePresetTab(PresetBuilderCtx& ctx, QWidget* parent = nullptr);
public slots:
    void refreshTrackList();
signals:
    void presetExported();
};
} // namespace gui
```

- [ ] **Step 7: Write stub `.cpp` files for the three tabs**

```cpp
// gui/IngestTab.cpp
#include "IngestTab.h"
#include "PresetBuilderCtx.h"
#include <QLabel>
#include <QVBoxLayout>
namespace gui {
IngestTab::IngestTab(PresetBuilderCtx& /*ctx*/, QWidget* parent)
    : QWidget(parent) {
    auto* l = new QVBoxLayout(this);
    l->addWidget(new QLabel("Ingest (coming soon)", this));
}
} // namespace gui
```

```cpp
// gui/BrowseTab.cpp
#include "BrowseTab.h"
#include "PresetBuilderCtx.h"
#include <QLabel>
#include <QVBoxLayout>
namespace gui {
BrowseTab::BrowseTab(PresetBuilderCtx& /*ctx*/, QWidget* parent)
    : QWidget(parent) {
    auto* l = new QVBoxLayout(this);
    l->addWidget(new QLabel("Browse (coming soon)", this));
}
void BrowseTab::refresh() {}
} // namespace gui
```

```cpp
// gui/CreatePresetTab.cpp
#include "CreatePresetTab.h"
#include "PresetBuilderCtx.h"
#include <QLabel>
#include <QVBoxLayout>
namespace gui {
CreatePresetTab::CreatePresetTab(PresetBuilderCtx& /*ctx*/, QWidget* parent)
    : QWidget(parent) {
    auto* l = new QVBoxLayout(this);
    l->addWidget(new QLabel("Create Preset (coming soon)", this));
}
void CreatePresetTab::refreshTrackList() {}
} // namespace gui
```

Write these three files with the exact content above.

- [ ] **Step 8: Build**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings.

- [ ] **Step 9: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/PresetBuilderCtx.h gui/PresetBuilderDialog.h gui/PresetBuilderDialog.cpp \
        gui/IngestTab.h gui/IngestTab.cpp \
        gui/BrowseTab.h gui/BrowseTab.cpp \
        gui/CreatePresetTab.h gui/CreatePresetTab.cpp
git commit -m "feat: add PresetBuilderDialog skeleton + stub tabs (dialog opens, tabs placeholder)"
```

---

### Task 4: `IngestTab` + `IngestWorker`

**Files:**
- Modify: `gui/IngestTab.h` (replace stub)
- Modify: `gui/IngestTab.cpp` (replace stub)

- [ ] **Step 1: Replace `gui/IngestTab.h`**

```cpp
#pragma once

#include "preset_builder/domain/track.hpp"
#include "preset_builder/services/ingest_service.hpp"

#include <QThread>
#include <QWidget>
#include <string>

namespace gui {
struct PresetBuilderCtx;
}

QT_BEGIN_NAMESPACE
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
QT_END_NAMESPACE

namespace gui {

class IngestWorker : public QThread {
    Q_OBJECT
public:
    explicit IngestWorker(QObject* parent = nullptr);
    void setup(const std::string& path,
               pb::TrackRepository& repo,
               pb::MetadataProvider& meta);

signals:
    void progress(float fraction, const QString& stage);
    void finished(pb::IngestReport report);

protected:
    void run() override;

private:
    std::string           path_;
    pb::TrackRepository*  repo_ = nullptr;
    pb::MetadataProvider* meta_ = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────

class IngestTab : public QWidget {
    Q_OBJECT
public:
    explicit IngestTab(PresetBuilderCtx& ctx, QWidget* parent = nullptr);

signals:
    void libraryChanged();

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onAddFolder();
    void onProgress(float fraction, const QString& stage);
    void onFinished(pb::IngestReport report);

private:
    void startIngest(const std::string& path);
    void setRunning(bool running);

    PresetBuilderCtx& ctx_;
    IngestWorker*     worker_    = nullptr;

    QPushButton*  addBtn_        = nullptr;
    QLabel*       dropLabel_     = nullptr;
    QProgressBar* progressBar_   = nullptr;
    QLabel*       stageLabel_    = nullptr;
    QLabel*       addedLbl_      = nullptr;
    QLabel*       skippedLbl_    = nullptr;
    QLabel*       failedLbl_     = nullptr;
    QListWidget*  errorList_     = nullptr;
};

} // namespace gui
```

- [ ] **Step 2: Replace `gui/IngestTab.cpp`**

```cpp
#include "IngestTab.h"
#include "PresetBuilderCtx.h"

#include "preset_builder/services/ingest_service.hpp"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace gui {

// ─── IngestWorker ────────────────────────────────────────────────────────────

IngestWorker::IngestWorker(QObject* parent) : QThread(parent) {}

void IngestWorker::setup(const std::string& path,
                         pb::TrackRepository& repo,
                         pb::MetadataProvider& meta) {
    path_ = path;
    repo_ = &repo;
    meta_ = &meta;
}

void IngestWorker::run() {
    pb::IngestService svc;
    auto report = svc.ingest(path_, *repo_, *meta_,
        [this](float f, const std::string& s) {
            emit progress(f, QString::fromStdString(s));
        });
    emit finished(std::move(report));
}

// ─── IngestTab ───────────────────────────────────────────────────────────────

IngestTab::IngestTab(PresetBuilderCtx& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx)
{
    setAcceptDrops(true);

    auto* vbox = new QVBoxLayout(this);
    vbox->setSpacing(8);

    // ── Input row ────────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        addBtn_   = new QPushButton("Add folder…", this);
        dropLabel_ = new QLabel("  or drop a folder / files here", this);
        dropLabel_->setStyleSheet(
            "border: 2px dashed #888; border-radius: 4px; padding: 6px; color: #555;");
        connect(addBtn_, &QPushButton::clicked, this, &IngestTab::onAddFolder);
        row->addWidget(addBtn_);
        row->addWidget(dropLabel_, 1);
        vbox->addLayout(row);
    }

    // ── Progress ──────────────────────────────────────────────────────────────
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->hide();
    stageLabel_  = new QLabel(this);
    stageLabel_->hide();
    vbox->addWidget(progressBar_);
    vbox->addWidget(stageLabel_);

    // ── Report ────────────────────────────────────────────────────────────────
    {
        auto* grp  = new QGroupBox("Last ingest result", this);
        auto* grid = new QHBoxLayout(grp);
        addedLbl_   = new QLabel("Added: —",   grp);
        skippedLbl_ = new QLabel("Skipped: —", grp);
        failedLbl_  = new QLabel("Failed: —",  grp);
        grid->addWidget(addedLbl_);
        grid->addWidget(skippedLbl_);
        grid->addWidget(failedLbl_);
        grid->addStretch();
        vbox->addWidget(grp);
    }

    errorList_ = new QListWidget(this);
    errorList_->hide();
    vbox->addWidget(errorList_, 1);
    vbox->addStretch();

    worker_ = new IngestWorker(this);
    connect(worker_, &IngestWorker::progress, this, &IngestTab::onProgress);
    connect(worker_, &IngestWorker::finished, this, &IngestTab::onFinished);
}

void IngestTab::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void IngestTab::dropEvent(QDropEvent* e) {
    const auto urls = e->mimeData()->urls();
    if (urls.isEmpty()) return;
    startIngest(urls.first().toLocalFile().toStdString());
}

void IngestTab::onAddFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select folder to ingest");
    if (!dir.isEmpty()) startIngest(dir.toStdString());
}

void IngestTab::startIngest(const std::string& path) {
    if (worker_->isRunning()) return;
    errorList_->clear();
    errorList_->hide();
    setRunning(true);
    worker_->setup(path, ctx_.tracks, ctx_.metadata);
    worker_->start();
}

void IngestTab::onProgress(float fraction, const QString& stage) {
    progressBar_->setValue(static_cast<int>(fraction * 100.f));
    stageLabel_->setText(stage);
}

void IngestTab::onFinished(pb::IngestReport report) {
    setRunning(false);
    addedLbl_->setText(  QString("Added: %1").arg(report.added));
    skippedLbl_->setText(QString("Skipped: %1").arg(report.skipped));
    failedLbl_->setText( QString("Failed: %1").arg(report.failed));

    if (!report.errors.empty()) {
        errorList_->show();
        for (const auto& [p, msg] : report.errors)
            errorList_->addItem(QString::fromStdString(p + ": " + msg));
    }

    if (report.added > 0) emit libraryChanged();
}

void IngestTab::setRunning(bool running) {
    addBtn_->setEnabled(!running);
    progressBar_->setVisible(running);
    stageLabel_->setVisible(running);
    if (running) progressBar_->setValue(0);
}

} // namespace gui
```

- [ ] **Step 3: Build**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings.

- [ ] **Step 4: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/IngestTab.h gui/IngestTab.cpp
git commit -m "feat: implement IngestTab (drag-drop, directory picker, progress, report)"
```

---

### Task 5: `BrowseTab`

**Files:**
- Modify: `gui/BrowseTab.h` (replace stub)
- Modify: `gui/BrowseTab.cpp` (replace stub)

- [ ] **Step 1: Replace `gui/BrowseTab.h`**

```cpp
#pragma once

#include "preset_builder/domain/track.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;
class QTimer;
QT_END_NAMESPACE

namespace gui {
struct PresetBuilderCtx;
}

namespace gui {

class BrowseTab : public QWidget {
    Q_OBJECT
public:
    explicit BrowseTab(PresetBuilderCtx& ctx, QWidget* parent = nullptr);

public slots:
    void refresh();  // re-run current filter and repopulate table

signals:
    void libraryChanged();  // emitted after delete

private slots:
    void onFilterChanged();
    void onItemChanged(QTableWidgetItem* item);
    void onDeleteSelected();

private:
    void populateTable(const std::vector<pb::Track>& tracks);
    pb::TrackFilter currentFilter() const;

    PresetBuilderCtx& ctx_;

    QLineEdit*    titleEdit_   = nullptr;
    QLineEdit*    artistEdit_  = nullptr;
    QLineEdit*    genreEdit_   = nullptr;
    QTableWidget* table_       = nullptr;
    QLabel*       countLbl_    = nullptr;
    QTimer*       debounce_    = nullptr;

    bool          blocking_    = false;  // suppresses itemChanged during populate

    // Column indices
    static constexpr int kColTitle  = 0;
    static constexpr int kColArtist = 1;
    static constexpr int kColAlbum  = 2;
    static constexpr int kColGenre  = 3;
    static constexpr int kColYear   = 4;
    static constexpr int kColSource = 5;
    static constexpr int kColPath   = 6;
};

} // namespace gui
```

- [ ] **Step 2: Replace `gui/BrowseTab.cpp`**

```cpp
#include "BrowseTab.h"
#include "PresetBuilderCtx.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace gui {

BrowseTab::BrowseTab(PresetBuilderCtx& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setSpacing(6);

    // ── Search row ────────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        titleEdit_  = new QLineEdit(this); titleEdit_->setPlaceholderText("Title…");
        artistEdit_ = new QLineEdit(this); artistEdit_->setPlaceholderText("Artist…");
        genreEdit_  = new QLineEdit(this); genreEdit_->setPlaceholderText("Genre…");
        row->addWidget(new QLabel("Title:",  this));  row->addWidget(titleEdit_,  1);
        row->addWidget(new QLabel("Artist:", this));  row->addWidget(artistEdit_, 1);
        row->addWidget(new QLabel("Genre:",  this));  row->addWidget(genreEdit_,  1);
        vbox->addLayout(row);
    }

    // ── Table ─────────────────────────────────────────────────────────────────
    table_ = new QTableWidget(this);
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels(
        {"Title", "Artist", "Album", "Genre", "Year", "Source", "Path"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kColSource, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColYear,   QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setAlternatingRowColors(true);
    vbox->addWidget(table_, 1);

    // ── Bottom toolbar ────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        auto* delBtn = new QPushButton("Delete selected", this);
        countLbl_    = new QLabel("0 tracks", this);
        connect(delBtn, &QPushButton::clicked, this, &BrowseTab::onDeleteSelected);
        row->addWidget(delBtn);
        row->addStretch();
        row->addWidget(countLbl_);
        vbox->addLayout(row);
    }

    // ── Debounce timer ────────────────────────────────────────────────────────
    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(200);
    connect(debounce_, &QTimer::timeout, this, &BrowseTab::refresh);

    connect(titleEdit_,  &QLineEdit::textChanged, this, &BrowseTab::onFilterChanged);
    connect(artistEdit_, &QLineEdit::textChanged, this, &BrowseTab::onFilterChanged);
    connect(genreEdit_,  &QLineEdit::textChanged, this, &BrowseTab::onFilterChanged);

    connect(table_, &QTableWidget::itemChanged, this, &BrowseTab::onItemChanged);
}

void BrowseTab::onFilterChanged() { debounce_->start(); }

pb::TrackFilter BrowseTab::currentFilter() const {
    pb::TrackFilter f;
    if (!titleEdit_->text().isEmpty())  f.title  = titleEdit_->text().toStdString();
    if (!artistEdit_->text().isEmpty()) f.artist = artistEdit_->text().toStdString();
    if (!genreEdit_->text().isEmpty())  f.genre  = genreEdit_->text().toStdString();
    return f;
}

void BrowseTab::refresh() {
    populateTable(ctx_.tracks.search(currentFilter()));
}

void BrowseTab::populateTable(const std::vector<pb::Track>& tracks) {
    blocking_ = true;
    table_->setRowCount(0);

    for (const auto& t : tracks) {
        const int row = table_->rowCount();
        table_->insertRow(row);

        auto* titleItem = new QTableWidgetItem(
            t.metadata.title ? QString::fromStdString(*t.metadata.title) : QString());
        titleItem->setData(Qt::UserRole, QString::fromStdString(t.id.hash));
        table_->setItem(row, kColTitle,  titleItem);

        table_->setItem(row, kColArtist,
            new QTableWidgetItem(t.metadata.artist
                ? QString::fromStdString(*t.metadata.artist) : QString()));
        table_->setItem(row, kColAlbum,
            new QTableWidgetItem(t.metadata.album
                ? QString::fromStdString(*t.metadata.album) : QString()));
        table_->setItem(row, kColGenre,
            new QTableWidgetItem(t.metadata.genre
                ? QString::fromStdString(*t.metadata.genre) : QString()));
        table_->setItem(row, kColYear,
            new QTableWidgetItem(t.metadata.year
                ? QString::number(*t.metadata.year) : QString()));

        auto* srcItem = new QTableWidgetItem(
            t.metadata.source == pb::MetadataSource::acoustid ? "AcoustID" : "filename");
        srcItem->setFlags(srcItem->flags() & ~Qt::ItemIsEditable);
        table_->setItem(row, kColSource, srcItem);

        auto* pathItem = new QTableWidgetItem(QString::fromStdString(t.path));
        pathItem->setFlags(pathItem->flags() & ~Qt::ItemIsEditable);
        pathItem->setToolTip(pathItem->text());
        table_->setItem(row, kColPath, pathItem);
    }

    countLbl_->setText(QString("%1 track%2")
        .arg(table_->rowCount())
        .arg(table_->rowCount() == 1 ? "" : "s"));
    blocking_ = false;
}

void BrowseTab::onItemChanged(QTableWidgetItem* item) {
    if (blocking_) return;
    const int row = item->row();

    // Retrieve track hash stored in the Title cell's UserRole.
    const QString hash = table_->item(row, kColTitle)->data(Qt::UserRole).toString();
    auto existing = ctx_.tracks.find(pb::TrackId{hash.toStdString()});
    if (!existing) return;

    pb::Track t = *existing;
    auto textOrNull = [&](int col) -> std::optional<std::string> {
        const QString v = table_->item(row, col)->text().trimmed();
        if (v.isEmpty()) return std::nullopt;
        return v.toStdString();
    };

    t.metadata.title  = textOrNull(kColTitle);
    t.metadata.artist = textOrNull(kColArtist);
    t.metadata.album  = textOrNull(kColAlbum);
    t.metadata.genre  = textOrNull(kColGenre);

    const QString yearStr = table_->item(row, kColYear)->text().trimmed();
    if (yearStr.isEmpty()) t.metadata.year.reset();
    else { bool ok; int y = yearStr.toInt(&ok); if (ok) t.metadata.year = y; }

    ctx_.tracks.save(t);
}

void BrowseTab::onDeleteSelected() {
    const auto sel = table_->selectedItems();
    if (sel.isEmpty()) return;

    // Collect unique row indices.
    std::set<int> rows;
    for (auto* item : sel) rows.insert(item->row());

    if (QMessageBox::question(this, "Delete tracks",
            QString("Delete %1 track(s) from the library?").arg(rows.size()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    for (int r : rows) {
        const QString hash = table_->item(r, kColTitle)->data(Qt::UserRole).toString();
        ctx_.tracks.remove(pb::TrackId{hash.toStdString()});
    }

    refresh();
    emit libraryChanged();
}

} // namespace gui
```

- [ ] **Step 3: Build**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings.

- [ ] **Step 4: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/BrowseTab.h gui/BrowseTab.cpp
git commit -m "feat: implement BrowseTab (search, inline edit, delete)"
```

---

### Task 6: `CreatePresetTab` + `StatsWorker`

**Files:**
- Modify: `gui/CreatePresetTab.h` (replace stub)
- Modify: `gui/CreatePresetTab.cpp` (replace stub)

- [ ] **Step 1: Replace `gui/CreatePresetTab.h`**

```cpp
#pragma once

#include "preset_builder/domain/preset.hpp"
#include "preset_builder/domain/track.hpp"

#include <QThread>
#include <QWidget>
#include <set>
#include <string>
#include <vector>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QTableWidget;
class QTimer;
QT_END_NAMESPACE

namespace gui {
struct PresetBuilderCtx;
}

namespace gui {

class StatsWorker : public QThread {
    Q_OBJECT
public:
    explicit StatsWorker(QObject* parent = nullptr);
    void setup(std::vector<pb::Track> tracks);

signals:
    void finished(pb::PresetStats stats);

protected:
    void run() override;

private:
    std::vector<pb::Track> tracks_;
};

// ─────────────────────────────────────────────────────────────────────────────

class CreatePresetTab : public QWidget {
    Q_OBJECT
public:
    explicit CreatePresetTab(PresetBuilderCtx& ctx, QWidget* parent = nullptr);

public slots:
    void refreshTrackList();

signals:
    void presetExported();

private slots:
    void onFilterChanged();
    void onSelectionChanged();
    void onStatsReady(pb::PresetStats stats);
    void onExportDefault();
    void onSaveAs();

private:
    void populateTrackTable(const std::vector<pb::Track>& tracks);
    void updateExportButtons();
    void doExport(const std::string& outputPath);
    pb::TrackFilter currentFilter() const;
    std::vector<pb::Track> selectedTracks() const;

    PresetBuilderCtx& ctx_;

    // Identity
    QLineEdit* nameEdit_  = nullptr;
    QLineEdit* descEdit_  = nullptr;

    // Track search + table
    QLineEdit*    titleEdit_  = nullptr;
    QLineEdit*    artistEdit_ = nullptr;
    QLineEdit*    genreEdit_  = nullptr;
    QTableWidget* trackTable_ = nullptr;
    QTimer*       debounce_   = nullptr;

    // Stats panel labels
    QLabel* statsLabels_[7]{};  // per-band: "RMS / CorrMin / Transient"
    QLabel* overallRmsLbl_ = nullptr;
    QLabel* overallCorrLbl_= nullptr;

    // Export
    QLabel* exportStatusLbl_ = nullptr;
    class QPushButton* exportBtn_  = nullptr;
    class QPushButton* saveAsBtn_  = nullptr;

    // State
    std::set<std::string>       selectedHashes_;  // survives filter changes
    std::vector<pb::Track>      displayedTracks_; // current table contents

    StatsWorker* statsWorker_ = nullptr;
    QTimer*      statsDebounce_ = nullptr;

    static constexpr int kColCheck  = 0;
    static constexpr int kColTitle  = 1;
    static constexpr int kColArtist = 2;
    static constexpr int kColGenre  = 3;
};

} // namespace gui
```

- [ ] **Step 2: Replace `gui/CreatePresetTab.cpp`**

```cpp
#include "CreatePresetTab.h"
#include "PresetBuilderCtx.h"
#include "utils.h"

#include "preset_builder/services/export_service.hpp"
#include "preset_builder/services/stats_service.hpp"
#include "mastertweak/analysis.hpp"

#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace gui {

static constexpr const char* kBandNames[7] = {
    "Sub", "Lows", "Lo-Mid", "Mids", "Hi-Mid", "Highs", "Air"
};

// ─── StatsWorker ─────────────────────────────────────────────────────────────

StatsWorker::StatsWorker(QObject* parent) : QThread(parent) {}

void StatsWorker::setup(std::vector<pb::Track> tracks) {
    tracks_ = std::move(tracks);
}

void StatsWorker::run() {
    pb::StatsService svc;
    emit finished(svc.compute(tracks_));
}

// ─── CreatePresetTab ─────────────────────────────────────────────────────────

CreatePresetTab::CreatePresetTab(PresetBuilderCtx& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setSpacing(6);

    // ── Identity ──────────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        nameEdit_ = new QLineEdit(this); nameEdit_->setPlaceholderText("Preset name (required)");
        descEdit_ = new QLineEdit(this); descEdit_->setPlaceholderText("Description (optional)");
        row->addWidget(new QLabel("Name:", this));        row->addWidget(nameEdit_, 2);
        row->addWidget(new QLabel("Description:", this)); row->addWidget(descEdit_, 3);
        vbox->addLayout(row);
    }

    // ── Main splitter: track list (left) | stats (right) ─────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left: search + table + selection buttons
    {
        auto* left = new QWidget(splitter);
        auto* lv   = new QVBoxLayout(left);
        lv->setContentsMargins(0, 0, 0, 0);

        // Search
        auto* searchRow = new QHBoxLayout;
        titleEdit_  = new QLineEdit(left); titleEdit_->setPlaceholderText("Title…");
        artistEdit_ = new QLineEdit(left); artistEdit_->setPlaceholderText("Artist…");
        genreEdit_  = new QLineEdit(left); genreEdit_->setPlaceholderText("Genre…");
        searchRow->addWidget(titleEdit_);
        searchRow->addWidget(artistEdit_);
        searchRow->addWidget(genreEdit_);
        lv->addLayout(searchRow);

        // Select All / Deselect All
        auto* btnRow = new QHBoxLayout;
        auto* selAll   = new QPushButton("Select All",    left);
        auto* deselAll = new QPushButton("Deselect All",  left);
        btnRow->addWidget(selAll);
        btnRow->addWidget(deselAll);
        btnRow->addStretch();
        lv->addLayout(btnRow);

        // Track table
        trackTable_ = new QTableWidget(left);
        trackTable_->setColumnCount(4);
        trackTable_->setHorizontalHeaderLabels({"", "Title", "Artist", "Genre"});
        trackTable_->horizontalHeader()->setSectionResizeMode(kColCheck,  QHeaderView::ResizeToContents);
        trackTable_->horizontalHeader()->setSectionResizeMode(kColTitle,  QHeaderView::Stretch);
        trackTable_->horizontalHeader()->setSectionResizeMode(kColArtist, QHeaderView::Stretch);
        trackTable_->horizontalHeader()->setSectionResizeMode(kColGenre,  QHeaderView::Stretch);
        trackTable_->setSelectionMode(QAbstractItemView::NoSelection);
        trackTable_->setAlternatingRowColors(true);
        lv->addWidget(trackTable_, 1);

        connect(selAll, &QPushButton::clicked, this, [this] {
            for (const auto& t : displayedTracks_) selectedHashes_.insert(t.id.hash);
            populateTrackTable(displayedTracks_);
        });
        connect(deselAll, &QPushButton::clicked, this, [this] {
            selectedHashes_.clear();
            populateTrackTable(displayedTracks_);
        });

        splitter->addWidget(left);
    }

    // Right: stats panel
    {
        auto* right = new QGroupBox("Stats preview", splitter);
        auto* rv    = new QVBoxLayout(right);

        auto* form = new QFormLayout;
        for (int i = 0; i < 7; ++i) {
            statsLabels_[i] = new QLabel(QString::fromUtf8("\xe2\x80\x94"), right);
            form->addRow(kBandNames[i], statsLabels_[i]);
        }
        overallRmsLbl_  = new QLabel(QString::fromUtf8("\xe2\x80\x94"), right);
        overallCorrLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), right);
        form->addRow("Overall RMS",      overallRmsLbl_);
        form->addRow("Overall Corr Min", overallCorrLbl_);
        rv->addLayout(form);
        rv->addStretch();

        splitter->addWidget(right);
    }

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    vbox->addWidget(splitter, 1);

    // ── Export row ────────────────────────────────────────────────────────────
    {
        auto* row     = new QHBoxLayout;
        exportBtn_    = new QPushButton("Export to Presets folder", this);
        saveAsBtn_    = new QPushButton("Save As…", this);
        exportStatusLbl_ = new QLabel(this);
        exportBtn_->setEnabled(false);
        saveAsBtn_->setEnabled(false);
        connect(exportBtn_,  &QPushButton::clicked, this, &CreatePresetTab::onExportDefault);
        connect(saveAsBtn_,  &QPushButton::clicked, this, &CreatePresetTab::onSaveAs);
        row->addWidget(exportBtn_);
        row->addWidget(saveAsBtn_);
        row->addWidget(exportStatusLbl_, 1);
        vbox->addLayout(row);
    }

    // ── Debounce timers ───────────────────────────────────────────────────────
    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(200);
    connect(debounce_, &QTimer::timeout, this, &CreatePresetTab::refreshTrackList);

    statsDebounce_ = new QTimer(this);
    statsDebounce_->setSingleShot(true);
    statsDebounce_->setInterval(300);
    connect(statsDebounce_, &QTimer::timeout, this, &CreatePresetTab::onSelectionChanged);

    connect(titleEdit_,  &QLineEdit::textChanged, this, [this]{ debounce_->start(); });
    connect(artistEdit_, &QLineEdit::textChanged, this, [this]{ debounce_->start(); });
    connect(genreEdit_,  &QLineEdit::textChanged, this, [this]{ debounce_->start(); });
    connect(nameEdit_,   &QLineEdit::textChanged, this, &CreatePresetTab::updateExportButtons);

    connect(trackTable_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item->column() != kColCheck) return;
        const std::string h = item->data(Qt::UserRole).toString().toStdString();
        if (item->checkState() == Qt::Checked) selectedHashes_.insert(h);
        else                                   selectedHashes_.erase(h);
        statsDebounce_->start();
        updateExportButtons();
    });

    statsWorker_ = new StatsWorker(this);
    connect(statsWorker_, &StatsWorker::finished, this, &CreatePresetTab::onStatsReady);
}

void CreatePresetTab::refreshTrackList() {
    displayedTracks_ = ctx_.tracks.search(currentFilter());
    populateTrackTable(displayedTracks_);
}

pb::TrackFilter CreatePresetTab::currentFilter() const {
    pb::TrackFilter f;
    if (!titleEdit_->text().isEmpty())  f.title  = titleEdit_->text().toStdString();
    if (!artistEdit_->text().isEmpty()) f.artist = artistEdit_->text().toStdString();
    if (!genreEdit_->text().isEmpty())  f.genre  = genreEdit_->text().toStdString();
    return f;
}

void CreatePresetTab::populateTrackTable(const std::vector<pb::Track>& tracks) {
    trackTable_->blockSignals(true);
    trackTable_->setRowCount(0);

    for (const auto& t : tracks) {
        const int row = trackTable_->rowCount();
        trackTable_->insertRow(row);

        auto* chk = new QTableWidgetItem;
        chk->setData(Qt::UserRole, QString::fromStdString(t.id.hash));
        chk->setCheckState(selectedHashes_.count(t.id.hash) ? Qt::Checked : Qt::Unchecked);
        chk->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        trackTable_->setItem(row, kColCheck, chk);

        auto* titleItm = new QTableWidgetItem(
            t.metadata.title ? QString::fromStdString(*t.metadata.title) : QString());
        titleItm->setFlags(titleItm->flags() & ~Qt::ItemIsEditable);
        trackTable_->setItem(row, kColTitle, titleItm);

        auto* artistItm = new QTableWidgetItem(
            t.metadata.artist ? QString::fromStdString(*t.metadata.artist) : QString());
        artistItm->setFlags(artistItm->flags() & ~Qt::ItemIsEditable);
        trackTable_->setItem(row, kColArtist, artistItm);

        auto* genreItm = new QTableWidgetItem(
            t.metadata.genre ? QString::fromStdString(*t.metadata.genre) : QString());
        genreItm->setFlags(genreItm->flags() & ~Qt::ItemIsEditable);
        trackTable_->setItem(row, kColGenre, genreItm);
    }

    trackTable_->blockSignals(false);
}

void CreatePresetTab::onSelectionChanged() {
    const auto sel = selectedTracks();
    if (sel.empty()) {
        const QString dash = QString::fromUtf8("\xe2\x80\x94");
        for (int i = 0; i < 7; ++i) statsLabels_[i]->setText(dash);
        overallRmsLbl_->setText(dash);
        overallCorrLbl_->setText(dash);
        return;
    }
    if (statsWorker_->isRunning()) return;
    statsWorker_->setup(sel);
    statsWorker_->start();
}

void CreatePresetTab::onStatsReady(pb::PresetStats stats) {
    for (int i = 0; i < 7; ++i) {
        const auto si = static_cast<size_t>(i);
        statsLabels_[i]->setText(
            QString("%1 / %2 / %3 dB")
                .arg(static_cast<double>(stats.bandRmsDb[si]),       0, 'f', 1)
                .arg(static_cast<double>(stats.bandCorrMin[si]),     0, 'f', 2)
                .arg(static_cast<double>(stats.bandTransientDb[si]), 0, 'f', 1));
    }
    overallRmsLbl_->setText( QString("%1 dBFS").arg(static_cast<double>(stats.overallRmsDb),   0, 'f', 1));
    overallCorrLbl_->setText(QString("%1")     .arg(static_cast<double>(stats.overallCorrMin), 0, 'f', 2));
}

std::vector<pb::Track> CreatePresetTab::selectedTracks() const {
    std::vector<pb::Track> result;
    for (const auto& t : displayedTracks_) {
        if (selectedHashes_.count(t.id.hash)) result.push_back(t);
    }
    // Also include selected tracks not in the current filter
    for (const auto& h : selectedHashes_) {
        bool found = false;
        for (const auto& t : result) if (t.id.hash == h) { found = true; break; }
        if (!found) {
            auto t = ctx_.tracks.find(pb::TrackId{h});
            if (t) result.push_back(*t);
        }
    }
    return result;
}

void CreatePresetTab::updateExportButtons() {
    const bool canExport = !nameEdit_->text().trimmed().isEmpty()
                        && !selectedHashes_.empty();
    exportBtn_->setEnabled(canExport);
    saveAsBtn_->setEnabled(canExport);
}

void CreatePresetTab::doExport(const std::string& outputPath) {
    pb::PresetStats stats;
    {
        pb::StatsService svc;
        stats = svc.compute(selectedTracks());
    }

    pb::Preset preset;
    preset.id          = pb::PresetId{QUuid::createUuid().toString().toStdString()};
    preset.name        = nameEdit_->text().trimmed().toStdString();
    preset.description = descEdit_->text().trimmed().toStdString();
    preset.createdAt   = ""; // informational only
    for (const auto& h : selectedHashes_)
        preset.trackIds.push_back(pb::TrackId{h});

    try {
        pb::ExportService exp;
        exp.exportXml(preset, stats, outputPath);
        ctx_.presets.save(preset);
        exportStatusLbl_->setText(
            QString("Exported \xe2\x86\x92 %1").arg(QString::fromStdString(outputPath)));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Export failed", e.what());
    }
}

void CreatePresetTab::onExportDefault() {
    const QString dir = QDir::homePath() + "/.config/MixAdvice/Presets";
    QDir().mkpath(dir);
    const std::string stem = gui::sanitizePresetName(nameEdit_->text().toStdString());
    const std::string path = (dir + "/" + QString::fromStdString(stem) + ".xml").toStdString();
    doExport(path);
    emit presetExported();
}

void CreatePresetTab::onSaveAs() {
    const QString defaultName = QString::fromStdString(
        gui::sanitizePresetName(nameEdit_->text().toStdString())) + ".xml";
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Preset XML", QDir::homePath() + "/" + defaultName,
        "XML Presets (*.xml);;All Files (*)");
    if (path.isEmpty()) return;
    doExport(path.toStdString());
}

} // namespace gui
```

- [ ] **Step 3: Add missing `#include <QUuid>` to `CreatePresetTab.cpp`**

Add at the top of the includes:
```cpp
#include <QUuid>
```

- [ ] **Step 4: Build**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings.

- [ ] **Step 5: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/CreatePresetTab.h gui/CreatePresetTab.cpp
git commit -m "feat: implement CreatePresetTab (track selection, stats preview, export)"
```

---

### Task 7: `PresetSelector` "Manage presets…" + `MainWindow` wiring

**Files:**
- Modify: `gui/PresetSelector.h`
- Modify: `gui/PresetSelector.cpp`
- Modify: `gui/MainWindow.h`
- Modify: `gui/MainWindow.cpp`

- [ ] **Step 1: Update `gui/PresetSelector.h`** — add `manageBtn_` and `manageRequested()` signal

In `gui/PresetSelector.h`, add `manageRequested()` to `signals:`:
```cpp
signals:
    void presetChanged(const mt::PresetData& preset);
    void manageRequested();
```

Add `manageBtn_` to the private members:
```cpp
    QComboBox*   combo_     = nullptr;
    QPushButton* openBtn_   = nullptr;
    QPushButton* manageBtn_ = nullptr;
```

- [ ] **Step 2: Update `gui/PresetSelector.cpp`** — add button and wire signal

In the constructor, after `openBtn_->setFixedWidth(70);` add:

```cpp
    manageBtn_ = new QPushButton("Manage presets\xe2\x80\xa6", this);
    manageBtn_->setFixedWidth(130);
```

Add to the layout (after `openBtn_`):
```cpp
    layout->addWidget(manageBtn_);
```

Add the connection (after the other `connect` calls):
```cpp
    connect(manageBtn_, &QPushButton::clicked, this, &PresetSelector::manageRequested);
```

- [ ] **Step 3: Update `gui/MainWindow.h`** — add `PresetBuilderDialog*` member and forward declaration

Add forward declaration before the class:
```cpp
namespace gui { class PresetBuilderDialog; }
```

Add private member:
```cpp
    PresetBuilderDialog*          presetBuilderDialog_ = nullptr;
```

Add private slot:
```cpp
    void onManagePresets();
```

- [ ] **Step 4: Update `gui/MainWindow.cpp`** — lazy-construct dialog and wire signals

Add include at the top of the file (with other local includes):
```cpp
#include "PresetBuilderDialog.h"
```

In `buildUi()`, inside the preset row block after the `connect(resetBtn_, ...)` call, add:

```cpp
        connect(presetSelector_, &PresetSelector::manageRequested,
                this, &MainWindow::onManagePresets);
```

Add the slot implementation (near the end of the file, before or after `makeDefaultOutputPath`):

```cpp
void MainWindow::onManagePresets() {
    if (!presetBuilderDialog_) {
        const std::string execDir = fs::path{
            QApplication::applicationFilePath().toStdString()
        }.parent_path().string();

        presetBuilderDialog_ = new PresetBuilderDialog(execDir, this);
        connect(presetBuilderDialog_, &PresetBuilderDialog::presetExported,
                this, [this] {
                    const std::string execDir = fs::path{
                        QApplication::applicationFilePath().toStdString()
                    }.parent_path().string();
                    presetSelector_->populate(execDir);
                });
    }
    presetBuilderDialog_->show();
    presetBuilderDialog_->raise();
    presetBuilderDialog_->activateWindow();
}
```

- [ ] **Step 5: Build**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
```

Expected: clean build, zero warnings.

- [ ] **Step 6: Run all tests**

```bash
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add gui/PresetSelector.h gui/PresetSelector.cpp \
        gui/MainWindow.h gui/MainWindow.cpp
git commit -m "feat: wire PresetBuilderDialog to MainWindow via PresetSelector button"
```

---

### Task 8: Final verification + TODO update

**Files:**
- Modify: `TODO.md`

- [ ] **Step 1: Full build and test suite**

```bash
cmake --build /home/yvan/Projects/AudioPlugins/MasterTweak/build --parallel 2>&1
ctest --test-dir /home/yvan/Projects/AudioPlugins/MasterTweak/build --output-on-failure 2>&1
```

Expected: clean build, zero warnings, all tests pass.

- [ ] **Step 2: Update `TODO.md`**

Find:
```
  - Sub-project B: Qt6 Preset Builder dialog (Ingest / Browse+Tag / Create Preset screens) — spec + plan pending.
```

Replace with:
```
  - ~~Sub-project B: Qt6 Preset Builder dialog (Ingest / Browse+Tag / Create Preset screens)~~ **DONE**
```

- [ ] **Step 3: Commit**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
git add TODO.md
git commit -m "docs: mark Preset Builder dialog (Sub-project B) as done in TODO"
```
