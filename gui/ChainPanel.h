#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/pipeline.hpp"
#include "mastertweak/preset.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
QT_END_NAMESPACE

namespace gui {

class ChainPanel : public QWidget {
    Q_OBJECT
public:
    explicit ChainPanel(QWidget* parent = nullptr);

    // Populate all controls and inline readouts from advice + analysis + preset.
    // Stores advice as the clean baseline; clears all dirty tints.
    void setAdvice(const mt::AdviceSet& advice,
                   const mt::AnalysisSnapshot& snap,
                   const mt::PresetData& preset);

    // Returns current AdviceSet: non-editable fields from stored baseline,
    // editable fields from current spinbox values.
    mt::AdviceSet currentAdvice() const;

    // Writes bypass states into opts (one flag per checkable QGroupBox).
    void populateBypassFlags(mt::RenderOptions& opts) const;

    // Restore spinboxes to last-advised baseline and clear dirty tints.
    void resetToAdvice();

    // Reset to pre-analysis state: default spinbox values, "—" readouts.
    void clear();

signals:
    // Emitted when any spinbox value or bypass checkbox changes.
    void overrideChanged(const mt::AdviceSet& advice);

private:
    void buildUi();
    void applyToSpins(const mt::AdviceSet& adv);
    void updateTint(QDoubleSpinBox* sp, double cleanVal);
    void clearAllTints();
    void emitOverride();

    static constexpr int kNumBands = mt::AdviceSet::kNumBands;

    struct CleanValues {
        double eqGain[kNumBands]{};
        double limCeiling   = -1.0;
        double satDrive     =  0.0;
        double mixbusThresh = -20.0;
        double mixbusMakeup =  0.0;
    } cleanValues_;

    mt::AdviceSet autoAdvice_;
    bool          hasAdvice_ = false;

    // EQ section (Tier 1)
    QGroupBox*      eqBox_               = nullptr;
    QLabel*         eqReadouts_[kNumBands]{};
    QDoubleSpinBox* eqGainSpins_[kNumBands]{};

    // Multiband Comp section (Tier 2, col 0-1)
    QGroupBox* mbBox_      = nullptr;
    QLabel*    mbCrestLbl_ = nullptr;

    // Stereo Width section (Tier 2, col 2-3)
    QGroupBox* widthBox_     = nullptr;
    QLabel*    widthCorrLbl_ = nullptr;

    // Saturator section (Tier 2, col 4-5)
    QGroupBox*      satBox_      = nullptr;
    QLabel*         satCrestLbl_ = nullptr;
    QDoubleSpinBox* satDriveSpin_ = nullptr;

    // Mixbus Comp section (Tier 3, col 0-3)
    QGroupBox*      mixbusBox_        = nullptr;
    QDoubleSpinBox* mixbusThreshSpin_ = nullptr;
    QDoubleSpinBox* mixbusMakeupSpin_ = nullptr;
    QLabel*         mixbusRmsLbl_     = nullptr;

    // Limiter section (Tier 3, col 4-5)
    QGroupBox*      limBox_        = nullptr;
    QDoubleSpinBox* limCeilingSpin_ = nullptr;
    QLabel*         limPeakLbl_    = nullptr;
};

} // namespace gui
