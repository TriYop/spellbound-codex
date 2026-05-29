#pragma once
#include <QWidget>
namespace gui { struct PresetBuilderCtx; }
namespace gui {
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
