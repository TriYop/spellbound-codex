#pragma once

#include "mastertweak/analysis.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QToolButton;
QT_END_NAMESPACE

namespace gui {

// Read-only display of per-band analysis results (RMS, peak, correlation, crest).
class AnalysisPanel : public QWidget {
    Q_OBJECT
public:
    explicit AnalysisPanel(QWidget* parent = nullptr);

    void setSnapshot(const mt::AnalysisSnapshot& snap);
    void clear();

private slots:
    void onToggle();

private:
    void buildUi();

    static constexpr int kNumBands = mt::AnalysisSnapshot::kNumBands;

    QWidget*     contentWidget_ = nullptr;
    QToolButton* toggleBtn_     = nullptr;
    bool         collapsed_     = true;

    QLabel* avgLabels_  [kNumBands]{};
    QLabel* peakLabels_ [kNumBands]{};
    QLabel* corrLabels_ [kNumBands]{};
    QLabel* crestLabels_[kNumBands]{};
    QLabel* overallLabel_ = nullptr;
};

} // namespace gui
