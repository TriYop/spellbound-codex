#include "MainWindow.h"
#include "ChainPanel.h"
#include "PresetSelector.h"
#include "TargetLevelCombo.h"
#include "TransportWidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QScrollArea>
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
        connect(resetBtn_, &QPushButton::clicked, this, [this] {
            chainPanel_->resetToAdvice();
        });
        row->addWidget(lbl);
        row->addWidget(presetSelector_, 1);
        row->addWidget(resetBtn_);
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

    // ── Output format + Render row ────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;

        renderButton_ = new QPushButton("Render", central);
        saveButton_   = new QPushButton("Save As…", central);
        renderButton_->setEnabled(false);
        saveButton_->setEnabled(false);

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

        row->addWidget(renderButton_);
        row->addWidget(saveButton_);
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

    analysisWorker_ = new AnalysisWorker(this);
    connect(analysisWorker_, &AnalysisWorker::finished,
            this, &MainWindow::onAnalysisFinished);

    renderWorker_ = new RenderWorker(this);
    connect(renderWorker_, &RenderWorker::progress, this, &MainWindow::onRenderProgress);
    connect(renderWorker_, &RenderWorker::finished, this, &MainWindow::onRenderFinished);
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
    statusLabel_->setText(QString("Rendered \xe2\x86\x92 %1").arg(renderedPath_));
    transport_->loadFile(renderedPath_);

    chainPanel_->setAdvice(result.advice, result.analysis, *currentPreset_);
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

// static
std::string MainWindow::sanitizePresetName(const std::string& name) {
    std::string s;
    s.reserve(name.size());
    for (char ch : name) {
        const auto c = static_cast<unsigned char>(ch);
        s += std::isalnum(c) ? static_cast<char>(std::tolower(c)) : '_';
    }
    std::string out;
    bool prevUnder = false;
    for (char c : s) {
        if (c == '_') { if (!prevUnder) out += c; prevUnder = true; }
        else          { out += c; prevUnder = false; }
    }
    auto start = out.find_first_not_of('_');
    if (start == std::string::npos) return "preset";
    out = out.substr(start);
    auto end = out.find_last_not_of('_');
    if (end != std::string::npos) out = out.substr(0, end + 1);
    if (out.size() > 24) out.resize(24);
    auto end2 = out.find_last_not_of('_');
    if (end2 != std::string::npos) out = out.substr(0, end2 + 1);
    return out.empty() ? "preset" : out;
}

QString MainWindow::makeDefaultOutputPath(bool flac) const {
    if (inputPath_.isEmpty()) return {};
    fs::path p{inputPath_.toStdString()};
    const std::string suffix = currentPreset_ ? sanitizePresetName(currentPreset_->name) : "master";
    const std::string stem = p.stem().string() + "_" + suffix;
    return QString::fromStdString(
        (p.parent_path() / (stem + (flac ? ".flac" : ".wav"))).string());
}

} // namespace gui
