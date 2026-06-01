#pragma once

#include "preset_builder/domain/track.hpp"
#include "preset_builder/services/similarity_service.hpp"

#include <QThread>
#include <QDialog>
#include <set>
#include <vector>

QT_BEGIN_NAMESPACE
class QCloseEvent;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSplitter;
class QTableWidget;
QT_END_NAMESPACE

namespace gui {
struct PresetBuilderCtx;
}

namespace gui {

class DiscoverWorker : public QThread {
    Q_OBJECT
public:
    explicit DiscoverWorker(QObject* parent = nullptr);
    void setup(std::vector<pb::Track> tracks, float threshold);
signals:
    void finished(std::vector<pb::SimilarityGroup> groups);
protected:
    void run() override;
private:
    std::vector<pb::Track> tracks_;
    float threshold_ = 1.5f;
};

// ─────────────────────────────────────────────────────────────────────────────

class AutoDiscoverDialog : public QDialog {
    Q_OBJECT
public:
    explicit AutoDiscoverDialog(PresetBuilderCtx& ctx, QWidget* parent = nullptr);

signals:
    void presetSaved();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onDiscover();
    void onDiscoverFinished(std::vector<pb::SimilarityGroup> groups);
    void onGroupSelected(int row);
    void onRemoveTrack();
    void onSavePreset();

private:
    void populateGroupList();
    void populateTrackTable(int groupIdx);
    void selectNextUnsaved();
    std::string buildOutputPath(const std::string& name) const;

    PresetBuilderCtx& ctx_;

    // Top controls
    QDoubleSpinBox* thresholdSpin_ = nullptr;
    QPushButton*    discoverBtn_   = nullptr;
    QLabel*         statusLbl_     = nullptr;

    // Splitter
    QListWidget*  groupList_   = nullptr;
    QTableWidget* trackTable_  = nullptr;
    QLineEdit*    nameEdit_    = nullptr;
    QLineEdit*    descEdit_    = nullptr;
    QPushButton*  removeBtn_   = nullptr;
    QPushButton*  saveBtn_     = nullptr;

    // State
    std::vector<pb::SimilarityGroup> groups_;   // in-memory working copy
    std::set<int>                    savedIdx_;  // indices of saved groups

    DiscoverWorker* worker_ = nullptr;

    static constexpr int kColTitle  = 0;
    static constexpr int kColArtist = 1;
    static constexpr int kColGenre  = 2;
};

} // namespace gui
