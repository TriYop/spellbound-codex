#include "VUMeterWidget.h"

#include <QTimer>
#include <QPainter>
#include <cmath>

namespace gui {

VUMeterWidget::VUMeterWidget(const std::atomic<float>* rmsL,
                             const std::atomic<float>* rmsR,
                             QWidget* parent)
    : QWidget(parent), atomicRmsL_(rmsL), atomicRmsR_(rmsR)
{
    alpha_ = 1.f - std::exp(-1.f / (static_cast<float>(kTickHz) * kTau));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    timer_ = new QTimer(this);
    timer_->setInterval(1000 / kTickHz);
    connect(timer_, &QTimer::timeout, this, &VUMeterWidget::onTick);
    timer_->start();
}

QSize VUMeterWidget::sizeHint() const {
    return QSize(kPad * 2 + kBarW * 2 + kGap + kScaleW, 80);
}

QSize VUMeterWidget::minimumSizeHint() const { return sizeHint(); }

float VUMeterWidget::vuFromRms(float rms) {
    if (rms < kRmsFloor) rms = kRmsFloor;
    float vu = 20.f * std::log10(rms) + 18.f;
    if (vu < kVuMin) vu = kVuMin;
    if (vu > kVuMax) vu = kVuMax;
    return vu;
}

void VUMeterWidget::onTick() {
    if (atomicRmsL_) smoothedL_ = alpha_ * atomicRmsL_->load(std::memory_order_relaxed) + (1.f - alpha_) * smoothedL_;
    if (atomicRmsR_) smoothedR_ = alpha_ * atomicRmsR_->load(std::memory_order_relaxed) + (1.f - alpha_) * smoothedR_;
    update();
}

void VUMeterWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const int barH = height() - kLabelH;

    p.fillRect(rect(), QColor("#1a1a1a"));

    // Zone thresholds in pixel-y space.
    // y=0 = top = +3 VU; y=barH = bottom = -20 VU.
    // Formula: y = (kVuMax - vu) / (kVuMax - kVuMin) * barH
    const float range      = kVuMax - kVuMin;  // 23 VU
    const int   redThresh  = static_cast<int>((kVuMax -  0.f) / range * barH);  // y at 0 VU
    const int   ylwThresh  = static_cast<int>((kVuMax - -3.f) / range * barH);  // y at -3 VU

    auto drawBar = [&](int x, float rms, const char* label) {
        float vu    = vuFromRms(rms);
        float norm  = (vu - kVuMin) / range;           // 0 = -20 VU, 1 = +3 VU
        int   fillH = static_cast<int>(norm * barH);
        int   fillY = barH - fillH;                    // top pixel of filled region

        // Green zone: ylwThresh..barH — only the part inside the fill
        {
            int top = std::max(fillY, ylwThresh);
            if (top < barH)
                p.fillRect(x, top, kBarW, barH - top, QColor("#00aa00"));
        }
        // Yellow zone: redThresh..ylwThresh
        if (fillY < ylwThresh) {
            int top = std::max(fillY, redThresh);
            if (top < ylwThresh)
                p.fillRect(x, top, kBarW, ylwThresh - top, QColor("#cccc00"));
        }
        // Red zone: fillY..redThresh
        if (fillY < redThresh)
            p.fillRect(x, fillY, kBarW, redThresh - fillY, QColor("#cc0000"));

        // 0 VU reference line
        p.setPen(QPen(QColor("#ffffff"), 2));
        p.drawLine(x, redThresh, x + kBarW - 1, redThresh);

        // Channel label
        p.setPen(QColor("#aaaaaa"));
        p.setFont(QFont("sans-serif", 8));
        p.drawText(QRect(x, barH, kBarW, kLabelH), Qt::AlignCenter, QString(label));
    };

    drawBar(kPad,                    smoothedL_, "L");
    drawBar(kPad + kBarW + kGap,     smoothedR_, "R");

    // Scale ticks to the right of the R bar
    const int tickX = kPad + kBarW * 2 + kGap + 2;
    struct Tick { float vu; const char* lbl; bool thick; };
    const Tick ticks[] = {
        {  3.f, "+3",  false },
        {  0.f,  "0",  true  },
        { -3.f, "-3",  false },
        {-10.f,"-10",  false },
        {-20.f,"-20",  false },
    };
    p.setFont(QFont("sans-serif", 7));
    for (const auto& t : ticks) {
        int y = static_cast<int>((kVuMax - t.vu) / range * barH);
        p.setPen(QPen(t.thick ? QColor("#ffffff") : QColor("#555555"), t.thick ? 2 : 1));
        p.drawLine(tickX, y, tickX + 3, y);
        p.setPen(QColor("#777777"));
        p.drawText(tickX + 5, y + 4, t.lbl);
    }
}

} // namespace gui
