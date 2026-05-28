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
    smoothedL_ = alpha_ * atomicRmsL_->load(std::memory_order_relaxed) + (1.f - alpha_) * smoothedL_;
    smoothedR_ = alpha_ * atomicRmsR_->load(std::memory_order_relaxed) + (1.f - alpha_) * smoothedR_;
    update();
}

void VUMeterWidget::paintEvent(QPaintEvent*) {
    // Placeholder — full implementation in Task 2
    QPainter p(this);
    p.fillRect(rect(), QColor("#1a1a1a"));
}

} // namespace gui
