#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/pipeline.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;
QT_END_NAMESPACE

namespace gui {

// Panel showing all mastering parameters derived from auto-advice.
// Spinboxes are tinted blue when their value diverges from the last
// advised value (dirty tracking).  Use setAdvice() after each analysis
// to sync the panel.  getParameters() always returns current spinbox
// values merged with the non-editable AdviceSet fields (mbComp, width).
class ParameterPanel : public QWidget {
    Q_OBJECT
public:
    explicit ParameterPanel(QWidget* parent = nullptr);

    // Populate all spinboxes with advised values, store them as clean
    // baselines, and clear all tints.
    void setAdvice(const mt::AdviceSet& advice);

    // Restore all spinboxes to the last-set clean values and clear tints.
    void resetToAdvice();

    // Returns a full AdviceSet: non-editable fields (mbComp, width) come
    // from the stored advice; editable fields come from current spinbox values.
    mt::AdviceSet getParameters() const;

    // RenderOptions populated from the bypass checkboxes.
    mt::RenderOptions getBypassOptions() const;

    // Reset spinboxes to default values and clear stored advice and tints.
    void clear();

private slots:
    void onResetClicked();

private:
    void buildUi();

    // Apply values from adv to spinboxes using blockSignals to suppress
    // dirty-tracking callbacks.
    void applyToSpins(const mt::AdviceSet& adv);

    // Recompute dirty tint for a single spinbox.
    void updateTint(QDoubleSpinBox* sp, double cleanVal);

    static constexpr int kNumBands = mt::AdviceSet::kNumBands;

    // Last advised values — used as the clean baseline.
    struct CleanValues {
        double eqGain[kNumBands]{};
        double limCeiling  = -1.0;
        double satDrive    =  0.0;
        double mixbusThresh = -20.0;
        double mixbusMakeup =  0.0;
    } cleanValues_;

    // The full last-set advice (for mbComp / width pass-through).
    mt::AdviceSet autoAdvice_;
    bool hasAdvice_ = false;

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
