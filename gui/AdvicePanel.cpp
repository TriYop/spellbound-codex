#include "AdvicePanel.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>

namespace gui {

AdvicePanel::AdvicePanel(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void AdvicePanel::buildUi() {
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);

    auto* box  = new QGroupBox("Advice", this);
    auto* grid = new QGridLayout(box);
    grid->setColumnMinimumWidth(0, 70);

    // Header
    grid->addWidget(new QLabel("<b>Band</b>"),      0, 0);
    grid->addWidget(new QLabel("<b>EQ gain</b>"),   0, 1);
    grid->addWidget(new QLabel("<b>MB comp</b>"),   0, 2);
    grid->addWidget(new QLabel("<b>Width</b>"),     0, 3);

    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        const int row = i + 1;
        grid->addWidget(new QLabel(
            QString::fromUtf8(mt::AnalysisSnapshot::kBandNames[si])), row, 0);
        eqLabels_   [i] = new QLabel("—", box); grid->addWidget(eqLabels_   [i], row, 1);
        mbLabels_   [i] = new QLabel("—", box); grid->addWidget(mbLabels_   [i], row, 2);
        widthLabels_[i] = new QLabel("—", box); grid->addWidget(widthLabels_[i], row, 3);
    }

    const int sumRow = kNumBands + 1;
    grid->addWidget(new QLabel("<b>Other</b>"), sumRow, 0);
    summaryLabel_ = new QLabel("—", box);
    grid->addWidget(summaryLabel_, sumRow, 1, 1, 3);

    vbox->addWidget(box);
}

void AdvicePanel::setAdvice(const mt::AdviceSet& adv) {
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        const auto& eq = adv.eq[si];
        const auto& mb = adv.mbComp[si];
        const auto& w  = adv.width[si];
        eqLabels_   [i]->setText(QString::asprintf("%+.1f dB (Q %.2f)", eq.gainDb, eq.q));
        mbLabels_   [i]->setText(QString::asprintf("thr %.1f  %.1f:1",  mb.thresholdDb, mb.ratio));
        widthLabels_[i]->setText(QString::asprintf("%.2f",               w.width));
    }
    summaryLabel_->setText(
        QString::asprintf("Sat %.1f dB  |  MixBus thr %.1f / %.1f:1  |  Lim %.1f dBTP",
            adv.saturator.driveDb,
            adv.mixbusComp.thresholdDb, adv.mixbusComp.ratio,
            adv.limiter.ceilingDb));
}

void AdvicePanel::clear() {
    for (int i = 0; i < kNumBands; ++i) {
        eqLabels_   [i]->setText("—");
        mbLabels_   [i]->setText("—");
        widthLabels_[i]->setText("—");
    }
    summaryLabel_->setText("—");
}

} // namespace gui
