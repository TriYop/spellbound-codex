#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/pipeline.hpp"

#include <QWidget>
#include <optional>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;
QT_END_NAMESPACE

namespace gui {

// Collapsible panel that lets the user tweak key advice parameters before rendering.
// When the groupbox is unchecked, getAdviceOverride() returns nullopt and the
// pipeline re-derives advice normally. When checked, it returns a copy of the
// last auto-advice with the user's edits applied.
// Stage bypass checkboxes live here so they are co-located with the override values.
class ParameterOverridePanel : public QWidget {
    Q_OBJECT
public:
    explicit ParameterOverridePanel(QWidget* parent = nullptr);

    // Called after each analysis so spinboxes stay in sync with auto values.
    void setAutoAdvice(const mt::AdviceSet& advice);
    void clear();

    // nullopt when overrides checkbox is off; modified advice otherwise.
    std::optional<mt::AdviceSet> getAdviceOverride() const;

    // RenderOptions with bypass flags from the stage bypass checkboxes.
    mt::RenderOptions getBypassOptions() const;

private slots:
    void onResetToAuto();

private:
    void buildUi();
    void applyAdviceToSpins(const mt::AdviceSet& adv);

    static constexpr int kNumBands = mt::AdviceSet::kNumBands;

    std::optional<mt::AdviceSet> autoAdvice_;

    QGroupBox*      groupBox_         = nullptr;
    QDoubleSpinBox* eqGainSpins_[kNumBands]{};
    QDoubleSpinBox* limCeilingSpin_   = nullptr;
    QDoubleSpinBox* satDriveSpin_     = nullptr;
    QDoubleSpinBox* mixbusThreshSpin_ = nullptr;
    QDoubleSpinBox* mixbusMakeupSpin_ = nullptr;
    QPushButton*    resetBtn_         = nullptr;

    QCheckBox* bypassEqChk_      = nullptr;
    QCheckBox* bypassMbChk_      = nullptr;
    QCheckBox* bypassSatChk_     = nullptr;
    QCheckBox* bypassWidthChk_   = nullptr;
    QCheckBox* bypassMixbusChk_  = nullptr;
    QCheckBox* bypassLimiterChk_ = nullptr;
    QCheckBox* bypassDitherChk_  = nullptr;
};

} // namespace gui
