#include "AnalysisPanel.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>

namespace gui {

AnalysisPanel::AnalysisPanel(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void AnalysisPanel::buildUi() {
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // Toggle button (always visible)
    toggleBtn_ = new QToolButton(this);
    toggleBtn_->setText("▶ Analysis");
    toggleBtn_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toggleBtn_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(toggleBtn_, &QToolButton::clicked, this, &AnalysisPanel::onToggle);
    vbox->addWidget(toggleBtn_);

    // Content widget (collapsed by default)
    contentWidget_ = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(contentWidget_);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    auto* box = new QGroupBox(contentWidget_);
    auto* grid = new QGridLayout(box);
    grid->setColumnMinimumWidth(0, 70);

    // Header row
    grid->addWidget(new QLabel("<b>Band</b>"),    0, 0);
    grid->addWidget(new QLabel("<b>Avg RMS</b>"), 0, 1);
    grid->addWidget(new QLabel("<b>Peak RMS</b>"),0, 2);
    grid->addWidget(new QLabel("<b>Corr</b>"),    0, 3);
    grid->addWidget(new QLabel("<b>Crest</b>"),   0, 4);

    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        const int row = i + 1;
        grid->addWidget(new QLabel(
            QString::fromUtf8(mt::AnalysisSnapshot::kBandNames[si])), row, 0);
        avgLabels_  [i] = new QLabel("—", box); grid->addWidget(avgLabels_  [i], row, 1);
        peakLabels_ [i] = new QLabel("—", box); grid->addWidget(peakLabels_ [i], row, 2);
        corrLabels_ [i] = new QLabel("—", box); grid->addWidget(corrLabels_ [i], row, 3);
        crestLabels_[i] = new QLabel("—", box); grid->addWidget(crestLabels_[i], row, 4);
    }

    // Overall row
    const int overallRow = kNumBands + 1;
    grid->addWidget(new QLabel("<b>Overall</b>"), overallRow, 0);
    overallLabel_ = new QLabel("—", box);
    grid->addWidget(overallLabel_, overallRow, 1, 1, 4);

    contentLayout->addWidget(box);
    contentWidget_->setVisible(false);
    vbox->addWidget(contentWidget_);
}

void AnalysisPanel::onToggle() {
    collapsed_ = !collapsed_;
    contentWidget_->setVisible(!collapsed_);
    toggleBtn_->setText(collapsed_ ? "▶ Analysis" : "▼ Analysis");
}

void AnalysisPanel::setSnapshot(const mt::AnalysisSnapshot& snap) {
    for (int i = 0; i < kNumBands; ++i) {
        const auto& b = snap.bands[static_cast<size_t>(i)];
        avgLabels_  [i]->setText(QString::asprintf("%.1f dB",  b.avgRmsDb));
        peakLabels_ [i]->setText(QString::asprintf("%.1f dB",  b.peakRmsDb));
        corrLabels_ [i]->setText(QString::asprintf("%.3f",     b.correlation));
        crestLabels_[i]->setText(QString::asprintf("%.1f dB",  b.crestDb));
    }
    overallLabel_->setText(QString::asprintf("avg %.1f dBFS  peak %.1f dBFS  corr %.3f",
        snap.overallAvgDb, snap.overallPeakDb, snap.overallCorr));
}

void AnalysisPanel::clear() {
    for (int i = 0; i < kNumBands; ++i) {
        avgLabels_  [i]->setText("—");
        peakLabels_ [i]->setText("—");
        corrLabels_ [i]->setText("—");
        crestLabels_[i]->setText("—");
    }
    overallLabel_->setText("—");
}

} // namespace gui
