#pragma once

#include "preset_builder/services/ingest_service.hpp"

#include <QThread>
#include <QWidget>
#include <string>

namespace gui {
struct PresetBuilderCtx;
}

namespace pb {
class TrackRepository;
class MetadataProvider;
}

QT_BEGIN_NAMESPACE
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
QT_END_NAMESPACE

namespace gui {

class IngestWorker : public QThread {
    Q_OBJECT
public:
    explicit IngestWorker(QObject* parent = nullptr);
    void setup(const std::string& path,
               pb::TrackRepository& repo,
               pb::MetadataProvider& meta);

signals:
    void progress(float fraction, const QString& stage);
    void finished(pb::IngestReport report);

protected:
    void run() override;

private:
    std::string           path_;
    pb::TrackRepository*  repo_ = nullptr;
    pb::MetadataProvider* meta_ = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────

class IngestTab : public QWidget {
    Q_OBJECT
public:
    explicit IngestTab(PresetBuilderCtx& ctx, QWidget* parent = nullptr);

public:
    bool isWorkerRunning() const;
    void waitForWorker();

signals:
    void libraryChanged();
    void ingesting(bool active);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onAddFolder();
    void onProgress(float fraction, const QString& stage);
    void onFinished(pb::IngestReport report);

private:
    void startIngest(const std::string& path);
    void setRunning(bool running);

    PresetBuilderCtx& ctx_;
    IngestWorker*     worker_    = nullptr;

    QPushButton*  addBtn_        = nullptr;
    QLabel*       dropLabel_     = nullptr;
    QProgressBar* progressBar_   = nullptr;
    QLabel*       stageLabel_    = nullptr;
    QLabel*       addedLbl_      = nullptr;
    QLabel*       skippedLbl_    = nullptr;
    QLabel*       failedLbl_     = nullptr;
    QListWidget*  errorList_     = nullptr;
};

} // namespace gui
