#include "ManagePresetsTab.h"
#include "PresetBuilderCtx.h"
#include "utils.h"

#include "preset_builder/services/export_service.hpp"
#include "preset_builder/services/stats_service.hpp"

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

#include <algorithm>
#include <cmath>

namespace gui {

static constexpr const char* kBandNames[7] = {
    "Sub", "Lows", "Lo-Mid", "Mids", "Hi-Mid", "Highs", "Air"
};

// ─── ManagePresetsTab ─────────────────────────────────────────────────────────

ManagePresetsTab::ManagePresetsTab(PresetBuilderCtx& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx)
{
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // ── Left pane: preset list + delete button ────────────────────────────────
    {
        auto* left = new QWidget(splitter);
        auto* lv   = new QVBoxLayout(left);
        lv->setContentsMargins(0, 0, 0, 0);

        presetList_ = new QListWidget(left);
        deleteBtn_  = new QPushButton("Delete Preset", left);
        deleteBtn_->setEnabled(false);

        lv->addWidget(presetList_, 1);
        lv->addWidget(deleteBtn_);

        splitter->addWidget(left);
    }

    // ── Right pane: track table ───────────────────────────────────────────────
    {
        trackTable_ = new QTableWidget(splitter);
        trackTable_->setColumnCount(11);

        QStringList headers;
        headers << "Title" << "Artist" << "Genre";
        for (const char* name : kBandNames)
            headers << name;
        headers << "\xc3\x97";  // ×
        trackTable_->setHorizontalHeaderLabels(headers);

        auto* hdr = trackTable_->horizontalHeader();
        hdr->setSectionResizeMode(kColTitle,  QHeaderView::Stretch);
        hdr->setSectionResizeMode(kColArtist, QHeaderView::Stretch);
        hdr->setSectionResizeMode(kColGenre,  QHeaderView::ResizeToContents);
        // 7 band columns (3–9)
        for (int c = kColSub; c < kColRemove; ++c)
            hdr->setSectionResizeMode(c, QHeaderView::ResizeToContents);
        hdr->setSectionResizeMode(kColRemove, QHeaderView::ResizeToContents);

        trackTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        trackTable_->setSelectionMode(QAbstractItemView::NoSelection);
        trackTable_->setAlternatingRowColors(true);

        splitter->addWidget(trackTable_);
    }

    // 1/3 left, 2/3 right
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    auto* topLayout = new QVBoxLayout(this);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->addWidget(splitter);

    connect(presetList_, &QListWidget::currentRowChanged,
            this, &ManagePresetsTab::onPresetSelected);
    connect(deleteBtn_, &QPushButton::clicked,
            this, &ManagePresetsTab::onDeletePreset);
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
        const int  n      = static_cast<int>(tracks.size());
        auto* item = new QListWidgetItem(
            QString::fromStdString(p.name)
            + QString("  (%1 track%2)").arg(n).arg(n == 1 ? "" : "s"));
        item->setData(Qt::UserRole, QString::fromStdString(p.id.uuid));
        presetList_->addItem(item);
    }

    presetList_->blockSignals(false);

    if (!presets.empty()) {
        const int row = std::max(0, std::min(prevRow, static_cast<int>(presets.size()) - 1));
        presetList_->setCurrentRow(row);
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
    auto found = ctx_.presets.find(pb::PresetId{uuid});
    if (!found) return;

    currentPreset_ = *found;
    deleteBtn_->setEnabled(true);

    const auto tracks = ctx_.presets.tracksFor(currentPreset_.id);
    pb::StatsService svc;
    const auto stats  = svc.compute(tracks);
    populateTrackTable(currentPreset_, stats, tracks);
}

void ManagePresetsTab::populateTrackTable(const pb::Preset& /*preset*/,
                                          const pb::PresetStats& stats,
                                          const std::vector<pb::Track>& tracks)
{
    trackTable_->blockSignals(true);
    trackTable_->setRowCount(0);

    const float presetOverallRms = stats.overallRmsDb;

    for (const auto& t : tracks) {
        const int row = trackTable_->rowCount();
        trackTable_->insertRow(row);

        // Title — store hash in UserRole
        auto* titleItem = new QTableWidgetItem(
            t.metadata.title ? QString::fromStdString(*t.metadata.title) : QString());
        titleItem->setData(Qt::UserRole, QString::fromStdString(t.id.hash));
        titleItem->setFlags(titleItem->flags() & ~Qt::ItemIsEditable);
        trackTable_->setItem(row, kColTitle, titleItem);

        // Artist
        auto* artistItem = new QTableWidgetItem(
            t.metadata.artist ? QString::fromStdString(*t.metadata.artist) : QString());
        artistItem->setFlags(artistItem->flags() & ~Qt::ItemIsEditable);
        trackTable_->setItem(row, kColArtist, artistItem);

        // Genre
        auto* genreItem = new QTableWidgetItem(
            t.metadata.genre ? QString::fromStdString(*t.metadata.genre) : QString());
        genreItem->setFlags(genreItem->flags() & ~Qt::ItemIsEditable);
        trackTable_->setItem(row, kColGenre, genreItem);

        // 7 band deviation cells (cols 3–9)
        for (int b = 0; b < 7; ++b) {
            const auto bi = static_cast<size_t>(b);
            const float trackRelative  = t.analysis.bandRmsDb[bi] - t.analysis.overallRmsDb;
            const float presetRelative = stats.bandRmsDb[bi]      - presetOverallRms;
            const float deviation      = trackRelative - presetRelative;

            const QString text = QString("%1%2")
                .arg(deviation >= 0.f ? "+" : "")
                .arg(static_cast<double>(deviation), 0, 'f', 1);

            auto* cell = new QTableWidgetItem(text);
            cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
            cell->setBackground(deviationColor(std::abs(deviation)));
            cell->setTextAlignment(Qt::AlignCenter);
            trackTable_->setItem(row, kColSub + b, cell);
        }

        // Remove button (col 10)
        auto* removeBtn = new QPushButton(QString::fromUtf8("\xc3\x97"), this);
        removeBtn->setFixedSize(24, 24);
        const std::string hash = t.id.hash;
        connect(removeBtn, &QPushButton::clicked, this,
                [this, hash] { onRemoveTrack(hash); });
        trackTable_->setCellWidget(row, kColRemove, removeBtn);
    }

    trackTable_->blockSignals(false);
}

void ManagePresetsTab::onRemoveTrack(const std::string& hash) {
    // Erase matching TrackId from currentPreset_.trackIds
    auto& ids = currentPreset_.trackIds;
    ids.erase(std::remove_if(ids.begin(), ids.end(),
                             [&hash](const pb::TrackId& tid) { return tid.hash == hash; }),
              ids.end());

    ctx_.presets.save(currentPreset_);

    // Refresh track table
    const auto tracks = ctx_.presets.tracksFor(currentPreset_.id);
    pb::StatsService svc;
    const auto stats  = svc.compute(tracks);
    populateTrackTable(currentPreset_, stats, tracks);

    // Re-export XML
    const QString dir = QDir::homePath() + "/.config/MixAdvice/Presets";
    QDir().mkpath(dir);
    const std::string stem     = gui::sanitizePresetName(currentPreset_.name);
    const std::string outPath  = (dir + "/" + QString::fromStdString(stem) + ".xml").toStdString();

    try {
        pb::StatsService svc2;
        const auto tracks2 = ctx_.presets.tracksFor(currentPreset_.id);
        const auto stats2  = svc2.compute(tracks2);
        pb::ExportService exp;
        exp.exportXml(currentPreset_, stats2, outPath);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Export failed", e.what());
    }

    updateCurrentListItem();
    emit presetExported();
}

void ManagePresetsTab::onDeletePreset() {
    if (presetList_->currentRow() < 0) return;

    const int ret = QMessageBox::question(
        this, "Delete Preset",
        QString("Delete preset \"%1\"? This cannot be undone.")
            .arg(QString::fromStdString(currentPreset_.name)),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    ctx_.presets.remove(currentPreset_.id);

    const int row = presetList_->currentRow();
    delete presetList_->takeItem(row);

    trackTable_->setRowCount(0);
    deleteBtn_->setEnabled(false);

    emit presetExported();
}

void ManagePresetsTab::updateCurrentListItem() {
    auto* item = presetList_->currentItem();
    if (!item) return;
    const int n = static_cast<int>(currentPreset_.trackIds.size());
    item->setText(
        QString::fromStdString(currentPreset_.name)
        + QString("  (%1 track%2)").arg(n).arg(n == 1 ? "" : "s"));
}

QColor ManagePresetsTab::deviationColor(float absDeviationDb) const {
    if (absDeviationDb < 2.f) return QColor(100, 200, 100);  // green
    if (absDeviationDb < 5.f) return QColor(220, 200,  80);  // yellow
    return                           QColor(220,  80,  80);  // red
}

} // namespace gui
