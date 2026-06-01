#include "AutoDiscoverDialog.h"
#include "PresetBuilderCtx.h"
#include "utils.h"

#include "preset_builder/services/export_service.hpp"
#include "preset_builder/services/stats_service.hpp"

#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

namespace gui {

// ─── DiscoverWorker ──────────────────────────────────────────────────────────

DiscoverWorker::DiscoverWorker(QObject* parent) : QThread(parent) {}

void DiscoverWorker::setup(std::vector<pb::Track> tracks, float threshold) {
    tracks_    = std::move(tracks);
    threshold_ = threshold;
}

void DiscoverWorker::run() {
    pb::SimilarityService svc;
    auto groups = svc.discover(tracks_, threshold_);
    emit finished(std::move(groups));
}

// ─── AutoDiscoverDialog ──────────────────────────────────────────────────────

AutoDiscoverDialog::AutoDiscoverDialog(PresetBuilderCtx& ctx, QWidget* parent)
    : QDialog(parent), ctx_(ctx)
{
    setWindowTitle("Auto-Discover Similar Tracks");
    resize(800, 560);

    auto* vbox = new QVBoxLayout(this);
    vbox->setSpacing(6);

    // ── Row 1: controls ───────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("Similarity threshold:", this));

        thresholdSpin_ = new QDoubleSpinBox(this);
        thresholdSpin_->setRange(0.0, 5.0);
        thresholdSpin_->setSingleStep(0.1);
        thresholdSpin_->setValue(1.5);
        thresholdSpin_->setDecimals(1);
        row->addWidget(thresholdSpin_);

        row->addWidget(new QLabel("(lower = tighter groups)", this));

        discoverBtn_ = new QPushButton("Discover Groups", this);
        row->addWidget(discoverBtn_);

        statusLbl_ = new QLabel(this);
        row->addWidget(statusLbl_, 1);

        vbox->addLayout(row);
    }

    // ── Splitter ──────────────────────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left: group list
    groupList_ = new QListWidget(splitter);
    splitter->addWidget(groupList_);

    // Right: track detail panel
    {
        auto* right = new QWidget(splitter);
        auto* rv    = new QVBoxLayout(right);
        rv->setContentsMargins(0, 0, 0, 0);

        removeBtn_ = new QPushButton("Remove from group", right);
        rv->addWidget(removeBtn_);

        trackTable_ = new QTableWidget(right);
        trackTable_->setColumnCount(3);
        trackTable_->setHorizontalHeaderLabels({"Title", "Artist", "Genre"});
        trackTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        trackTable_->setAlternatingRowColors(true);
        trackTable_->horizontalHeader()->setSectionResizeMode(kColTitle,  QHeaderView::Stretch);
        trackTable_->horizontalHeader()->setSectionResizeMode(kColArtist, QHeaderView::Stretch);
        trackTable_->horizontalHeader()->setSectionResizeMode(kColGenre,  QHeaderView::Stretch);
        rv->addWidget(trackTable_, 1);

        auto* form = new QFormLayout;
        nameEdit_ = new QLineEdit(right);
        descEdit_ = new QLineEdit(right);
        form->addRow("Name:", nameEdit_);
        form->addRow("Description:", descEdit_);
        rv->addLayout(form);

        saveBtn_ = new QPushButton("Save as Preset", right);
        saveBtn_->setEnabled(false);
        rv->addWidget(saveBtn_);

        splitter->addWidget(right);
    }

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    vbox->addWidget(splitter, 1);

    // ── Button box ────────────────────────────────────────────────────────────
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vbox->addWidget(btnBox);

    // ── Worker setup ─────────────────────────────────────────────────────────
    worker_ = new DiscoverWorker(this);
    connect(worker_, &DiscoverWorker::finished,
            this,    &AutoDiscoverDialog::onDiscoverFinished);

    // ── Signal wiring ─────────────────────────────────────────────────────────
    connect(discoverBtn_, &QPushButton::clicked, this, &AutoDiscoverDialog::onDiscover);
    connect(groupList_,   &QListWidget::currentRowChanged,
            this,         &AutoDiscoverDialog::onGroupSelected);
    connect(removeBtn_,   &QPushButton::clicked, this, &AutoDiscoverDialog::onRemoveTrack);
    connect(saveBtn_,     &QPushButton::clicked, this, &AutoDiscoverDialog::onSavePreset);
    connect(nameEdit_,    &QLineEdit::textChanged, this, [this] {
        const int row = groupList_->currentRow();
        if (row < 0) return;
        auto* item = groupList_->item(row);
        if (!item) return;
        const int groupIdx = item->data(Qt::UserRole).toInt();
        if (savedIdx_.count(groupIdx)) return;
        saveBtn_->setEnabled(!nameEdit_->text().trimmed().isEmpty()
                             && !groups_[static_cast<size_t>(groupIdx)].tracks.empty());
    });
}

// ─── Slots ───────────────────────────────────────────────────────────────────

void AutoDiscoverDialog::onDiscover() {
    discoverBtn_->setEnabled(false);
    statusLbl_->setText("Analysing\xe2\x80\xa6");

    groups_.clear();
    savedIdx_.clear();
    groupList_->clear();
    trackTable_->setRowCount(0);
    nameEdit_->clear();
    descEdit_->clear();

    const auto allTracks = ctx_.tracks.search({});
    if (allTracks.empty()) {
        statusLbl_->setText("No tracks in library.");
        discoverBtn_->setEnabled(true);
        return;
    }

    worker_->setup(allTracks, static_cast<float>(thresholdSpin_->value()));
    worker_->start();
}

void AutoDiscoverDialog::onDiscoverFinished(std::vector<pb::SimilarityGroup> groups) {
    discoverBtn_->setEnabled(true);
    groups_ = std::move(groups);
    populateGroupList();

    if (groups_.empty()) {
        statusLbl_->setText("No similar groups found.");
        return;
    }

    statusLbl_->setText(QString("%1 group(s) found.").arg(static_cast<int>(groups_.size())));
    groupList_->setCurrentRow(0);
}

void AutoDiscoverDialog::populateGroupList() {
    // Remember current selection by group index before clearing.
    int currentGroupIdx = -1;
    if (groupList_->currentItem())
        currentGroupIdx = groupList_->currentItem()->data(Qt::UserRole).toInt();

    groupList_->blockSignals(true);
    groupList_->clear();

    for (int i = 0; i < static_cast<int>(groups_.size()); ++i) {
        const auto& g    = groups_[static_cast<size_t>(i)];
        const QString name = QString::fromStdString(g.suggestedName);
        const QString text = name + " (" + QString::number(g.tracks.size()) + " tracks)";

        auto* item = new QListWidgetItem;
        item->setData(Qt::UserRole, i);

        if (savedIdx_.count(i)) {
            item->setText(QString::fromUtf8("\xe2\x9c\x93 ") + text);
            item->setForeground(Qt::gray);
            item->setFlags(Qt::NoItemFlags);
        } else {
            item->setText(text);
        }
        groupList_->addItem(item);
    }

    // Restore selection if possible.
    if (currentGroupIdx >= 0) {
        for (int r = 0; r < groupList_->count(); ++r) {
            if (groupList_->item(r)->data(Qt::UserRole).toInt() == currentGroupIdx) {
                groupList_->setCurrentRow(r);
                break;
            }
        }
    }

    groupList_->blockSignals(false);
}

void AutoDiscoverDialog::onGroupSelected(int row) {
    if (row < 0) return;
    auto* item = groupList_->item(row);
    if (!item) return;

    const int groupIdx = item->data(Qt::UserRole).toInt();
    if (savedIdx_.count(groupIdx)) return;

    populateTrackTable(groupIdx);
    nameEdit_->setText(QString::fromStdString(groups_[static_cast<size_t>(groupIdx)].suggestedName));
    descEdit_->clear();
    saveBtn_->setEnabled(!nameEdit_->text().trimmed().isEmpty()
                         && groups_[static_cast<size_t>(groupIdx)].tracks.size() >= 1);
}

void AutoDiscoverDialog::populateTrackTable(int groupIdx) {
    trackTable_->blockSignals(true);
    trackTable_->setRowCount(0);

    const auto& grp = groups_[static_cast<size_t>(groupIdx)];
    for (const auto& t : grp.tracks) {
        const int row = trackTable_->rowCount();
        trackTable_->insertRow(row);

        auto mkItem = [](const QString& text) {
            auto* it = new QTableWidgetItem(text);
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            return it;
        };

        const QString title  = t.metadata.title  ? QString::fromStdString(*t.metadata.title)  : QString();
        const QString artist = t.metadata.artist ? QString::fromStdString(*t.metadata.artist) : QString();
        const QString genre  = t.metadata.genre  ? QString::fromStdString(*t.metadata.genre)  : QString();

        auto* titleItem = mkItem(title);
        titleItem->setData(Qt::UserRole, QString::fromStdString(t.id.hash));
        trackTable_->setItem(row, kColTitle,  titleItem);
        trackTable_->setItem(row, kColArtist, mkItem(artist));
        trackTable_->setItem(row, kColGenre,  mkItem(genre));
    }

    trackTable_->blockSignals(false);
}

void AutoDiscoverDialog::onRemoveTrack() {
    const auto selectedRows = trackTable_->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) return;

    const int listRow = groupList_->currentRow();
    if (listRow < 0) return;
    auto* item = groupList_->item(listRow);
    if (!item) return;
    const int groupIdx = item->data(Qt::UserRole).toInt();

    // Collect row indices, sort descending.
    std::vector<int> rows;
    rows.reserve(static_cast<size_t>(selectedRows.size()));
    for (const auto& idx : selectedRows)
        rows.push_back(idx.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    auto& tracks = groups_[static_cast<size_t>(groupIdx)].tracks;
    for (int r : rows) {
        if (r >= 0 && r < static_cast<int>(tracks.size()))
            tracks.erase(tracks.begin() + r);
    }

    populateTrackTable(groupIdx);
    populateGroupList();  // updates track count text; restores selection

    saveBtn_->setEnabled(!nameEdit_->text().trimmed().isEmpty()
                         && !groups_[static_cast<size_t>(groupIdx)].tracks.empty());
}

void AutoDiscoverDialog::onSavePreset() {
    const int listRow = groupList_->currentRow();
    if (listRow < 0) return;
    auto* item = groupList_->item(listRow);
    if (!item) return;
    const int groupIdx = item->data(Qt::UserRole).toInt();

    const QString name = nameEdit_->text().trimmed();
    if (name.isEmpty()) return;

    const auto& grp = groups_[static_cast<size_t>(groupIdx)];

    pb::Preset preset;
    preset.id.uuid     = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    preset.name        = name.toStdString();
    preset.description = descEdit_->text().trimmed().toStdString();
    for (const auto& t : grp.tracks)
        preset.trackIds.push_back(t.id);

    pb::StatsService statsSvc;
    const pb::PresetStats stats = statsSvc.compute(grp.tracks);

    const std::string outputPath = buildOutputPath(name.toStdString());

    try {
        QDir().mkpath(QDir::homePath() + "/.config/MixAdvice/Presets");
        pb::ExportService exp;
        exp.exportXml(preset, stats, outputPath);
        ctx_.presets.save(preset);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Export failed", e.what());
        return;
    }

    savedIdx_.insert(groupIdx);
    populateGroupList();

    // Clear right panel.
    trackTable_->setRowCount(0);
    nameEdit_->clear();
    descEdit_->clear();
    saveBtn_->setEnabled(false);

    selectNextUnsaved();
    emit presetSaved();
}

void AutoDiscoverDialog::selectNextUnsaved() {
    for (int r = 0; r < groupList_->count(); ++r) {
        auto* it = groupList_->item(r);
        if (!it) continue;
        const int idx = it->data(Qt::UserRole).toInt();
        if (!savedIdx_.count(idx)) {
            groupList_->setCurrentRow(r);
            return;
        }
    }
}

std::string AutoDiscoverDialog::buildOutputPath(const std::string& name) const {
    return (QDir::homePath() + "/.config/MixAdvice/Presets/"
            + QString::fromStdString(gui::sanitizePresetName(name))
            + ".xml").toStdString();
}

void AutoDiscoverDialog::closeEvent(QCloseEvent* event) {
    // Wait for any running discovery to complete before closing,
    // to avoid destroying the worker while the thread is still running.
    if (worker_ && worker_->isRunning()) {
        worker_->wait();
    }
    QDialog::closeEvent(event);
}

} // namespace gui
