#include "CreatePresetTab.h"
#include "PresetBuilderCtx.h"
#include "utils.h"

#include "preset_builder/services/export_service.hpp"
#include "preset_builder/services/stats_service.hpp"

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
#include <QUuid>
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
        nameEdit_ = new QLineEdit(this);
        nameEdit_->setPlaceholderText("Preset name (required)");
        descEdit_ = new QLineEdit(this);
        descEdit_->setPlaceholderText("Description (optional)");
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
        titleEdit_  = new QLineEdit(left); titleEdit_->setPlaceholderText("Title\xe2\x80\xa6");
        artistEdit_ = new QLineEdit(left); artistEdit_->setPlaceholderText("Artist\xe2\x80\xa6");
        genreEdit_  = new QLineEdit(left); genreEdit_->setPlaceholderText("Genre\xe2\x80\xa6");
        searchRow->addWidget(titleEdit_);
        searchRow->addWidget(artistEdit_);
        searchRow->addWidget(genreEdit_);
        lv->addLayout(searchRow);

        // Select All / Deselect All
        auto* btnRow   = new QHBoxLayout;
        auto* selAll   = new QPushButton("Select All",   left);
        auto* deselAll = new QPushButton("Deselect All", left);
        btnRow->addWidget(selAll);
        btnRow->addWidget(deselAll);
        btnRow->addStretch();
        lv->addLayout(btnRow);

        // Track table (checkbox + title + artist + genre)
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
            statsDebounce_->start();
            updateExportButtons();
        });
        connect(deselAll, &QPushButton::clicked, this, [this] {
            selectedHashes_.clear();
            populateTrackTable(displayedTracks_);
            statsDebounce_->start();
            updateExportButtons();
        });

        splitter->addWidget(left);
    }

    // Right: stats panel
    {
        auto* right = new QGroupBox("Stats preview", splitter);
        auto* rv    = new QVBoxLayout(right);
        auto* form  = new QFormLayout;

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
        auto* row        = new QHBoxLayout;
        exportBtn_       = new QPushButton("Export to Presets folder", this);
        saveAsBtn_       = new QPushButton("Save As\xe2\x80\xa6", this);
        exportStatusLbl_ = new QLabel(this);
        exportBtn_->setEnabled(false);
        saveAsBtn_->setEnabled(false);
        connect(exportBtn_, &QPushButton::clicked, this, &CreatePresetTab::onExportDefault);
        connect(saveAsBtn_, &QPushButton::clicked, this, &CreatePresetTab::onSaveAs);
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

        auto mkReadOnly = [](const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            return item;
        };
        trackTable_->setItem(row, kColTitle,
            mkReadOnly(t.metadata.title  ? QString::fromStdString(*t.metadata.title)  : QString()));
        trackTable_->setItem(row, kColArtist,
            mkReadOnly(t.metadata.artist ? QString::fromStdString(*t.metadata.artist) : QString()));
        trackTable_->setItem(row, kColGenre,
            mkReadOnly(t.metadata.genre  ? QString::fromStdString(*t.metadata.genre)  : QString()));
    }

    trackTable_->blockSignals(false);
}

void CreatePresetTab::onSelectionChanged() {
    const QString dash = QString::fromUtf8("\xe2\x80\x94");
    const auto sel = selectedTracks();
    if (sel.empty()) {
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
    overallRmsLbl_->setText(
        QString("%1 dBFS").arg(static_cast<double>(stats.overallRmsDb),   0, 'f', 1));
    overallCorrLbl_->setText(
        QString("%1").arg(static_cast<double>(stats.overallCorrMin), 0, 'f', 2));
}

std::vector<pb::Track> CreatePresetTab::selectedTracks() const {
    std::vector<pb::Track> result;
    // First, add all displayed tracks that are selected.
    for (const auto& t : displayedTracks_) {
        if (selectedHashes_.count(t.id.hash)) result.push_back(t);
    }
    // Then add any selected tracks not currently displayed (filtered out).
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
    preset.id.uuid     = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    preset.name        = nameEdit_->text().trimmed().toStdString();
    preset.description = descEdit_->text().trimmed().toStdString();
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
    const QString defaultName =
        QString::fromStdString(gui::sanitizePresetName(nameEdit_->text().toStdString())) + ".xml";
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Preset XML",
        QDir::homePath() + "/" + defaultName,
        "XML Presets (*.xml);;All Files (*)");
    if (path.isEmpty()) return;
    doExport(path.toStdString());
}

} // namespace gui
