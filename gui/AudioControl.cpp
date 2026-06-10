#include "AudioControl.h"

#include <algorithm>
#include <cmath>

#include <QFont>
#include <QFontMetrics>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace gui {

AudioControl::AudioControl(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::SizeVerCursor);

    lineEdit_ = new QLineEdit(this);
    lineEdit_->hide();
    lineEdit_->setAlignment(Qt::AlignCenter);

    // Hide on Enter or focus-lost; validate and apply value.
    connect(lineEdit_, &QLineEdit::editingFinished, this, [this]() {
        if (!lineEdit_->isVisible()) return;  // guard against double-fire
        const QString text = lineEdit_->text().trimmed();
        lineEdit_->hide();
        if (text.isEmpty()) return;  // Escape or empty — discard
        bool ok = false;
        const double parsed = text.toDouble(&ok);
        if (ok) clampAndEmit(parsed);
    });
}

void AudioControl::setValue(double v) {
    value_ = std::clamp(v, minVal_, maxVal_);
    update();
}

double AudioControl::value() const { return value_; }

void AudioControl::setRange(double min, double max) {
    minVal_ = min;
    maxVal_ = max;
    value_  = std::clamp(value_, minVal_, maxVal_);
    update();
}

void AudioControl::setSingleStep(double step) { step_ = step; }

void AudioControl::setClean(double baseline) {
    cleanVal_ = baseline;
    update();
}

void AudioControl::setSuffix(const QString& s) {
    suffix_ = s;
    update();
}

void AudioControl::setValueColor(const QColor& c) {
    valueColor_ = c;
    update();
}

void AudioControl::clampAndEmit(double v) {
    value_ = std::clamp(v, minVal_, maxVal_);
    update();
    emit valueChanged(value_);
}

// ─────────────────────────────────────────────────────────────────────────────

void AudioControl::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Delegate visual drawing to subclass
    const QRect controlRect(0, 0, width(), height() - kLabelH);
    paintControl(p, controlRect);

    // Value label strip at the bottom
    const QRect labelRect(0, height() - kLabelH, width(), kLabelH);

    // "+3.0 dB" / "-1.0 dBTP" / "0.0 dB"
    const QString valStr = (value_ > 0.0 ? "+" : "") +
                           QString::number(value_, 'f', 1) + suffix_;

    p.setPen(valueColor_);
    QFont lf = font();
    lf.setPointSize(8);
    p.setFont(lf);
    const QFontMetrics fm(lf);
    const int textW = fm.horizontalAdvance(valStr);
    const int textX = (width() - textW) / 2;
    const int textY = labelRect.top() + (kLabelH + fm.ascent() - fm.descent()) / 2;
    p.drawText(textX, textY, valStr);

    // Blue dot to the right of the text when value ≠ clean baseline
    if (std::abs(value_ - cleanVal_) > kDirtyEps) {
        p.setBrush(QColor("#4a90e2"));
        p.setPen(Qt::NoPen);
        const int dotX = textX + textW + 4;
        const int dotY = textY - fm.ascent() / 2;
        p.drawEllipse(dotX, dotY, 6, 6);
    }
}

void AudioControl::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        startDragY_   = e->pos().y();
        startDragVal_ = value_;
        dragging_     = true;
        setFocus();
    }
}

void AudioControl::mouseMoveEvent(QMouseEvent* e) {
    if (!dragging_) return;
    const double delta = static_cast<double>(startDragY_ - e->pos().y());
    clampAndEmit(startDragVal_ + delta * step_ / static_cast<double>(kPixelsPerStep));
}

void AudioControl::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton)
        dragging_ = false;
}

void AudioControl::wheelEvent(QWheelEvent* e) {
    clampAndEmit(value_ + e->angleDelta().y() / 120.0 * step_);
    e->accept();
}

void AudioControl::mouseDoubleClickEvent(QMouseEvent*) {
    lineEdit_->setGeometry(0, height() - kLabelH, width(), kLabelH);
    lineEdit_->setText(QString::number(value_, 'f', 1));
    lineEdit_->show();
    lineEdit_->setFocus();
    lineEdit_->selectAll();
}

} // namespace gui
