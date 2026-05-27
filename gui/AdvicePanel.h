#pragma once

#include "mastertweak/advice.hpp"

#include <QWidget>
#include <optional>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace gui {

// Read-only display of the derived AdviceSet (EQ gains, comp thresholds, etc.).
class AdvicePanel : public QWidget {
    Q_OBJECT
public:
    explicit AdvicePanel(QWidget* parent = nullptr);

    void setAdvice(const mt::AdviceSet& advice);
    void clear();

private:
    void buildUi();

    static constexpr int kNumBands = mt::AdviceSet::kNumBands;

    QLabel* eqLabels_  [kNumBands]{};
    QLabel* mbLabels_  [kNumBands]{};
    QLabel* widthLabels_[kNumBands]{};
    QLabel* summaryLabel_ = nullptr;
};

} // namespace gui
