#pragma once

#include "AudioControl.h"

namespace gui {

// Rotary knob: 270° arc from 7 o'clock (min) to 5 o'clock (max).
class RotaryKnob : public AudioControl {
    Q_OBJECT
public:
    explicit RotaryKnob(QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintControl(QPainter& p, const QRect& r) override;
};

} // namespace gui
