#include "ParameterPanel.h"

#include "mastertweak/analysis.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace gui {

ParameterPanel::ParameterPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void ParameterPanel::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    groupBox_ = new QGroupBox("Parameters", this);
    // Non-checkable — always visible.
    outer->addWidget(groupBox_);

    auto* vbox = new QVBoxLayout(groupBox_);

    // ── Per-band EQ gains ──────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("EQ (dB):"));
        for (int i = 0; i < kNumBands; ++i) {
            const auto si = static_cast<size_t>(i);
            auto* col = new QVBoxLayout;
            col->setSpacing(2);
            col->addWidget(new QLabel(mt::AnalysisSnapshot::kBandNames[si]));
            auto* sp = new QDoubleSpinBox;
            sp->setRange(-12.0, 12.0);
            sp->setSingleStep(0.5);
            sp->setDecimals(1);
            sp->setValue(0.0);
            sp->setFixedWidth(64);
            eqGainSpins_[i] = sp;
            col->addWidget(sp);
            row->addLayout(col);

            const int captured_i = i;
            connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this, captured_i](double val) {
                        updateTint(eqGainSpins_[captured_i],
                                   cleanValues_.eqGain[captured_i]);
                        (void)val;
                    });
        }
        row->addStretch();
        vbox->addLayout(row);
    }

    // ── Global parameter row ───────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;

        auto addSpin = [&](const QString& label, QDoubleSpinBox** ptr,
                           double lo, double hi, double step, double def,
                           const QString& suffix = {}) {
            row->addWidget(new QLabel(label));
            auto* sp = new QDoubleSpinBox;
            sp->setRange(lo, hi);
            sp->setSingleStep(step);
            sp->setDecimals(1);
            sp->setValue(def);
            if (!suffix.isEmpty()) sp->setSuffix(suffix);
            sp->setFixedWidth(80);
            *ptr = sp;
            row->addWidget(sp);
            row->addSpacing(12);
        };

        addSpin("Lim ceiling:",   &limCeilingSpin_,    -6.0,   0.0, 0.5, -1.0,  " dBTP");
        addSpin("Sat drive:",     &satDriveSpin_,       0.0,   6.0, 0.5,  0.0,  " dB");
        addSpin("MixBus thr:",    &mixbusThreshSpin_, -40.0,   0.0, 1.0, -20.0, " dB");
        addSpin("MixBus makeup:", &mixbusMakeupSpin_, -12.0,  12.0, 0.5,  0.0,  " dB");

        connect(limCeilingSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(limCeilingSpin_, cleanValues_.limCeiling);
                });
        connect(satDriveSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(satDriveSpin_, cleanValues_.satDrive);
                });
        connect(mixbusThreshSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(mixbusThreshSpin_, cleanValues_.mixbusThresh);
                });
        connect(mixbusMakeupSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(mixbusMakeupSpin_, cleanValues_.mixbusMakeup);
                });

        resetBtn_ = new QPushButton("Reset to advice");
        connect(resetBtn_, &QPushButton::clicked,
                this, &ParameterPanel::onResetClicked);
        row->addWidget(resetBtn_);
        row->addStretch();
        vbox->addLayout(row);
    }

    // ── Stage bypass checkboxes ────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("Bypass:"));

        auto addChk = [&](const QString& label, QCheckBox** ptr) {
            auto* chk = new QCheckBox(label);
            *ptr = chk;
            row->addWidget(chk);
        };

        addChk("EQ",        &bypassEqChk_);
        addChk("Multiband", &bypassMbChk_);
        addChk("Saturator", &bypassSatChk_);
        addChk("Width",     &bypassWidthChk_);
        addChk("MixBus",    &bypassMixbusChk_);
        addChk("Limiter",   &bypassLimiterChk_);
        addChk("Dither",    &bypassDitherChk_);
        row->addStretch();
        vbox->addLayout(row);
    }
}

// ──────────────────────────────────────────────────────────────────────────────

void ParameterPanel::applyToSpins(const mt::AdviceSet& adv) {
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        eqGainSpins_[i]->blockSignals(true);
        eqGainSpins_[i]->setValue(static_cast<double>(adv.eq[si].gainDb));
        eqGainSpins_[i]->blockSignals(false);
    }
    limCeilingSpin_->blockSignals(true);
    limCeilingSpin_->setValue(static_cast<double>(adv.limiter.ceilingDb));
    limCeilingSpin_->blockSignals(false);

    satDriveSpin_->blockSignals(true);
    satDriveSpin_->setValue(static_cast<double>(adv.saturator.driveDb));
    satDriveSpin_->blockSignals(false);

    mixbusThreshSpin_->blockSignals(true);
    mixbusThreshSpin_->setValue(static_cast<double>(adv.mixbusComp.thresholdDb));
    mixbusThreshSpin_->blockSignals(false);

    mixbusMakeupSpin_->blockSignals(true);
    mixbusMakeupSpin_->setValue(static_cast<double>(adv.mixbusComp.makeupDb));
    mixbusMakeupSpin_->blockSignals(false);
}

void ParameterPanel::updateTint(QDoubleSpinBox* sp, double cleanVal) {
    constexpr double kEps = 1e-9;
    if (std::abs(sp->value() - cleanVal) > kEps)
        sp->setStyleSheet("background-color: #d0e8ff;");
    else
        sp->setStyleSheet("");
}

void ParameterPanel::setAdvice(const mt::AdviceSet& advice) {
    autoAdvice_ = advice;
    hasAdvice_  = true;

    // Store clean baselines.
    for (int i = 0; i < kNumBands; ++i)
        cleanValues_.eqGain[i] = static_cast<double>(advice.eq[static_cast<size_t>(i)].gainDb);
    cleanValues_.limCeiling   = static_cast<double>(advice.limiter.ceilingDb);
    cleanValues_.satDrive     = static_cast<double>(advice.saturator.driveDb);
    cleanValues_.mixbusThresh = static_cast<double>(advice.mixbusComp.thresholdDb);
    cleanValues_.mixbusMakeup = static_cast<double>(advice.mixbusComp.makeupDb);

    // Populate spinboxes and clear all tints.
    applyToSpins(advice);
    for (int i = 0; i < kNumBands; ++i)
        eqGainSpins_[i]->setStyleSheet("");
    limCeilingSpin_->setStyleSheet("");
    satDriveSpin_->setStyleSheet("");
    mixbusThreshSpin_->setStyleSheet("");
    mixbusMakeupSpin_->setStyleSheet("");
}

void ParameterPanel::resetToAdvice() {
    if (!hasAdvice_) return;
    applyToSpins(autoAdvice_);
    for (int i = 0; i < kNumBands; ++i)
        eqGainSpins_[i]->setStyleSheet("");
    limCeilingSpin_->setStyleSheet("");
    satDriveSpin_->setStyleSheet("");
    mixbusThreshSpin_->setStyleSheet("");
    mixbusMakeupSpin_->setStyleSheet("");
}

void ParameterPanel::onResetClicked() {
    resetToAdvice();
}

mt::AdviceSet ParameterPanel::getParameters() const {
    // Start from stored advice so mbComp/width arrays are preserved.
    mt::AdviceSet adv = autoAdvice_;

    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        adv.eq[si].gainDb = static_cast<float>(eqGainSpins_[i]->value());
    }
    adv.limiter.ceilingDb      = static_cast<float>(limCeilingSpin_->value());
    adv.saturator.driveDb      = static_cast<float>(satDriveSpin_->value());
    adv.mixbusComp.thresholdDb = static_cast<float>(mixbusThreshSpin_->value());
    adv.mixbusComp.makeupDb    = static_cast<float>(mixbusMakeupSpin_->value());
    return adv;
}

mt::RenderOptions ParameterPanel::getBypassOptions() const {
    mt::RenderOptions opts;
    opts.bypassEq         = bypassEqChk_->isChecked();
    opts.bypassMbComp     = bypassMbChk_->isChecked();
    opts.bypassSaturator  = bypassSatChk_->isChecked();
    opts.bypassWidth      = bypassWidthChk_->isChecked();
    opts.bypassMixbusComp = bypassMixbusChk_->isChecked();
    opts.bypassLimiter    = bypassLimiterChk_->isChecked();
    opts.bypassDither     = bypassDitherChk_->isChecked();
    return opts;
}

void ParameterPanel::clear() {
    hasAdvice_ = false;
    autoAdvice_ = mt::AdviceSet{};  // reset to default

    // Reset clean baselines to defaults.
    for (int i = 0; i < kNumBands; ++i) cleanValues_.eqGain[i] = 0.0;
    cleanValues_.limCeiling   = -1.0;
    cleanValues_.satDrive     =  0.0;
    cleanValues_.mixbusThresh = -20.0;
    cleanValues_.mixbusMakeup =  0.0;

    // Reset spinboxes to defaults, suppressing dirty callbacks.
    for (int i = 0; i < kNumBands; ++i) {
        eqGainSpins_[i]->blockSignals(true);
        eqGainSpins_[i]->setValue(0.0);
        eqGainSpins_[i]->blockSignals(false);
        eqGainSpins_[i]->setStyleSheet("");
    }
    limCeilingSpin_->blockSignals(true);
    limCeilingSpin_->setValue(-1.0);
    limCeilingSpin_->blockSignals(false);
    limCeilingSpin_->setStyleSheet("");

    satDriveSpin_->blockSignals(true);
    satDriveSpin_->setValue(0.0);
    satDriveSpin_->blockSignals(false);
    satDriveSpin_->setStyleSheet("");

    mixbusThreshSpin_->blockSignals(true);
    mixbusThreshSpin_->setValue(-20.0);
    mixbusThreshSpin_->blockSignals(false);
    mixbusThreshSpin_->setStyleSheet("");

    mixbusMakeupSpin_->blockSignals(true);
    mixbusMakeupSpin_->setValue(0.0);
    mixbusMakeupSpin_->blockSignals(false);
    mixbusMakeupSpin_->setStyleSheet("");
}

} // namespace gui
