#include "ChainPanel.h"

#include "mastertweak/analysis.hpp"

#include <cmath>

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace gui {

ChainPanel::ChainPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void ChainPanel::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* grid = new QGridLayout;
    // Equal column weights; Mixbus spans cols 0-3, Limiter spans cols 4-5.
    for (int c = 0; c < 6; ++c)
        grid->setColumnStretch(c, 1);

    // ── Tier 1: EQ (full width) ───────────────────────────────────────────────
    {
        eqBox_ = new QGroupBox("EQ", this);
        eqBox_->setCheckable(true);
        eqBox_->setChecked(true);

        auto* hbox = new QHBoxLayout(eqBox_);
        for (int i = 0; i < kNumBands; ++i) {
            const auto si = static_cast<size_t>(i);
            auto* col = new QVBoxLayout;
            col->setSpacing(2);

            auto* nameLbl = new QLabel(mt::AnalysisSnapshot::kBandNames[si], eqBox_);
            nameLbl->setAlignment(Qt::AlignHCenter);
            col->addWidget(nameLbl);

            auto* readout = new QLabel(QString::fromUtf8("\xe2\x80\x94"), eqBox_);
            readout->setAlignment(Qt::AlignHCenter);
            readout->setStyleSheet("font-size: 9pt; color: #555;");
            eqReadouts_[i] = readout;
            col->addWidget(readout);

            auto* sp = new QDoubleSpinBox(eqBox_);
            sp->setRange(-12.0, 12.0);
            sp->setSingleStep(0.5);
            sp->setDecimals(1);
            sp->setSuffix(" dB");
            sp->setValue(0.0);
            sp->setAlignment(Qt::AlignRight);
            eqGainSpins_[i] = sp;
            col->addWidget(sp);

            hbox->addLayout(col);

            const int ci = i;
            connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this, ci](double) {
                        updateTint(eqGainSpins_[ci], cleanValues_.eqGain[ci]);
                        emitOverride();
                    });
        }
        connect(eqBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(eqBox_, 0, 0, 1, 6);
    }

    // ── Tier 2 col 0-1: Multiband Comp ───────────────────────────────────────
    {
        mbBox_ = new QGroupBox("Multiband Comp", this);
        mbBox_->setCheckable(true);
        mbBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(mbBox_);
        mbCrestLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), mbBox_);
        vbox->addWidget(mbCrestLbl_);
        auto* adv = new QLabel("Advice-driven", mbBox_);
        adv->setStyleSheet("color: #888; font-style: italic;");
        vbox->addWidget(adv);
        vbox->addStretch();

        connect(mbBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(mbBox_, 1, 0, 1, 2);
    }

    // ── Tier 2 col 2-3: Stereo Width ─────────────────────────────────────────
    {
        widthBox_ = new QGroupBox("Stereo Width", this);
        widthBox_->setCheckable(true);
        widthBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(widthBox_);
        widthCorrLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), widthBox_);
        vbox->addWidget(widthCorrLbl_);
        auto* adv = new QLabel("Advice-driven", widthBox_);
        adv->setStyleSheet("color: #888; font-style: italic;");
        vbox->addWidget(adv);
        vbox->addStretch();

        connect(widthBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(widthBox_, 1, 2, 1, 2);
    }

    // ── Tier 2 col 4-5: Saturator ────────────────────────────────────────────
    {
        satBox_ = new QGroupBox("Saturator", this);
        satBox_->setCheckable(true);
        satBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(satBox_);
        satCrestLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), satBox_);
        vbox->addWidget(satCrestLbl_);

        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("Drive:", satBox_));
        satDriveSpin_ = new QDoubleSpinBox(satBox_);
        satDriveSpin_->setRange(0.0, 6.0);
        satDriveSpin_->setSingleStep(0.5);
        satDriveSpin_->setDecimals(1);
        satDriveSpin_->setSuffix(" dB");
        satDriveSpin_->setValue(0.0);
        row->addWidget(satDriveSpin_);
        row->addStretch();
        vbox->addLayout(row);
        vbox->addStretch();

        connect(satDriveSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(satDriveSpin_, cleanValues_.satDrive);
                    emitOverride();
                });
        connect(satBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(satBox_, 1, 4, 1, 2);
    }

    // ── Tier 3 col 0-3: Mixbus Comp ──────────────────────────────────────────
    {
        mixbusBox_ = new QGroupBox("Mixbus Comp", this);
        mixbusBox_->setCheckable(true);
        mixbusBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(mixbusBox_);

        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("Thr:", mixbusBox_));
        mixbusThreshSpin_ = new QDoubleSpinBox(mixbusBox_);
        mixbusThreshSpin_->setRange(-40.0, 0.0);
        mixbusThreshSpin_->setSingleStep(1.0);
        mixbusThreshSpin_->setDecimals(1);
        mixbusThreshSpin_->setSuffix(" dB");
        mixbusThreshSpin_->setValue(-20.0);
        row->addWidget(mixbusThreshSpin_);
        row->addSpacing(8);
        row->addWidget(new QLabel("Mkup:", mixbusBox_));
        mixbusMakeupSpin_ = new QDoubleSpinBox(mixbusBox_);
        mixbusMakeupSpin_->setRange(-12.0, 12.0);
        mixbusMakeupSpin_->setSingleStep(0.5);
        mixbusMakeupSpin_->setDecimals(1);
        mixbusMakeupSpin_->setSuffix(" dB");
        mixbusMakeupSpin_->setValue(0.0);
        row->addWidget(mixbusMakeupSpin_);
        row->addStretch();
        vbox->addLayout(row);

        mixbusRmsLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), mixbusBox_);
        mixbusRmsLbl_->setStyleSheet("color: #555;");
        vbox->addWidget(mixbusRmsLbl_);

        connect(mixbusThreshSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(mixbusThreshSpin_, cleanValues_.mixbusThresh);
                    emitOverride();
                });
        connect(mixbusMakeupSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(mixbusMakeupSpin_, cleanValues_.mixbusMakeup);
                    emitOverride();
                });
        connect(mixbusBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(mixbusBox_, 2, 0, 1, 4);
    }

    // ── Tier 3 col 4-5: Limiter ──────────────────────────────────────────────
    {
        limBox_ = new QGroupBox("Limiter", this);
        limBox_->setCheckable(true);
        limBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(limBox_);

        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("Ceil:", limBox_));
        limCeilingSpin_ = new QDoubleSpinBox(limBox_);
        limCeilingSpin_->setRange(-6.0, 0.0);
        limCeilingSpin_->setSingleStep(0.5);
        limCeilingSpin_->setDecimals(1);
        limCeilingSpin_->setSuffix(" dBTP");
        limCeilingSpin_->setValue(-1.0);
        row->addWidget(limCeilingSpin_);
        row->addStretch();
        vbox->addLayout(row);

        limPeakLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), limBox_);
        limPeakLbl_->setStyleSheet("color: #555;");
        vbox->addWidget(limPeakLbl_);

        connect(limCeilingSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
                    updateTint(limCeilingSpin_, cleanValues_.limCeiling);
                    emitOverride();
                });
        connect(limBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(limBox_, 2, 4, 1, 2);
    }

    outer->addLayout(grid);
}

// ─────────────────────────────────────────────────────────────────────────────

void ChainPanel::applyToSpins(const mt::AdviceSet& adv) {
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

void ChainPanel::updateTint(QDoubleSpinBox* sp, double cleanVal) {
    constexpr double kEps = 1e-9;
    sp->setStyleSheet(std::abs(sp->value() - cleanVal) > kEps
                      ? "background-color: #d0e8ff;" : "");
}

void ChainPanel::clearAllTints() {
    for (int i = 0; i < kNumBands; ++i)
        eqGainSpins_[i]->setStyleSheet("");
    limCeilingSpin_->setStyleSheet("");
    satDriveSpin_->setStyleSheet("");
    mixbusThreshSpin_->setStyleSheet("");
    mixbusMakeupSpin_->setStyleSheet("");
}

void ChainPanel::emitOverride() {
    emit overrideChanged(currentAdvice());
}

// ─────────────────────────────────────────────────────────────────────────────

void ChainPanel::setAdvice(const mt::AdviceSet& advice,
                           const mt::AnalysisSnapshot& snap,
                           const mt::PresetData& preset) {
    autoAdvice_ = advice;
    hasAdvice_  = true;

    for (int i = 0; i < kNumBands; ++i)
        cleanValues_.eqGain[i] = static_cast<double>(advice.eq[static_cast<size_t>(i)].gainDb);
    cleanValues_.limCeiling   = static_cast<double>(advice.limiter.ceilingDb);
    cleanValues_.satDrive     = static_cast<double>(advice.saturator.driveDb);
    cleanValues_.mixbusThresh = static_cast<double>(advice.mixbusComp.thresholdDb);
    cleanValues_.mixbusMakeup = static_cast<double>(advice.mixbusComp.makeupDb);

    applyToSpins(advice);
    clearAllTints();

    // EQ per-band readouts: "preset target → measured RMS"
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        eqReadouts_[i]->setText(
            QString("%1\xe2\x86\x92%2 dB")
                .arg(static_cast<int>(std::round(preset.bandRmsDb[si])))
                .arg(static_cast<int>(std::round(snap.bands[si].avgRmsDb))));
    }

    // Average crest factor (shared by MB Comp and Saturator sections)
    float crestSum = 0.f;
    for (int i = 0; i < kNumBands; ++i)
        crestSum += snap.bands[static_cast<size_t>(i)].crestDb;
    const float avgCrest = crestSum / static_cast<float>(kNumBands);

    mbCrestLbl_->setText(QString("Crest: %1 dB").arg(static_cast<double>(avgCrest), 0, 'f', 1));
    satCrestLbl_->setText(QString("Crest: %1 dB").arg(static_cast<double>(avgCrest), 0, 'f', 1));

    widthCorrLbl_->setText(
        QString("Corr: %1").arg(static_cast<double>(snap.overallCorr), 0, 'f', 2));

    mixbusRmsLbl_->setText(
        QString("rms: %1 dBFS").arg(static_cast<double>(snap.overallAvgDb), 0, 'f', 1));

    limPeakLbl_->setText(
        QString("peak: %1 dBFS").arg(static_cast<double>(snap.overallPeakDb), 0, 'f', 1));
}

mt::AdviceSet ChainPanel::currentAdvice() const {
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

void ChainPanel::populateBypassFlags(mt::RenderOptions& opts) const {
    opts.bypassEq         = !eqBox_->isChecked();
    opts.bypassMbComp     = !mbBox_->isChecked();
    opts.bypassSaturator  = !satBox_->isChecked();
    opts.bypassWidth      = !widthBox_->isChecked();
    opts.bypassMixbusComp = !mixbusBox_->isChecked();
    opts.bypassLimiter    = !limBox_->isChecked();
    // bypassDither not exposed in UI; stays false (default)
}

void ChainPanel::resetToAdvice() {
    if (!hasAdvice_) return;
    applyToSpins(autoAdvice_);
    clearAllTints();
}

void ChainPanel::clear() {
    hasAdvice_ = false;
    autoAdvice_ = mt::AdviceSet{};

    for (int i = 0; i < kNumBands; ++i) cleanValues_.eqGain[i] = 0.0;
    cleanValues_.limCeiling   = -1.0;
    cleanValues_.satDrive     =  0.0;
    cleanValues_.mixbusThresh = -20.0;
    cleanValues_.mixbusMakeup =  0.0;

    const QString dash = QString::fromUtf8("\xe2\x80\x94");

    for (int i = 0; i < kNumBands; ++i) {
        eqGainSpins_[i]->blockSignals(true);
        eqGainSpins_[i]->setValue(0.0);
        eqGainSpins_[i]->blockSignals(false);
        eqGainSpins_[i]->setStyleSheet("");
        eqReadouts_[i]->setText(dash);
    }

    auto resetSpin = [](QDoubleSpinBox* sp, double val) {
        sp->blockSignals(true);
        sp->setValue(val);
        sp->blockSignals(false);
        sp->setStyleSheet("");
    };
    resetSpin(limCeilingSpin_,    -1.0);
    resetSpin(satDriveSpin_,       0.0);
    resetSpin(mixbusThreshSpin_, -20.0);
    resetSpin(mixbusMakeupSpin_,   0.0);

    mbCrestLbl_->setText(dash);
    widthCorrLbl_->setText(dash);
    satCrestLbl_->setText(dash);
    mixbusRmsLbl_->setText(dash);
    limPeakLbl_->setText(dash);
}

} // namespace gui
