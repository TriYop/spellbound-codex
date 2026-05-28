#pragma once

#include "AudioControl.h"

namespace gui {

// Vertical fader: track with a thumb. Bottom = min, top = max.
class VerticalFader : public AudioControl {
    Q_OBJECT
public:
    explicit VerticalFader(QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintControl(QPainter& p, const QRect& r) override;
};

} // namespace gui
