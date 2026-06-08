#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/pipeline.hpp"
#include "mastertweak/preset.hpp"

#include <QCheckBox>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QGroupBox;
class QLabel;
QT_END_NAMESPACE

namespace gui {
class AudioControl;
class RotaryKnob;
class VerticalFader;
}

namespace gui {

class ChainPanel : public QWidget {
    Q_OBJECT
public:
    explicit ChainPanel(QWidget* parent = nullptr);

    // Populate all controls and inline readouts from advice + analysis + preset.
    // Stores advice as the clean baseline; widgets clear dirty dots.
    void setAdvice(const mt::AdviceSet& advice,
                   const mt::AnalysisSnapshot& snap,
                   const mt::PresetData& preset);

    // Returns current AdviceSet: non-editable fields from stored baseline,
    // editable fields from current control values.
    mt::AdviceSet currentAdvice() const;

    // Writes bypass states into opts (one flag per checkable QGroupBox).
    void populateBypassFlags(mt::RenderOptions& opts) const;

    // Restore controls to last-advised baseline and clear dirty dots.
    void resetToAdvice();

    // Reset to pre-analysis state: default values, "—" readouts.
    void clear();

    // Accessors for MIDI wiring — do not call before setAdvice() has been called.
    VerticalFader* eqGainFader(int band)  const;
    RotaryKnob*    satDriveKnob()         const;
    RotaryKnob*    mixbusThreshKnob()     const;
    RotaryKnob*    mixbusMakeupKnob()     const;
    VerticalFader* limCeilingFader()      const;

signals:
    // Emitted when any control value or bypass checkbox changes.
    void overrideChanged(const mt::AdviceSet& advice);

private:
    void buildUi();
    void applyToControls(const mt::AdviceSet& adv);
    void emitOverride();

    static constexpr int kNumBands = mt::AdviceSet::kNumBands;

    mt::AdviceSet autoAdvice_;
    bool          hasAdvice_ = false;

    // Resonance EQ section (Tier 0, above EQ)
    static constexpr int kMaxResonances = 8;
    QGroupBox*  resBox_              = nullptr;
    QLabel*     resNoResLabel_       = nullptr;     // "No resonances detected"
    QLabel*     resRowLabels_[kMaxResonances]{};    // "4127 Hz  Q:8.3  -6.2 dB"
    QCheckBox*  resRowChecks_[kMaxResonances]{};    // per-resonance enable/disable

    // EQ section (Tier 1)
    QGroupBox*     eqBox_                = nullptr;
    QLabel*        eqReadouts_[kNumBands]{};
    VerticalFader* eqGainFaders_[kNumBands]{};

    // Multiband Comp section (Tier 2, col 0-1)
    QGroupBox* mbBox_      = nullptr;
    QLabel*    mbCrestLbl_ = nullptr;

    // Stereo Width section (Tier 2, col 2-3)
    QGroupBox* widthBox_     = nullptr;
    QLabel*    widthCorrLbl_ = nullptr;

    // Saturator section (Tier 2, col 4-5)
    QGroupBox*  satBox_       = nullptr;
    QLabel*     satCrestLbl_  = nullptr;
    RotaryKnob* satDriveKnob_ = nullptr;

    // Mixbus Comp section (Tier 3, col 0-3)
    QGroupBox*  mixbusBox_        = nullptr;
    RotaryKnob* mixbusThreshKnob_ = nullptr;
    RotaryKnob* mixbusMakeupKnob_ = nullptr;
    QLabel*     mixbusRmsLbl_     = nullptr;

    // Limiter section (Tier 3, col 4-5)
    QGroupBox*     limBox_          = nullptr;
    VerticalFader* limCeilingFader_ = nullptr;
    QLabel*        limPeakLbl_      = nullptr;
};

} // namespace gui
