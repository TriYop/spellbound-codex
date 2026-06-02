#include "MainWindow.h"
#include "AudioControl.h"
#include "ChainPanel.h"
#include "MidiController.h"
#include "MidiSettingsDialog.h"
#include "PresetBuilderDialog.h"
#include "PresetSelector.h"
#include "RotaryKnob.h"
#include "TargetLevelCombo.h"
#include "TransportWidget.h"
#include "VerticalFader.h"
#include "utils.h"
#include "mastertweak/report.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace gui {

// ─────────────────────────────────────────────────────────────────────────────
// AnalysisWorker
// ─────────────────────────────────────────────────────────────────────────────

AnalysisWorker::AnalysisWorker(QObject* parent) : QThread(parent) {}

void AnalysisWorker::setup(const std::string& inputPath,
                            const mt::PresetData& preset, int seq) {
    inputPath_ = inputPath;
    preset_    = preset;
    seq_       = seq;
}

void AnalysisWorker::run() {
    std::string err;
    auto result = mt::analyseOnly(inputPath_, preset_, &err);
    emit finished(result.ok, QString::fromStdString(err), std::move(result), seq_);
}

// ─────────────────────────────────────────────────────────────────────────────
// RenderWorker
// ─────────────────────────────────────────────────────────────────────────────

RenderWorker::RenderWorker(QObject* parent) : QThread(parent) {}

void RenderWorker::setup(const std::string& inputPath,
                         const std::string& outputPath,
                         const mt::PresetData& preset,
                         const mt::RenderOptions& opts,
                         const mt::AdviceSet* adviceOverride) {
    inputPath_  = inputPath;
    outputPath_ = outputPath;
    preset_     = preset;
    opts_       = opts;
    if (adviceOverride) adviceOverride_ = *adviceOverride;
    else                adviceOverride_.reset();
}

void RenderWorker::run() {
    std::string err;
    auto result = mt::renderFile(
        inputPath_, outputPath_, preset_, opts_,
        adviceOverride_ ? &*adviceOverride_ : nullptr,
        [this](float frac, const std::string& stage) {
            emit progress(frac, QString::fromStdString(stage));
        },
        &err);
    emit finished(result.ok, QString::fromStdString(err), std::move(result));
}

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow
// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildUi();

    const std::string execDir = fs::path{
        QApplication::applicationFilePath().toStdString()
    }.parent_path().string();
    presetSelector_->populate(execDir);
}

void MainWindow::buildUi() {
    setWindowTitle("MasterTweak");
    resize(900, 740);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* vbox = new QVBoxLayout(central);

    // ── Input file row ────────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        auto* openBtn = new QPushButton("Open audio…", central);
        inputLabel_ = new QLabel("(no file)", central);
        inputLabel_->setWordWrap(false);
        connect(openBtn, &QPushButton::clicked, this, &MainWindow::onOpenFile);
        row->addWidget(openBtn);
        row->addWidget(inputLabel_, 1);
        vbox->addLayout(row);
    }

    // ── Preset row ────────────────────────────────────────────────────────────
    {
        auto* row  = new QHBoxLayout;
        auto* lbl  = new QLabel("Preset:", central);
        presetSelector_ = new PresetSelector(central);
        resetBtn_ = new QPushButton(QString::fromUtf8("\xe2\x86\xba Reset"), central);
        resetBtn_->setEnabled(false);
        connect(presetSelector_, &PresetSelector::presetChanged,
                this, &MainWindow::onPresetChanged);
        connect(presetSelector_, &PresetSelector::manageRequested,
                this, &MainWindow::onManagePresets);
        connect(resetBtn_, &QPushButton::clicked, this, [this] {
            chainPanel_->resetToAdvice();
        });
        row->addWidget(lbl);
        row->addWidget(presetSelector_, 1);
        row->addWidget(resetBtn_);
        vbox->addLayout(row);
    }

    // ── Output format + Render row ────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;

        renderButton_ = new QPushButton("Render", central);
        saveButton_   = new QPushButton("Save As…", central);
        renderButton_->setEnabled(false);
        saveButton_->setEnabled(false);

        exportAdviceBtn_ = new QPushButton(QString::fromUtf8("Export Advice\xe2\x80\xa6"), central);
        exportAdviceBtn_->setEnabled(false);

        auto* depthLbl = new QLabel("Bit depth:", central);
        bitDepthCombo_ = new QComboBox(central);
        bitDepthCombo_->addItem("16-bit", 16);
        bitDepthCombo_->addItem("24-bit", 24);
        bitDepthCombo_->addItem("32-bit float", 32);
        bitDepthCombo_->setCurrentIndex(1);

        flacCheck_ = new QCheckBox("FLAC", central);

        auto* targetLbl = new QLabel("Target:", central);
        targetCombo_ = new TargetLevelCombo(central);

        statusLabel_ = new QLabel("Ready", central);

        connect(renderButton_, &QPushButton::clicked, this, &MainWindow::onRenderClicked);
        connect(saveButton_,   &QPushButton::clicked, this, &MainWindow::onSaveAs);
        connect(exportAdviceBtn_, &QPushButton::clicked, this, &MainWindow::onExportAdvice);

        row->addWidget(renderButton_);
        row->addWidget(saveButton_);
        row->addWidget(exportAdviceBtn_);
        row->addSpacing(16);
        row->addWidget(depthLbl);
        row->addWidget(bitDepthCombo_);
        row->addWidget(flacCheck_);
        row->addSpacing(8);
        row->addWidget(targetLbl);
        row->addWidget(targetCombo_);
        row->addWidget(statusLabel_, 1);
        vbox->addLayout(row);
    }

    // ── Chain panel (scrollable) ──────────────────────────────────────────────
    {
        auto* scroll = new QScrollArea(central);
        scroll->setWidgetResizable(true);
        auto* inner  = new QWidget;
        auto* ivbox  = new QVBoxLayout(inner);

        chainPanel_ = new ChainPanel(inner);
        ivbox->addWidget(chainPanel_);
        ivbox->addStretch();

        scroll->setWidget(inner);
        vbox->addWidget(scroll, 1);
    }

    // ── Transport ─────────────────────────────────────────────────────────────
    transport_ = new TransportWidget(central);
    vbox->addWidget(transport_);

    analysisWorker_ = new AnalysisWorker(this);
    connect(analysisWorker_, &AnalysisWorker::finished,
            this, &MainWindow::onAnalysisFinished);

    renderWorker_ = new RenderWorker(this);
    connect(renderWorker_, &RenderWorker::progress, this, &MainWindow::onRenderProgress);
    connect(renderWorker_, &RenderWorker::finished, this, &MainWindow::onRenderFinished);

    // ── MIDI controller ───────────────────────────────────────────────────────
    midi_ = new MidiController(this);
    connect(midi_, &MidiController::ccReceived,
            this,  &MainWindow::onMidiCC);
    connect(midi_, &MidiController::noteOnReceived,
            this,  &MainWindow::onMidiNoteOn);

    // GUI → MIDI feedback: valueChanged fires only on user interaction (not setValue),
    // so connecting here creates no feedback loop.
    auto connectFeedback = [this](AudioControl* ctrl, MidiParam p) {
        connect(ctrl, &AudioControl::valueChanged, this,
                [this, ctrl, p](double v) {
                    const auto& b = midiMap_.ccFor(p);
                    if (b.cc >= 0)
                        midi_->sendCC(0, b.cc,
                                      valueToCC(v, ctrl->minimum(), ctrl->maximum()));
                });
    };
    for (int i = 0; i < 7; ++i)
        connectFeedback(chainPanel_->eqGainFader(i),
                        static_cast<MidiParam>(static_cast<int>(MidiParam::EqBand0) + i));
    connectFeedback(chainPanel_->satDriveKnob(),     MidiParam::SatDrive);
    connectFeedback(chainPanel_->mixbusThreshKnob(), MidiParam::MixbusThresh);
    connectFeedback(chainPanel_->mixbusMakeupKnob(), MidiParam::MixbusMakeup);
    connectFeedback(chainPanel_->limCeilingFader(),  MidiParam::LimCeiling);

    // Re-open MIDI ports saved from last session.
    {
        QSettings s("MasterTweak", "MasterTweak");
        const QString savedIn  = s.value("midi/inputPort").toString();
        const QString savedOut = s.value("midi/outputPort").toString();
        const auto inPorts  = midi_->inputPortNames();
        const auto outPorts = midi_->outputPortNames();
        for (unsigned i = 0; i < inPorts.size(); ++i)
            if (inPorts[i] == savedIn)  { midi_->openInput(i);  break; }
        for (unsigned i = 0; i < outPorts.size(); ++i)
            if (outPorts[i] == savedOut) { midi_->openOutput(i); break; }
    }

    // ── MIDI menu ─────────────────────────────────────────────────────────────
    auto* midiMenu = menuBar()->addMenu("MIDI");
    midiSettingsAction_ = midiMenu->addAction(QString::fromUtf8("MIDI Settings\xe2\x80\xa6"));
    connect(midiSettingsAction_, &QAction::triggered, this, [this] {
        MidiSettingsDialog dlg(midi_, this);
        dlg.exec();
    });
    midiSettingsAction_->setEnabled(true);
}

void MainWindow::onOpenFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Audio File", QString(),
        "Audio Files (*.wav *.flac *.aiff *.aif);;All Files (*)");
    if (path.isEmpty()) return;
    setInputFile(path);
}

void MainWindow::setInputFile(const QString& path) {
    inputPath_ = path;
    inputLabel_->setText(path);
    chainPanel_->clear();
    transport_->unload();
    transport_->setOriginalFile(path);
    renderedPath_.clear();
    saveButton_->setEnabled(false);
    renderButton_->setEnabled(false);
    exportAdviceBtn_->setEnabled(false);
    resetBtn_->setEnabled(false);
    startAnalysis();
}

void MainWindow::onPresetChanged(const mt::PresetData& preset) {
    currentPreset_ = preset;
    startAnalysis();
}

void MainWindow::startAnalysis() {
    if (inputPath_.isEmpty() || !currentPreset_) return;

    const int seq = ++analysisSeq_;

    if (analysisWorker_->isRunning()) {
        analysisWorker_->requestInterruption();
        analysisWorker_->wait(500);
    }
    statusLabel_->setText(QString::fromUtf8("Analysing\xe2\x80\xa6"));
    renderButton_->setEnabled(false);
    if (resetBtn_) resetBtn_->setEnabled(false);
    exportAdviceBtn_->setEnabled(false);
    analysisWorker_->setup(inputPath_.toStdString(), *currentPreset_, seq);
    analysisWorker_->start();
}

void MainWindow::onAnalysisFinished(bool ok, const QString& errorMsg,
                                    mt::MasterResult result, int seq) {
    if (seq != analysisSeq_) return;

    if (!ok) {
        statusLabel_->setText(QString("Analysis failed: %1").arg(errorMsg));
        return;
    }
    chainPanel_->setAdvice(result.advice, result.analysis, *currentPreset_);
    lastSnap_ = result.analysis;
    exportAdviceBtn_->setEnabled(true);
    resetBtn_->setEnabled(true);
    statusLabel_->setText(QString::fromUtf8("Analysis complete \xe2\x80\x94 ready to render"));
    renderButton_->setEnabled(true);
}

void MainWindow::onRenderClicked() {
    if (inputPath_.isEmpty() || !currentPreset_) return;

    const bool useFlac = flacCheck_->isChecked();
    renderedPath_ = makeDefaultOutputPath(useFlac);
    renderButton_->setEnabled(false);

    auto* dlg = new QProgressDialog(QString::fromUtf8("Rendering\xe2\x80\xa6"), "Cancel", 0, 100, this);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setAutoClose(false);
    progressDialog_ = dlg;
    dlg->show();

    connect(dlg, &QProgressDialog::canceled, this, [this] {
        if (renderWorker_->isRunning()) renderWorker_->terminate();
    });

    mt::RenderOptions opts;
    chainPanel_->populateBypassFlags(opts);
    opts.outputBitDepth = bitDepthCombo_->currentData().toInt();
    opts.outputFlac     = useFlac;
    opts.targetLevel    = targetCombo_->currentTarget();

    mt::AdviceSet params = chainPanel_->currentAdvice();
    renderWorker_->setup(inputPath_.toStdString(), renderedPath_.toStdString(),
                         *currentPreset_, opts, &params);
    renderWorker_->start();
}

void MainWindow::onRenderProgress(float fraction, const QString& stage) {
    if (progressDialog_) {
        progressDialog_->setValue(static_cast<int>(fraction * 100.f));
        progressDialog_->setLabelText(stage);
    }
}

void MainWindow::onRenderFinished(bool ok, const QString& errorMsg, mt::MasterResult result) {
    if (progressDialog_) {
        progressDialog_->close();
        progressDialog_->deleteLater();
        progressDialog_ = nullptr;
    }
    renderButton_->setEnabled(true);

    if (!ok) {
        QMessageBox::critical(this, "Render failed", errorMsg);
        statusLabel_->setText("Render failed");
        return;
    }

    saveButton_->setEnabled(true);
    exportAdviceBtn_->setEnabled(true);
    statusLabel_->setText(QString("Rendered \xe2\x86\x92 %1").arg(renderedPath_));
    transport_->loadFile(renderedPath_);

    chainPanel_->setAdvice(result.advice, result.analysis, *currentPreset_);
    lastSnap_ = result.analysis;
}

void MainWindow::onSaveAs() {
    if (renderedPath_.isEmpty()) return;
    const bool flac = renderedPath_.endsWith(".flac", Qt::CaseInsensitive);
    const QString dest = QFileDialog::getSaveFileName(
        this, "Save Mastered File", renderedPath_,
        flac ? "FLAC Files (*.flac);;All Files (*)"
             : "WAV Files (*.wav);;All Files (*)");
    if (dest.isEmpty()) return;

    if (!QFile::copy(renderedPath_, dest)) {
        QMessageBox::warning(this, "Save failed",
            QString("Could not copy %1 to %2").arg(renderedPath_, dest));
    }
}

QString MainWindow::makeDefaultOutputPath(bool flac) const {
    if (inputPath_.isEmpty()) return {};
    fs::path p{inputPath_.toStdString()};
    const std::string suffix = currentPreset_ ? gui::sanitizePresetName(currentPreset_->name) : "master";
    const std::string stem = p.stem().string() + "_" + suffix;
    return QString::fromStdString(
        (p.parent_path() / (stem + (flac ? ".flac" : ".wav"))).string());
}

void MainWindow::onManagePresets() {
    if (!presetBuilderDialog_) {
        const std::string execDir = fs::path{
            QApplication::applicationFilePath().toStdString()
        }.parent_path().string();

        presetBuilderDialog_ = new PresetBuilderDialog(execDir, this);
        connect(presetBuilderDialog_, &PresetBuilderDialog::presetExported,
                this, [this] {
                    const std::string execDir2 = fs::path{
                        QApplication::applicationFilePath().toStdString()
                    }.parent_path().string();
                    presetSelector_->populate(execDir2);
                });
    }
    presetBuilderDialog_->show();
    presetBuilderDialog_->raise();
    presetBuilderDialog_->activateWindow();
}

AudioControl* MainWindow::controlForParam(MidiParam p) const {
    switch (p) {
        case MidiParam::EqBand0: return chainPanel_->eqGainFader(0);
        case MidiParam::EqBand1: return chainPanel_->eqGainFader(1);
        case MidiParam::EqBand2: return chainPanel_->eqGainFader(2);
        case MidiParam::EqBand3: return chainPanel_->eqGainFader(3);
        case MidiParam::EqBand4: return chainPanel_->eqGainFader(4);
        case MidiParam::EqBand5: return chainPanel_->eqGainFader(5);
        case MidiParam::EqBand6: return chainPanel_->eqGainFader(6);
        case MidiParam::SatDrive:     return chainPanel_->satDriveKnob();
        case MidiParam::MixbusThresh: return chainPanel_->mixbusThreshKnob();
        case MidiParam::MixbusMakeup: return chainPanel_->mixbusMakeupKnob();
        case MidiParam::LimCeiling:   return chainPanel_->limCeilingFader();
        default: return nullptr;
    }
}

void MainWindow::onMidiCC(int channel, int cc, int value) {
    // Check CC-controlled params
    for (std::size_t i = 0; i < static_cast<std::size_t>(kMidiParamCcCount); ++i) {
        const auto& b = midiMap_.ccBindings[i];
        if (b.cc == cc && (b.channel < 0 || b.channel == channel)) {
            if (auto* ctrl = controlForParam(static_cast<MidiParam>(static_cast<int>(i))))
                ctrl->setValue(ccToValue(value, ctrl->minimum(), ctrl->maximum()));
            return;
        }
    }
    // Check transport triggers — nanoKONTROL2 sends CC (not Note) for transport.
    // We store the CC# in noteBindings[].note and check here on value > 63 (button press).
    if (value > 63) {
        if (midiMap_.noteBindings[0].note == cc) transport_->play();
        if (midiMap_.noteBindings[1].note == cc) transport_->stop();
    }
}

void MainWindow::onMidiNoteOn(int channel, int note, int /*velocity*/) {
    const auto& nb0 = midiMap_.noteBindings[0];
    const auto& nb1 = midiMap_.noteBindings[1];
    if (nb0.note == note && (nb0.channel < 0 || nb0.channel == channel))
        transport_->play();
    if (nb1.note == note && (nb1.channel < 0 || nb1.channel == channel))
        transport_->stop();
}

void MainWindow::onExportAdvice() {
    if (!currentPreset_) return;

    const QString defaultPath = [this]() -> QString {
        if (inputPath_.isEmpty()) return "advice.md";
        const fs::path p{inputPath_.toStdString()};
        const std::string stem = p.stem().string() + "_advice";
        return QString::fromStdString(
            (p.parent_path() / (stem + ".md")).string());
    }();

    const QString dest = QFileDialog::getSaveFileName(
        this, "Export Advice", defaultPath,
        "Markdown Files (*.md);;All Files (*)");
    if (dest.isEmpty()) return;

    const std::string content = mt::formatAdviceMarkdown(
        lastSnap_,
        chainPanel_->currentAdvice(),
        *currentPreset_,
        inputPath_.toStdString());

    QFile f(dest);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Export failed",
            QString("Could not write to %1").arg(dest));
        return;
    }
    const qint64 written = f.write(QByteArray::fromStdString(content));
    f.close();
    if (written < 0 || written != static_cast<qint64>(content.size())) {
        QMessageBox::warning(this, "Export failed",
            QString("Could not write to %1").arg(dest));
        return;
    }
    statusLabel_->setText(QString::fromUtf8("Advice exported \xe2\x86\x92 %1").arg(dest));
}

} // namespace gui
