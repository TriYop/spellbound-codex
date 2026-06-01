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

    pb::Preset currentPreset_;

    static constexpr int kColTitle  = 0;
    static constexpr int kColArtist = 1;
    static constexpr int kColGenre  = 2;
    static constexpr int kColSub    = 3;
    static constexpr int kColRemove = 10;
};

} // namespace gui
