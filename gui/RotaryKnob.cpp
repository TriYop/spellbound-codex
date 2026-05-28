#include "RotaryKnob.h"

#include <cmath>

#include <QPainter>

namespace gui {

static constexpr int kArcStartQt = 225 * 16;   // 225° CCW from 3 o'clock = ~7:30 position
static constexpr int kArcSpanQt  = -270 * 16;  // 270° CW sweep through 12 o'clock

RotaryKnob::RotaryKnob(QWidget* parent)
    : AudioControl(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize RotaryKnob::sizeHint() const {
    return QSize(56, 56 + kLabelH);
}

void RotaryKnob::paintControl(QPainter& p, const QRect& r) {
    const double norm = (maxVal_ > minVal_)
                        ? (value_ - minVal_) / (maxVal_ - minVal_)
                        : 0.0;

    const int cx     = r.center().x();
    const int cy     = r.center().y();
    const int radius = std::min(r.width(), r.height()) / 2 - 4;
    const QRect arcRect(cx - radius, cy - radius, radius * 2, radius * 2);

    // Background groove arc
    QPen arcPen(QColor("#cccccc"), 4, Qt::SolidLine, Qt::FlatCap);
    p.setPen(arcPen);
    p.setBrush(Qt::NoBrush);
    p.drawArc(arcRect, kArcStartQt, kArcSpanQt);

    // Value arc (filled proportion)
    arcPen.setColor(QColor("#2a6099"));
    p.setPen(arcPen);
    const int valSpan = static_cast<int>(norm * static_cast<double>(kArcSpanQt));
    p.drawArc(arcRect, kArcStartQt, valSpan);

    // Centre circle
    p.setBrush(QColor("#3a3a3a"));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(cx, cy), 12, 12);

    // Pointer line: at norm=0 → 225°, norm=0.5 → 90° (12 o'clock), norm=1 → -45° (315°)
    const double angleDeg = 225.0 - norm * 270.0;
    const double angleRad = angleDeg * M_PI / 180.0;
    const int px = cx + static_cast<int>(static_cast<double>(radius - 2) * std::cos(angleRad));
    const int py = cy - static_cast<int>(static_cast<double>(radius - 2) * std::sin(angleRad));
    p.setPen(QPen(QColor("#ffffff"), 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(cx, cy, px, py);
}

} // namespace gui
