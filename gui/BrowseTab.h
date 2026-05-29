#pragma once
#include <QWidget>
namespace gui { struct PresetBuilderCtx; }
namespace gui {
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
