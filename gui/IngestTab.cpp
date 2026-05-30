#include "IngestTab.h"
#include "PresetBuilderCtx.h"

#include "preset_builder/services/ingest_service.hpp"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace gui {

// ─── IngestWorker ────────────────────────────────────────────────────────────

IngestWorker::IngestWorker(QObject* parent) : QThread(parent) {}

void IngestWorker::setup(const std::string& path,
                         pb::TrackRepository& repo,
                         pb::MetadataProvider& meta) {
    path_ = path;
    repo_ = &repo;
    meta_ = &meta;
    cancelled_.store(false, std::memory_order_relaxed);
}

void IngestWorker::requestStop() {
    cancelled_.store(true, std::memory_order_relaxed);
}

void IngestWorker::run() {
    pb::IngestService svc;
    auto report = svc.ingest(path_, *repo_, *meta_,
        [this](float f, const std::string& s) {
            emit progress(f, QString::fromStdString(s));
        },
        &cancelled_);
    emit finished(std::move(report));
}

// ─── IngestTab ───────────────────────────────────────────────────────────────

IngestTab::IngestTab(PresetBuilderCtx& ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx)
{
    setAcceptDrops(true);

    auto* vbox = new QVBoxLayout(this);
    vbox->setSpacing(8);

    // ── Input row ────────────────────────────────────────────────────────────
    {
        auto* row  = new QHBoxLayout;
        addBtn_    = new QPushButton("Add folder\xe2\x80\xa6", this);
        stopBtn_   = new QPushButton("Stop", this);
        stopBtn_->setEnabled(false);
        stopBtn_->hide();
        dropLabel_ = new QLabel("  or drop a folder / files here", this);
        dropLabel_->setStyleSheet(
            "border: 2px dashed #888; border-radius: 4px; padding: 6px; color: #555;");
        connect(addBtn_,  &QPushButton::clicked, this, &IngestTab::onAddFolder);
        connect(stopBtn_, &QPushButton::clicked, this, [this] {
            stopBtn_->setEnabled(false);
            worker_->requestStop();
        });
        row->addWidget(addBtn_);
        row->addWidget(stopBtn_);
        row->addWidget(dropLabel_, 1);
        vbox->addLayout(row);
    }

    // ── Progress ──────────────────────────────────────────────────────────────
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->hide();
    stageLabel_  = new QLabel(this);
    stageLabel_->hide();
    vbox->addWidget(progressBar_);
    vbox->addWidget(stageLabel_);

    // ── Report ────────────────────────────────────────────────────────────────
    {
        auto* grp  = new QGroupBox("Last ingest result", this);
        auto* grid = new QHBoxLayout(grp);
        addedLbl_   = new QLabel(QString::fromUtf8("Added: \xe2\x80\x94"),   grp);
        skippedLbl_ = new QLabel(QString::fromUtf8("Skipped: \xe2\x80\x94"), grp);
        failedLbl_  = new QLabel(QString::fromUtf8("Failed: \xe2\x80\x94"),  grp);
        grid->addWidget(addedLbl_);
        grid->addWidget(skippedLbl_);
        grid->addWidget(failedLbl_);
        grid->addStretch();
        vbox->addWidget(grp);
    }

    errorList_ = new QListWidget(this);
    errorList_->hide();
    vbox->addWidget(errorList_, 1);
    vbox->addStretch();

    worker_ = new IngestWorker(this);
    connect(worker_, &IngestWorker::progress, this, &IngestTab::onProgress);
    connect(worker_, &IngestWorker::finished, this, &IngestTab::onFinished);
}

void IngestTab::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void IngestTab::dropEvent(QDropEvent* e) {
    const auto urls = e->mimeData()->urls();
    if (urls.isEmpty()) return;
    startIngest(urls.first().toLocalFile().toStdString());
}

void IngestTab::onAddFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select folder to ingest");
    if (!dir.isEmpty()) startIngest(dir.toStdString());
}

void IngestTab::startIngest(const std::string& path) {
    if (worker_->isRunning()) return;
    errorList_->clear();
    errorList_->hide();
    setRunning(true);
    emit ingesting(true);
    worker_->setup(path, ctx_.tracks, ctx_.metadata);
    worker_->start();
}

void IngestTab::onProgress(float fraction, const QString& stage) {
    progressBar_->setValue(static_cast<int>(fraction * 100.f));
    stageLabel_->setText(stage);
}

void IngestTab::onFinished(pb::IngestReport report) {
    setRunning(false);
    emit ingesting(false);
    addedLbl_->setText(  QString(report.cancelled ? "Added: %1 (stopped)" : "Added: %1").arg(report.added));
    skippedLbl_->setText(QString("Skipped: %1").arg(report.skipped));
    failedLbl_->setText( QString("Failed: %1").arg(report.failed));

    if (!report.errors.empty()) {
        errorList_->show();
        for (const auto& [p, msg] : report.errors)
            errorList_->addItem(QString::fromStdString(p + ": " + msg));
    }

    emit libraryChanged();
}

void IngestTab::setRunning(bool running) {
    addBtn_->setEnabled(!running);
    stopBtn_->setVisible(running);
    stopBtn_->setEnabled(running);
    progressBar_->setVisible(running);
    stageLabel_->setVisible(running);
    if (running) progressBar_->setValue(0);
}

bool IngestTab::isWorkerRunning() const { return worker_->isRunning(); }
void IngestTab::waitForWorker()          { if (worker_->isRunning()) worker_->wait(); }

} // namespace gui
