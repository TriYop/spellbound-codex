#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/pipeline.hpp"
#include "mastertweak/preset.hpp"

#include <QMainWindow>
#include <QThread>
#include <optional>
#include <string>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QLabel;
class QProgressDialog;
class QPushButton;
QT_END_NAMESPACE

namespace gui {
class AnalysisPanel;
class ParameterPanel;
class PresetSelector;
class TransportWidget;
}

namespace gui {

// Worker thread for background analysis (file open / preset change).
// Carries a sequence number so stale results can be discarded.
class AnalysisWorker : public QThread {
    Q_OBJECT
public:
    explicit AnalysisWorker(QObject* parent = nullptr);
    void setup(const std::string& inputPath, const mt::PresetData& preset, int seq);

signals:
    void finished(bool ok, const QString& errorMsg, mt::MasterResult result, int seq);

protected:
    void run() override;

private:
    std::string    inputPath_;
    mt::PresetData preset_;
    int            seq_ = 0;
};

// Worker thread that runs the mastering pipeline without blocking the UI.
class RenderWorker : public QThread {
    Q_OBJECT
public:
    explicit RenderWorker(QObject* parent = nullptr);

    void setup(const std::string& inputPath,
               const std::string& outputPath,
               const mt::PresetData& preset,
               const mt::RenderOptions& opts,
               const mt::AdviceSet* adviceOverride);

signals:
    void progress(float fraction, const QString& stage);
    void finished(bool ok, const QString& errorMsg, mt::MasterResult result);

protected:
    void run() override;

private:
    std::string   inputPath_;
    std::string   outputPath_;
    mt::PresetData preset_;
    mt::RenderOptions opts_;
    std::optional<mt::AdviceSet> adviceOverride_;
};

// ─────────────────────────────────────────────────────────────────────────────

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onOpenFile();
    void onPresetChanged(const mt::PresetData& preset);
    void onRenderClicked();
    void onSaveAs();
    void onAnalysisFinished(bool ok, const QString& errorMsg, mt::MasterResult result, int seq);
    void onRenderProgress(float fraction, const QString& stage);
    void onRenderFinished(bool ok, const QString& errorMsg, mt::MasterResult result);

private:
    void buildUi();
    void startAnalysis();
    void setInputFile(const QString& path);
    QString makeDefaultOutputPath(bool flac) const;
    static std::string sanitizePresetName(const std::string& name);

    // ── State ─────────────────────────────────────────────────────────────────
    QString                  inputPath_;
    QString                  renderedPath_;   // last successfully rendered output
    std::optional<mt::PresetData>      currentPreset_;

    // ── Widgets ───────────────────────────────────────────────────────────────
    QLabel*                 inputLabel_      = nullptr;
    PresetSelector*         presetSelector_  = nullptr;
    AnalysisPanel*          analysisPanel_   = nullptr;
    ParameterPanel*         parameterPanel_  = nullptr;
    TransportWidget*        transport_       = nullptr;
    QPushButton*            renderButton_    = nullptr;
    QPushButton*            saveButton_      = nullptr;
    QPushButton*            resetBtn_        = nullptr;
    QComboBox*              bitDepthCombo_   = nullptr;
    QCheckBox*              flacCheck_       = nullptr;
    QLabel*                 statusLabel_     = nullptr;

    // ── Workers ───────────────────────────────────────────────────────────────
    AnalysisWorker*     analysisWorker_  = nullptr;
    int                 analysisSeq_     = 0;   // discard stale analysis results
    RenderWorker*       renderWorker_    = nullptr;
    QProgressDialog*    progressDialog_  = nullptr;
};

} // namespace gui
