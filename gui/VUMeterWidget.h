#pragma once

#include <QWidget>
#include <atomic>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace gui {

class VUMeterWidget : public QWidget {
    Q_OBJECT
public:
    explicit VUMeterWidget(const std::atomic<float>* rmsL,
                           const std::atomic<float>* rmsR,
                           QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private slots:
    void onTick();

private:
    static float vuFromRms(float rms);

    static constexpr float kVuMin    = -20.f;
    static constexpr float kVuMax    =   3.f;
    static constexpr float kTau      =   0.300f;  // 300 ms VU ballistics
    static constexpr int   kTickHz   =  30;
    static constexpr float kRmsFloor =  1e-7f;
    static constexpr int   kBarW     =  14;
    static constexpr int   kGap      =   4;
    static constexpr int   kPad      =   4;
    static constexpr int   kScaleW   =  20;
    static constexpr int   kLabelH   =  14;

    const std::atomic<float>* atomicRmsL_ = nullptr;
    const std::atomic<float>* atomicRmsR_ = nullptr;

    float   smoothedL_ = 0.f;
    float   smoothedR_ = 0.f;
    float   alpha_     = 0.f;

    QTimer* timer_ = nullptr;
};

} // namespace gui
