#pragma once

#include <QColor>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
QT_END_NAMESPACE

namespace gui {

// Base class for all custom audio parameter controls (knobs, faders).
// Subclasses implement paintControl() and sizeHint().
// Provides: drag-to-change (vertical), scroll-wheel, double-click-to-type,
// value label with sign, dirty-dot indicator.
class AudioControl : public QWidget {
    Q_OBJECT
public:
    explicit AudioControl(QWidget* parent = nullptr);

    void   setValue(double v);
    double value() const;
    void   setRange(double min, double max);
    void   setSingleStep(double step);
    void   setClean(double baseline);        // dirty dot shown when value ≠ baseline
    void   setSuffix(const QString& s);      // e.g. " dB", " dBTP"
    void   setValueColor(const QColor& c);   // color of the value label text
    double minimum() const { return minVal_; }
    double maximum() const { return maxVal_; }

signals:
    void valueChanged(double value);

protected:
    // Subclass paints its visual in controlRect (widget rect minus label strip).
    virtual void paintControl(QPainter& p, const QRect& controlRect) = 0;

    // Exposed so subclasses can compute correct sizeHint().
    static constexpr int kLabelH = 20;

    double  value_      = 0.0;
    double  minVal_     = 0.0;
    double  maxVal_     = 1.0;
    double  step_       = 0.1;
    double  cleanVal_   = 0.0;
    QString suffix_;
    QColor  valueColor_ = QColor("#cccccc");

private:
    void paintEvent(QPaintEvent*) override final;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;

    void clampAndEmit(double v);

    QLineEdit* lineEdit_     = nullptr;
    double     startDragVal_ = 0.0;
    int        startDragY_   = 0;
    bool       dragging_     = false;

    static constexpr double kDirtyEps     = 1e-9;
    static constexpr int    kPixelsPerStep = 2;  // drag pixels per single step
};

} // namespace gui
