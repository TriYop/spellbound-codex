#include "BrowseTab.h"
#include "PresetBuilderCtx.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include <set>

namespace gui {

BrowseTab::BrowseTab(PresetBuilderCtx& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setSpacing(6);

    // ── Search row ────────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        titleEdit_  = new QLineEdit(this); titleEdit_->setPlaceholderText("Title\xe2\x80\xa6");
        artistEdit_ = new QLineEdit(this); artistEdit_->setPlaceholderText("Artist\xe2\x80\xa6");
        genreEdit_  = new QLineEdit(this); genreEdit_->setPlaceholderText("Genre\xe2\x80\xa6");
        row->addWidget(new QLabel("Title:",  this));  row->addWidget(titleEdit_,  1);
        row->addWidget(new QLabel("Artist:", this));  row->addWidget(artistEdit_, 1);
        row->addWidget(new QLabel("Genre:",  this));  row->addWidget(genreEdit_,  1);
        vbox->addLayout(row);
    }

    // ── Table ─────────────────────────────────────────────────────────────────
    table_ = new QTableWidget(this);
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels(
        {"Title", "Artist", "Album", "Genre", "Year", "Source", "Path"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kColSource, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kColYear,   QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setAlternatingRowColors(true);
    vbox->addWidget(table_, 1);

    // ── Bottom toolbar ────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        auto* delBtn = new QPushButton("Delete selected", this);
        countLbl_    = new QLabel("0 tracks", this);
        connect(delBtn, &QPushButton::clicked, this, &BrowseTab::onDeleteSelected);
        row->addWidget(delBtn);
        row->addStretch();
        row->addWidget(countLbl_);
        vbox->addLayout(row);
    }

    // ── Debounce timer ────────────────────────────────────────────────────────
    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(200);
    connect(debounce_, &QTimer::timeout, this, &BrowseTab::refresh);

    connect(titleEdit_,  &QLineEdit::textChanged, this, &BrowseTab::onFilterChanged);
    connect(artistEdit_, &QLineEdit::textChanged, this, &BrowseTab::onFilterChanged);
    connect(genreEdit_,  &QLineEdit::textChanged, this, &BrowseTab::onFilterChanged);

    connect(table_, &QTableWidget::itemChanged, this, &BrowseTab::onItemChanged);
}

void BrowseTab::onFilterChanged() { debounce_->start(); }

pb::TrackFilter BrowseTab::currentFilter() const {
    pb::TrackFilter f;
    if (!titleEdit_->text().isEmpty())  f.title  = titleEdit_->text().toStdString();
    if (!artistEdit_->text().isEmpty()) f.artist = artistEdit_->text().toStdString();
    if (!genreEdit_->text().isEmpty())  f.genre  = genreEdit_->text().toStdString();
    return f;
}

void BrowseTab::refresh() {
    populateTable(ctx_.tracks.search(currentFilter()));
}

void BrowseTab::populateTable(const std::vector<pb::Track>& tracks) {
    blocking_ = true;
    table_->setRowCount(0);

    for (const auto& t : tracks) {
        const int row = table_->rowCount();
        table_->insertRow(row);

        auto* titleItem = new QTableWidgetItem(
            t.metadata.title ? QString::fromStdString(*t.metadata.title) : QString());
        titleItem->setData(Qt::UserRole, QString::fromStdString(t.id.hash));
        table_->setItem(row, kColTitle,  titleItem);

        table_->setItem(row, kColArtist,
            new QTableWidgetItem(t.metadata.artist
                ? QString::fromStdString(*t.metadata.artist) : QString()));
        table_->setItem(row, kColAlbum,
            new QTableWidgetItem(t.metadata.album
                ? QString::fromStdString(*t.metadata.album) : QString()));
        table_->setItem(row, kColGenre,
            new QTableWidgetItem(t.metadata.genre
                ? QString::fromStdString(*t.metadata.genre) : QString()));
        table_->setItem(row, kColYear,
            new QTableWidgetItem(t.metadata.year
                ? QString::number(*t.metadata.year) : QString()));

        auto* srcItem = new QTableWidgetItem(
            t.metadata.source == pb::MetadataSource::acoustid ? "AcoustID" : "filename");
        srcItem->setFlags(srcItem->flags() & ~Qt::ItemIsEditable);
        table_->setItem(row, kColSource, srcItem);

        auto* pathItem = new QTableWidgetItem(QString::fromStdString(t.path));
        pathItem->setFlags(pathItem->flags() & ~Qt::ItemIsEditable);
        pathItem->setToolTip(pathItem->text());
        table_->setItem(row, kColPath, pathItem);
    }

    countLbl_->setText(QString("%1 track%2")
        .arg(table_->rowCount())
        .arg(table_->rowCount() == 1 ? "" : "s"));
    blocking_ = false;
}

void BrowseTab::onItemChanged(QTableWidgetItem* item) {
    if (blocking_) return;
    const int row = item->row();

    // Retrieve track hash stored in the Title cell's UserRole.
    const QString hash = table_->item(row, kColTitle)->data(Qt::UserRole).toString();
    auto existing = ctx_.tracks.find(pb::TrackId{hash.toStdString()});
    if (!existing) return;

    pb::Track t = *existing;
    auto textOrNull = [&](int col) -> std::optional<std::string> {
        const QString v = table_->item(row, col)->text().trimmed();
        if (v.isEmpty()) return std::nullopt;
        return v.toStdString();
    };

    t.metadata.title  = textOrNull(kColTitle);
    t.metadata.artist = textOrNull(kColArtist);
    t.metadata.album  = textOrNull(kColAlbum);
    t.metadata.genre  = textOrNull(kColGenre);

    const QString yearStr = table_->item(row, kColYear)->text().trimmed();
    if (yearStr.isEmpty()) t.metadata.year.reset();
    else { bool ok; int y = yearStr.toInt(&ok); if (ok) t.metadata.year = y; }

    ctx_.tracks.save(t);
}

void BrowseTab::onDeleteSelected() {
    const auto sel = table_->selectedItems();
    if (sel.isEmpty()) return;

    // Collect unique row indices.
    std::set<int> rows;
    for (auto* item : sel) rows.insert(item->row());

    if (QMessageBox::question(this, "Delete tracks",
            QString("Delete %1 track(s) from the library?").arg(static_cast<int>(rows.size())),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    for (int r : rows) {
        const QString hash = table_->item(r, kColTitle)->data(Qt::UserRole).toString();
        ctx_.tracks.remove(pb::TrackId{hash.toStdString()});
    }

    refresh();
    emit libraryChanged();
}

} // namespace gui
