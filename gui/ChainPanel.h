#pragma once

#include "mastertweak/advice.hpp"
#include "mastertweak/analysis.hpp"
#include "mastertweak/pipeline.hpp"
#include "mastertweak/preset.hpp"

#include <QCheckBox>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace gui {
class AudioControl;
class RackUnit;
class RotaryKnob;
}

namespace gui {

class ChainPanel : public QWidget {
    Q_OBJECT
public:
    explicit ChainPanel(QWidget* parent = nullptr);

    void setAdvice(const mt::AdviceSet& advice,
                   const mt::AnalysisSnapshot& snap,
                   const mt::PresetData& preset);

    mt::AdviceSet currentAdvice() const;
    void populateBypassFlags(mt::RenderOptions& opts) const;
    void resetToAdvice();
    void clear();

    // Original MIDI accessors (wired in MainWindow)
    RotaryKnob*    eqGainFader(int band)  const;
    RotaryKnob*    satDriveKnob()         const;
    RotaryKnob*    mixbusThreshKnob()     const;
    RotaryKnob*    mixbusMakeupKnob()     const;
    RotaryKnob*    limCeilingFader()      const;

    // New MIDI accessors (declared for future wiring — not connected in MainWindow yet)
    RotaryKnob*    mbRatioKnob(int band)   const;
    RotaryKnob*    mbThreshFader(int band) const;
    RotaryKnob*    widthFader(int band)    const;
    RotaryKnob*    limTargetKnob()         const;
    RotaryKnob*    mixbusRatioKnob()       const;

signals:
    void overrideChanged(const mt::AdviceSet& advice);

private:
    void buildUi();
    void applyToControls(const mt::AdviceSet& adv);
    void emitOverride();

    static constexpr int kNumBands      = mt::AdviceSet::kNumBands;
    static constexpr int kMaxResonances = 8;

    mt::AdviceSet autoAdvice_;
    bool          hasAdvice_ = false;

    // Resonance EQ
    RackUnit*  resUnit_              = nullptr;
    QLabel*    resNoResLabel_        = nullptr;
    QLabel*    resRowLabels_[kMaxResonances]{};
    QCheckBox* resRowChecks_[kMaxResonances]{};

    // Parametric EQ
    RackUnit*   eqUnit_               = nullptr;
    QLabel*     eqReadouts_[kNumBands]{};
    RotaryKnob* eqGainFaders_[kNumBands]{};

    // Multiband Comp
    RackUnit*   mbUnit_               = nullptr;
    RotaryKnob* mbThreshFaders_[kNumBands]{};
    RotaryKnob* mbRatioKnobs_[kNumBands]{};

    // Saturator
    RackUnit*   satUnit_       = nullptr;
    RotaryKnob* satDriveKnob_  = nullptr;

    // Stereo Width
    RackUnit*   widthUnit_            = nullptr;
    RotaryKnob* widthFaders_[kNumBands]{};

    // Mixbus Comp
    RackUnit*   mixbusUnit_        = nullptr;
    RotaryKnob* mixbusThreshKnob_  = nullptr;
    RotaryKnob* mixbusRatioKnob_   = nullptr;
    RotaryKnob* mixbusMakeupKnob_  = nullptr;

    // Limiter
    RackUnit*   limUnit_          = nullptr;
    RotaryKnob* limTargetKnob_    = nullptr;
    RotaryKnob* limCeilingFader_  = nullptr;
};

} // namespace gui
