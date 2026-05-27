#include "ParameterOverridePanel.h"

#include "mastertweak/analysis.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace gui {

ParameterOverridePanel::ParameterOverridePanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void ParameterOverridePanel::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    groupBox_ = new QGroupBox("Manual Overrides", this);
    groupBox_->setCheckable(true);
    groupBox_->setChecked(false);
    groupBox_->setToolTip("Enable to override auto-derived advice before rendering");
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

        addSpin("Lim ceiling:",  &limCeilingSpin_,    -6.0,   0.0, 0.5, -1.0, " dBTP");
        addSpin("Sat drive:",    &satDriveSpin_,        0.0,   6.0, 0.5,  0.0, " dB");
        addSpin("MixBus thr:",   &mixbusThreshSpin_,  -40.0,   0.0, 1.0, -20.0, " dB");
        addSpin("MixBus makeup:", &mixbusMakeupSpin_, -12.0,  12.0, 0.5,  0.0, " dB");

        resetBtn_ = new QPushButton("Reset to auto");
        connect(resetBtn_, &QPushButton::clicked,
                this, &ParameterOverridePanel::onResetToAuto);
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

void ParameterOverridePanel::setAutoAdvice(const mt::AdviceSet& advice) {
    autoAdvice_ = advice;
    if (!groupBox_->isChecked())
        applyAdviceToSpins(advice);
}

void ParameterOverridePanel::clear() {
    autoAdvice_.reset();
    groupBox_->setChecked(false);
    for (int i = 0; i < kNumBands; ++i) eqGainSpins_[i]->setValue(0.0);
    limCeilingSpin_->setValue(-1.0);
    satDriveSpin_->setValue(0.0);
    mixbusThreshSpin_->setValue(-20.0);
    mixbusMakeupSpin_->setValue(0.0);
}

void ParameterOverridePanel::applyAdviceToSpins(const mt::AdviceSet& adv) {
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        eqGainSpins_[i]->setValue(static_cast<double>(adv.eq[si].gainDb));
    }
    limCeilingSpin_->setValue(static_cast<double>(adv.limiter.ceilingDb));
    satDriveSpin_->setValue(static_cast<double>(adv.saturator.driveDb));
    mixbusThreshSpin_->setValue(static_cast<double>(adv.mixbusComp.thresholdDb));
    mixbusMakeupSpin_->setValue(static_cast<double>(adv.mixbusComp.makeupDb));
}

void ParameterOverridePanel::onResetToAuto() {
    if (autoAdvice_) applyAdviceToSpins(*autoAdvice_);
}

std::optional<mt::AdviceSet> ParameterOverridePanel::getAdviceOverride() const {
    if (!groupBox_->isChecked() || !autoAdvice_) return std::nullopt;

    mt::AdviceSet adv = *autoAdvice_;
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

mt::RenderOptions ParameterOverridePanel::getBypassOptions() const {
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

} // namespace gui
