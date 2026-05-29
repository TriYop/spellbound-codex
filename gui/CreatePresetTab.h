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
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
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

    // Stats panel labels (7 bands)
    QLabel* statsLabels_[7]{};
    QLabel* overallRmsLbl_  = nullptr;
    QLabel* overallCorrLbl_ = nullptr;

    // Export
    QLabel*      exportStatusLbl_ = nullptr;
    QPushButton* exportBtn_       = nullptr;
    QPushButton* saveAsBtn_       = nullptr;

    // State
    std::set<std::string>  selectedHashes_;   // persists across filter changes
    std::vector<pb::Track> displayedTracks_;  // current table contents

    StatsWorker* statsWorker_   = nullptr;
    QTimer*      statsDebounce_ = nullptr;

    static constexpr int kColCheck  = 0;
    static constexpr int kColTitle  = 1;
    static constexpr int kColArtist = 2;
    static constexpr int kColGenre  = 3;
};

} // namespace gui
