#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/pipeline.hpp"
#include "mastertweak/preset.hpp"
#include "MidiMapping.h"
#include "TargetLevelCombo.h"

#include <QMainWindow>
#include <QThread>
#include <optional>
#include <string>

QT_BEGIN_NAMESPACE
class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QProgressDialog;
class QPushButton;
QT_END_NAMESPACE

namespace gui {
class AudioControl;
class ChainPanel;
class MidiController;
class PresetBuilderDialog;
class PresetSelector;
class TransportWidget;
}

namespace gui {

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
    void onManagePresets();
    void onExportAdvice();
    void onMidiCC(int channel, int cc, int value);
    void onMidiNoteOn(int channel, int note, int velocity);

private:
    void buildUi();
    void startAnalysis();
    void setInputFile(const QString& path);
    QString makeDefaultOutputPath(bool flac) const;
    // ── State ─────────────────────────────────────────────────────────────────
    QString                       inputPath_;
    QString                       renderedPath_;
    std::optional<mt::PresetData> currentPreset_;
    mt::AnalysisSnapshot          lastSnap_;

    // ── Widgets ───────────────────────────────────────────────────────────────
    QLabel*           inputLabel_     = nullptr;
    PresetSelector*   presetSelector_ = nullptr;
    ChainPanel*       chainPanel_     = nullptr;
    TransportWidget*  transport_      = nullptr;
    QPushButton*      renderButton_   = nullptr;
    QPushButton*      saveButton_     = nullptr;
    QPushButton*      exportAdviceBtn_ = nullptr;
    QPushButton*      resetBtn_       = nullptr;
    QComboBox*        bitDepthCombo_  = nullptr;
    QCheckBox*        flacCheck_      = nullptr;
    TargetLevelCombo* targetCombo_    = nullptr;
    QLabel*           statusLabel_    = nullptr;

    PresetBuilderDialog* presetBuilderDialog_ = nullptr;

    // ── Workers ───────────────────────────────────────────────────────────────
    AnalysisWorker*  analysisWorker_ = nullptr;
    int              analysisSeq_    = 0;
    RenderWorker*    renderWorker_   = nullptr;
    QProgressDialog* progressDialog_ = nullptr;

    // ── MIDI ──────────────────────────────────────────────────────────────────
    MidiController* midi_              = nullptr;
    MidiMap         midiMap_           = MidiMap::defaultMap();
    QAction*        midiSettingsAction_ = nullptr;

    AudioControl* controlForParam(MidiParam p) const;
};

} // namespace gui
