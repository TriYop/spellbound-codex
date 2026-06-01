# Manage Presets Tab — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a 4th "Manage Presets" tab to `PresetBuilderDialog` that lists all saved presets, shows each preset's tracks with per-band spectral deviation colouring, and allows removing individual tracks or deleting entire presets.

**Architecture:** New `ManagePresetsTab` widget (pure Qt6 UI glue, no new domain logic). It reads from `ctx_.presets` (list/find/tracksFor/remove/save), recomputes stats via `StatsService`, and re-exports XML via `ExportService` on every track removal. Wired into `PresetBuilderDialog` as the 4th tab and connected to refresh signals from `CreatePresetTab` and `AutoDiscoverDialog`.

**Tech Stack:** Qt6, C++20, `preset_builder_core` (StatsService, ExportService, SqlitePresetRepository).

---

## File Map

| Action | Path | Responsibility |
|--------|------|----------------|
| Create | `gui/ManagePresetsTab.h` | Tab class declaration |
| Create | `gui/ManagePresetsTab.cpp` | Tab implementation |
| Modify | `gui/PresetBuilderDialog.h` | Add `ManagePresetsTab*` member |
| Modify | `gui/PresetBuilderDialog.cpp` | Add 4th tab, wire refresh signals |
| Modify | `gui/CMakeLists.txt` | Add new source files |

No new tests — the tab is UI glue over already-tested services.

---

### Task 1: `ManagePresetsTab` — header

**Files:**
- Create: `gui/ManagePresetsTab.h`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "preset_builder/domain/preset.hpp"
#include "preset_builder/domain/track.hpp"

#include <QWidget>
#include <string>
#include <vector>

QT_BEGIN_NAMESPACE
class QColor;
class QListWidget;
class QPushButton;
class QShowEvent;
class QTableWidget;
QT_END_NAMESPACE

namespace gui {
struct PresetBuilderCtx;
}

namespace gui {

class ManagePresetsTab : public QWidget {
    Q_OBJECT
public:
    explicit ManagePresetsTab(PresetBuilderCtx& ctx, QWidget* parent = nullptr);

public slots:
    void refresh();

signals:
    void presetExported();

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onPresetSelected(int row);
    void onRemoveTrack(const std::string& hash);
    void onDeletePreset();

private:
    void populateTrackTable(const pb::Preset& preset,
                            const pb::PresetStats& stats,
                            const std::vector<pb::Track>& tracks);
    void updateCurrentListItem();
    QColor deviationColor(float absDeviationDb) const;

    PresetBuilderCtx& ctx_;

    QListWidget*  presetList_ = nullptr;
    QPushButton*  deleteBtn_  = nullptr;
    QTableWidget* trackTable_ = nullptr;

    pb::Preset currentPreset_;   // in-memory working copy for track removal

    static constexpr int kColTitle  = 0;
    static constexpr int kColArtist = 1;
    static constexpr int kColGenre  = 2;
    static constexpr int kColSub    = 3;   // bands occupy columns 3–9
    static constexpr int kColRemove = 10;
};

} // namespace gui
```

- [ ] **Step 2: Verify it compiles (no .cpp yet — will fail to link, that's fine)**

```bash
cmake --build build --parallel 2>&1 | grep -i "ManagePresetsTab\|error" | head -20
```

---

### Task 2: `ManagePresetsTab` — implementation

**Files:**
- Create: `gui/ManagePresetsTab.cpp`

- [ ] **Step 1: Create the implementation**

```cpp
#include "ManagePresetsTab.h"
#include "PresetBuilderCtx.h"
#include "utils.h"

#include "preset_builder/services/export_service.hpp"
#include "preset_builder/services/stats_service.hpp"

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace gui {

static constexpr const char* kBandNames[7] = {
    "Sub", "Lows", "Lo-Mid", "Mids", "Hi-Mid", "Highs", "Air"
};

ManagePresetsTab::ManagePresetsTab(PresetBuilderCtx& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx)
{
    auto* hbox    = new QHBoxLayout(this);
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // ── Left: preset list ────────────────────────────────────────────────────
    auto* leftWidget = new QWidget(splitter);
    auto* lv         = new QVBoxLayout(leftWidget);
    lv->setContentsMargins(0, 0, 0, 0);

    presetList_ = new QListWidget(leftWidget);
    lv->addWidget(presetList_, 1);

    deleteBtn_ = new QPushButton("Delete Preset", leftWidget);
    deleteBtn_->setEnabled(false);
    lv->addWidget(deleteBtn_);

    splitter->addWidget(leftWidget);

    // ── Right: track table ───────────────────────────────────────────────────
    trackTable_ = new QTableWidget(splitter);
    trackTable_->setColumnCount(11);

    QStringList headers;
    headers << "Title" << "Artist" << "Genre";
    for (const char* b : kBandNames) headers << b;
    headers << "";
    trackTable_->setHorizontalHeaderLabels(headers);

    trackTable_->horizontalHeader()->setSectionResizeMode(kColTitle,  QHeaderView::Stretch);
    trackTable_->horizontalHeader()->setSectionResizeMode(kColArtist, QHeaderView::Stretch);
    trackTable_->horizontalHeader()->setSectionResizeMode(kColGenre,  QHeaderView::ResizeToContents);
    for (int b = 0; b < 7; ++b)
        trackTable_->horizontalHeader()->setSectionResizeMode(
            kColSub + b, QHeaderView::ResizeToContents);
    trackTable_->horizontalHeader()->setSectionResizeMode(kColRemove, QHeaderView::ResizeToContents);

    trackTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trackTable_->setSelectionMode(QAbstractItemView::NoSelection);
    trackTable_->setAlternatingRowColors(true);
    splitter->addWidget(trackTable_);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    hbox->addWidget(splitter);

    connect(presetList_, &QListWidget::currentRowChanged,
            this,        &ManagePresetsTab::onPresetSelected);
    connect(deleteBtn_,  &QPushButton::clicked,
            this,        &ManagePresetsTab::onDeletePreset);
}

void ManagePresetsTab::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh();
}

void ManagePresetsTab::refresh() {
    const int prevRow = presetList_->currentRow();
    presetList_->blockSignals(true);
    presetList_->clear();

    const auto presets = ctx_.presets.listAll();
    for (const auto& p : presets) {
        const auto tracks = ctx_.presets.tracksFor(p.id);
        auto* item = new QListWidgetItem(
            QString::fromStdString(p.name) +
            QString("  (%1 tracks)").arg(static_cast<int>(tracks.size())));
        item->setData(Qt::UserRole, QString::fromStdString(p.id.uuid));
        presetList_->addItem(item);
    }
    presetList_->blockSignals(false);

    if (!presets.empty()) {
        const int row = std::min(prevRow, static_cast<int>(presets.size()) - 1);
        presetList_->setCurrentRow(std::max(0, row));
    } else {
        trackTable_->setRowCount(0);
        deleteBtn_->setEnabled(false);
    }
}

void ManagePresetsTab::onPresetSelected(int row) {
    if (row < 0) {
        trackTable_->setRowCount(0);
        deleteBtn_->setEnabled(false);
        return;
    }
    auto* item = presetList_->item(row);
    if (!item) return;

    const std::string uuid = item->data(Qt::UserRole).toString().toStdString();
    const auto opt = ctx_.presets.find(pb::PresetId{uuid});
    if (!opt) return;

    currentPreset_ = *opt;
    deleteBtn_->setEnabled(true);

    const auto tracks = ctx_.presets.tracksFor(currentPreset_.id);
    pb::StatsService svc;
    const auto stats = svc.compute(tracks);
    populateTrackTable(currentPreset_, stats, tracks);
}

void ManagePresetsTab::populateTrackTable(const pb::Preset& preset,
                                          const pb::PresetStats& stats,
                                          const std::vector<pb::Track>& tracks)
{
    (void)preset;
    trackTable_->blockSignals(true);
    trackTable_->setRowCount(0);

    const float presetOverallRms = stats.overallRmsDb;

    auto mkCell = [](const QString& text) {
        auto* item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    };

    for (int r = 0; r < static_cast<int>(tracks.size()); ++r) {
        const auto& t = tracks[static_cast<size_t>(r)];
        trackTable_->insertRow(r);

        auto* titleItem = mkCell(
            t.metadata.title ? QString::fromStdString(*t.metadata.title) : QString());
        titleItem->setData(Qt::UserRole, QString::fromStdString(t.id.hash));
        trackTable_->setItem(r, kColTitle, titleItem);

        trackTable_->setItem(r, kColArtist, mkCell(
            t.metadata.artist ? QString::fromStdString(*t.metadata.artist) : QString()));
        trackTable_->setItem(r, kColGenre, mkCell(
            t.metadata.genre  ? QString::fromStdString(*t.metadata.genre)  : QString()));

        // Per-band spectral shape deviation from preset mean.
        const float trackOverall = t.analysis.overallRmsDb;
        for (int b = 0; b < 7; ++b) {
            const auto bi = static_cast<size_t>(b);
            const float trackShape  = t.analysis.bandRmsDb[bi] - trackOverall;
            const float presetShape = stats.bandRmsDb[bi] - presetOverallRms;
            const float deviation   = trackShape - presetShape;

            auto* cell = mkCell(QString("%1").arg(
                static_cast<double>(deviation), 0, 'f', 1));
            cell->setBackground(deviationColor(std::abs(deviation)));
            cell->setTextAlignment(Qt::AlignCenter);
            trackTable_->setItem(r, kColSub + b, cell);
        }

        // Remove button — capture hash so row index shifts don't matter.
        auto* btn = new QPushButton("\xc3\x97", trackTable_);  // ×
        btn->setFixedSize(24, 24);
        const std::string hash = t.id.hash;
        connect(btn, &QPushButton::clicked, this, [this, hash]() {
            onRemoveTrack(hash);
        });
        trackTable_->setCellWidget(r, kColRemove, btn);
    }

    trackTable_->blockSignals(false);
}

void ManagePresetsTab::onRemoveTrack(const std::string& hash) {
    auto& ids = currentPreset_.trackIds;
    ids.erase(std::remove_if(ids.begin(), ids.end(),
                             [&hash](const pb::TrackId& id) { return id.hash == hash; }),
              ids.end());

    ctx_.presets.save(currentPreset_);

    const auto tracks = ctx_.presets.tracksFor(currentPreset_.id);
    pb::StatsService svc;
    const auto stats = svc.compute(tracks);

    const QString dir = QDir::homePath() + "/.config/MixAdvice/Presets";
    QDir().mkpath(dir);
    const std::string path =
        (dir + "/" +
         QString::fromStdString(gui::sanitizePresetName(currentPreset_.name)) +
         ".xml").toStdString();
    try {
        pb::ExportService exp;
        exp.exportXml(currentPreset_, stats, path);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Re-export failed", e.what());
    }

    populateTrackTable(currentPreset_, stats, tracks);
    updateCurrentListItem();
    emit presetExported();
}

void ManagePresetsTab::updateCurrentListItem() {
    const int row = presetList_->currentRow();
    if (row < 0) return;
    auto* item = presetList_->item(row);
    if (!item) return;
    item->setText(
        QString::fromStdString(currentPreset_.name) +
        QString("  (%1 tracks)").arg(
            static_cast<int>(currentPreset_.trackIds.size())));
}

void ManagePresetsTab::onDeletePreset() {
    const int row = presetList_->currentRow();
    if (row < 0) return;

    const auto reply = QMessageBox::question(
        this, "Delete Preset",
        QString("Delete preset \"%1\"? This cannot be undone.")
            .arg(QString::fromStdString(currentPreset_.name)));
    if (reply != QMessageBox::Yes) return;

    ctx_.presets.remove(currentPreset_.id);

    delete presetList_->takeItem(row);
    trackTable_->setRowCount(0);
    deleteBtn_->setEnabled(false);
    emit presetExported();
}

QColor ManagePresetsTab::deviationColor(float absDeviationDb) const {
    if (absDeviationDb < 2.f) return QColor(100, 200, 100);   // green
    if (absDeviationDb < 5.f) return QColor(220, 200,  80);   // yellow
    return                           QColor(220,  80,  80);   // red
}

} // namespace gui
```

- [ ] **Step 2: Build — expect link success (no wiring yet)**

```bash
cmake --build build --parallel 2>&1 | tail -5
```

Expected: `[N/N] Linking CXX executable gui/MasterTweak` (may fail if CMakeLists not updated yet — that's handled in Task 3).

---

### Task 3: Wire into `PresetBuilderDialog` and `CMakeLists`

**Files:**
- Modify: `gui/CMakeLists.txt`
- Modify: `gui/PresetBuilderDialog.h`
- Modify: `gui/PresetBuilderDialog.cpp`

- [ ] **Step 1: Add sources to `gui/CMakeLists.txt`**

In the `qt_add_executable(MasterTweak ...)` sources list, add after `CreatePresetTab.cpp`:
```cmake
    ManagePresetsTab.h
    ManagePresetsTab.cpp
```

- [ ] **Step 2: Add member to `gui/PresetBuilderDialog.h`**

Add forward declaration before the class:
```cpp
class ManagePresetsTab;
```

Add member after `createTab_`:
```cpp
    ManagePresetsTab* manageTab_  = nullptr;
```

- [ ] **Step 3: Wire tab in `gui/PresetBuilderDialog.cpp`**

Add include at top:
```cpp
#include "ManagePresetsTab.h"
```

In `buildUi()`, after `createTab_  = new CreatePresetTab(*ctx_, this);`:
```cpp
    manageTab_ = new ManagePresetsTab(*ctx_, this);
```

After `tabs_->addTab(createTab_, "Create Preset");`:
```cpp
    tabs_->addTab(manageTab_, "Manage Presets");
```

After the existing `connect(createTab_, &CreatePresetTab::presetExported, ...)`:
```cpp
    connect(createTab_,  &CreatePresetTab::presetExported,
            manageTab_,  &ManagePresetsTab::refresh);
    // AutoDiscoverDialog is created on demand; wire its signal via a lambda
    // on the dialog object just before exec() in CreatePresetTab::onAutoDiscover().
    // That existing connection already triggers createTab_::presetExported which
    // chains to manageTab_::refresh — no additional wiring needed here.
```

In `onIngesting(bool active)`, add line to also disable/enable the manage tab:
```cpp
    tabs_->setTabEnabled(3, !active);  // Manage Presets
```

In `showEvent()`, add:
```cpp
    if (manageTab_) manageTab_->refresh();
```

- [ ] **Step 4: Build**

```bash
cmake --build build --parallel 2>&1 | tail -10
```

Expected: `[N/N] Linking CXX executable gui/MasterTweak` with no errors.

- [ ] **Step 5: Run all tests**

```bash
ctest --test-dir build --output-on-failure
```

Expected: `100% tests passed`.

- [ ] **Step 6: Commit**

```bash
git add gui/ManagePresetsTab.h gui/ManagePresetsTab.cpp \
        gui/PresetBuilderDialog.h gui/PresetBuilderDialog.cpp \
        gui/CMakeLists.txt
git commit -m "feat: Manage Presets tab with per-band deviation colouring and track removal"
```

---

## Verification

```bash
./build/gui/MasterTweak
```

Checklist:
1. Preset Builder dialog shows 4 tabs: Ingest / Browse & Tag / Create Preset / **Manage Presets**
2. Manage Presets tab lists all saved presets with track counts
3. Selecting a preset populates the track table with 7 coloured band columns
4. Band cells show signed dB values; close tracks are green, outliers yellow/red
5. Clicking [✕] on a track removes it, updates the count in the left list, re-exports XML, no crash
6. "Delete Preset" shows a confirmation dialog; confirming removes it from the list and clears the table
7. Creating a new preset in Create Preset tab → switching to Manage Presets → new preset appears
8. Ingesting tracks disables the Manage Presets tab (tab index 3)
