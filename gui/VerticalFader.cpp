#include "VerticalFader.h"

#include <QPainter>

namespace gui {

static constexpr int kThumbW = 20;
static constexpr int kThumbH = 10;
static constexpr int kTrackW =  3;

VerticalFader::VerticalFader(QWidget* parent)
    : AudioControl(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
}

QSize VerticalFader::sizeHint() const {
    return QSize(30, 80 + kLabelH);
}

void VerticalFader::paintControl(QPainter& p, const QRect& r) {
    const double norm = (maxVal_ > minVal_)
                        ? (value_ - minVal_) / (maxVal_ - minVal_)
                        : 0.0;

    const int cx = r.center().x();

    // Vertical groove
    const int trackX = cx - kTrackW / 2;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#cccccc"));
    p.drawRect(trackX, r.top() + kThumbH / 2, kTrackW, r.height() - kThumbH);

    // Zero-crossing tick (if range spans 0)
    if (minVal_ < 0.0 && maxVal_ > 0.0) {
        const double normZero = -minVal_ / (maxVal_ - minVal_);
        const int yZero = r.bottom() - static_cast<int>(normZero * static_cast<double>(r.height()));
        p.setPen(QPen(QColor("#888888"), 1));
        p.drawLine(cx - 6, yZero, cx + 6, yZero);
    }

    // Thumb: rounded rectangle centered on current value position
    const int thumbY = r.bottom()
                     - static_cast<int>(norm * static_cast<double>(r.height()))
                     - kThumbH / 2;
    const QRect thumbRect(cx - kThumbW / 2, thumbY, kThumbW, kThumbH);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#2a6099"));
    p.drawRoundedRect(thumbRect, 3, 3);
}

} // namespace gui
