#pragma once
#include <QWidget>
namespace gui { struct PresetBuilderCtx; }
namespace gui {
class IngestTab : public QWidget {
    Q_OBJECT
public:
    explicit IngestTab(PresetBuilderCtx& ctx, QWidget* parent = nullptr);
signals:
    void libraryChanged();
};
} // namespace gui
