#pragma once

#include "preset_builder/domain/track.hpp"

#include <QWidget>
#include <set>
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

class BrowseTab : public QWidget {
    Q_OBJECT
public:
    explicit BrowseTab(PresetBuilderCtx& ctx, QWidget* parent = nullptr);

public slots:
    void refresh();  // re-run current filter and repopulate table

signals:
    void libraryChanged();             // emitted after delete
    void playRequested(const QString& path);  // emitted when Play is clicked

private slots:
    void onFilterChanged();
    void onItemChanged(QTableWidgetItem* item);
    void onDeleteSelected();
    void onSelectionChanged();
    void onPlaySelected();

private:
    void populateTable(const std::vector<pb::Track>& tracks);
    pb::TrackFilter currentFilter() const;
    QString selectedPath() const;  // path of the currently selected row, or ""

    PresetBuilderCtx& ctx_;

    QLineEdit*    titleEdit_   = nullptr;
    QLineEdit*    artistEdit_  = nullptr;
    QLineEdit*    genreEdit_   = nullptr;
    QTableWidget* table_       = nullptr;
    QLabel*       countLbl_    = nullptr;
    QTimer*       debounce_    = nullptr;
    QPushButton*  playBtn_     = nullptr;

    bool          blocking_    = false;  // suppresses itemChanged during populate

    // Column indices
    static constexpr int kColTitle  = 0;
    static constexpr int kColArtist = 1;
    static constexpr int kColAlbum  = 2;
    static constexpr int kColGenre  = 3;
    static constexpr int kColYear   = 4;
    static constexpr int kColSource = 5;
    static constexpr int kColPath   = 6;
};

} // namespace gui
