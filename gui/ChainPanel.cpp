#include "ChainPanel.h"
#include "AudioControl.h"
#include "RotaryKnob.h"
#include "VerticalFader.h"

#include "mastertweak/analysis.hpp"

#include <cmath>

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
    for (int c = 0; c < 6; ++c)
        grid->setColumnStretch(c, 1);

    // ── Tier 1: EQ ────────────────────────────────────────────────────────────
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

            auto* fdr = new VerticalFader(eqBox_);
            fdr->setRange(-12.0, 12.0);
            fdr->setSingleStep(0.5);
            fdr->setSuffix(" dB");
            fdr->setValue(0.0);
            eqGainFaders_[i] = fdr;
            col->addWidget(fdr, 0, Qt::AlignHCenter);

            hbox->addLayout(col);

            connect(fdr, &AudioControl::valueChanged,
                    this, [this](double) { emitOverride(); });
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

        auto* drvLbl = new QLabel("Drive", satBox_);
        drvLbl->setAlignment(Qt::AlignHCenter);
        vbox->addWidget(drvLbl);

        satDriveKnob_ = new RotaryKnob(satBox_);
        satDriveKnob_->setRange(0.0, 6.0);
        satDriveKnob_->setSingleStep(0.5);
        satDriveKnob_->setSuffix(" dB");
        satDriveKnob_->setValue(0.0);
        vbox->addWidget(satDriveKnob_, 0, Qt::AlignHCenter);
        vbox->addStretch();

        connect(satDriveKnob_, &AudioControl::valueChanged,
                this, [this](double) { emitOverride(); });
        connect(satBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(satBox_, 1, 4, 1, 2);
    }

    // ── Tier 3 col 0-3: Mixbus Comp ──────────────────────────────────────────
    {
        mixbusBox_ = new QGroupBox("Mixbus Comp", this);
        mixbusBox_->setCheckable(true);
        mixbusBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(mixbusBox_);
        auto* row  = new QHBoxLayout;

        auto* thrCol = new QVBoxLayout;
        auto* thrLbl = new QLabel("Thr", mixbusBox_);
        thrLbl->setAlignment(Qt::AlignHCenter);
        thrCol->addWidget(thrLbl);
        mixbusThreshKnob_ = new RotaryKnob(mixbusBox_);
        mixbusThreshKnob_->setRange(-40.0, 0.0);
        mixbusThreshKnob_->setSingleStep(1.0);
        mixbusThreshKnob_->setSuffix(" dB");
        mixbusThreshKnob_->setValue(-20.0);
        thrCol->addWidget(mixbusThreshKnob_, 0, Qt::AlignHCenter);
        row->addLayout(thrCol);

        auto* mkupCol = new QVBoxLayout;
        auto* mkupLbl = new QLabel("Mkup", mixbusBox_);
        mkupLbl->setAlignment(Qt::AlignHCenter);
        mkupCol->addWidget(mkupLbl);
        mixbusMakeupKnob_ = new RotaryKnob(mixbusBox_);
        mixbusMakeupKnob_->setRange(-12.0, 12.0);
        mixbusMakeupKnob_->setSingleStep(0.5);
        mixbusMakeupKnob_->setSuffix(" dB");
        mixbusMakeupKnob_->setValue(0.0);
        mkupCol->addWidget(mixbusMakeupKnob_, 0, Qt::AlignHCenter);
        row->addLayout(mkupCol);

        row->addStretch();
        vbox->addLayout(row);

        mixbusRmsLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), mixbusBox_);
        mixbusRmsLbl_->setStyleSheet("color: #555;");
        vbox->addWidget(mixbusRmsLbl_);

        connect(mixbusThreshKnob_, &AudioControl::valueChanged,
                this, [this](double) { emitOverride(); });
        connect(mixbusMakeupKnob_, &AudioControl::valueChanged,
                this, [this](double) { emitOverride(); });
        connect(mixbusBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(mixbusBox_, 2, 0, 1, 4);
    }

    // ── Tier 3 col 4-5: Limiter ──────────────────────────────────────────────
    {
        limBox_ = new QGroupBox("Limiter", this);
        limBox_->setCheckable(true);
        limBox_->setChecked(true);

        auto* vbox = new QVBoxLayout(limBox_);

        auto* ceilLbl = new QLabel("Ceiling", limBox_);
        ceilLbl->setAlignment(Qt::AlignHCenter);
        vbox->addWidget(ceilLbl);

        limCeilingFader_ = new VerticalFader(limBox_);
        limCeilingFader_->setRange(-6.0, 0.0);
        limCeilingFader_->setSingleStep(0.5);
        limCeilingFader_->setSuffix(" dBTP");
        limCeilingFader_->setValue(-1.0);
        vbox->addWidget(limCeilingFader_);

        limPeakLbl_ = new QLabel(QString::fromUtf8("\xe2\x80\x94"), limBox_);
        limPeakLbl_->setStyleSheet("color: #555;");
        vbox->addWidget(limPeakLbl_);

        connect(limCeilingFader_, &AudioControl::valueChanged,
                this, [this](double) { emitOverride(); });
        connect(limBox_, &QGroupBox::toggled, this, [this](bool) { emitOverride(); });
        grid->addWidget(limBox_, 2, 4, 1, 2);
    }

    outer->addLayout(grid);
}

// ─────────────────────────────────────────────────────────────────────────────

void ChainPanel::applyToControls(const mt::AdviceSet& adv) {
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        const double v = static_cast<double>(adv.eq[si].gainDb);
        eqGainFaders_[i]->blockSignals(true);
        eqGainFaders_[i]->setValue(v);
        eqGainFaders_[i]->setClean(eqGainFaders_[i]->value());  // post-clamp
        eqGainFaders_[i]->blockSignals(false);
    }
    auto setCtrl = [](AudioControl* c, double v) {
        c->blockSignals(true);
        c->setValue(v);
        c->setClean(c->value());  // use post-clamp value to avoid spurious dirty dot
        c->blockSignals(false);
    };
    setCtrl(satDriveKnob_,     static_cast<double>(adv.saturator.driveDb));
    setCtrl(mixbusThreshKnob_, static_cast<double>(adv.mixbusComp.thresholdDb));
    setCtrl(mixbusMakeupKnob_, static_cast<double>(adv.mixbusComp.makeupDb));
    setCtrl(limCeilingFader_,  static_cast<double>(adv.limiter.ceilingDb));
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

    applyToControls(advice);

    // EQ per-band readouts: "preset target → measured RMS"
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        eqReadouts_[i]->setText(
            QString("%1\xe2\x86\x92%2 dB")
                .arg(static_cast<int>(std::round(preset.bandRmsDb[si])))
                .arg(static_cast<int>(std::round(snap.bands[si].avgRmsDb))));
    }

    float crestSum = 0.f;
    for (int i = 0; i < kNumBands; ++i)
        crestSum += snap.bands[static_cast<size_t>(i)].crestDb;
    const float avgCrest = crestSum / static_cast<float>(kNumBands);

    mbCrestLbl_->setText(
        QString("Crest: %1 dB").arg(static_cast<double>(avgCrest), 0, 'f', 1));
    satCrestLbl_->setText(
        QString("Crest: %1 dB").arg(static_cast<double>(avgCrest), 0, 'f', 1));
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
        adv.eq[si].gainDb = static_cast<float>(eqGainFaders_[i]->value());
    }
    adv.limiter.ceilingDb      = static_cast<float>(limCeilingFader_->value());
    adv.saturator.driveDb      = static_cast<float>(satDriveKnob_->value());
    adv.mixbusComp.thresholdDb = static_cast<float>(mixbusThreshKnob_->value());
    adv.mixbusComp.makeupDb    = static_cast<float>(mixbusMakeupKnob_->value());
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
    applyToControls(autoAdvice_);

    for (QGroupBox* box : {eqBox_, mbBox_, widthBox_, satBox_, mixbusBox_, limBox_}) {
        box->blockSignals(true);
        box->setChecked(true);
        box->blockSignals(false);
    }
}

void ChainPanel::clear() {
    hasAdvice_  = false;
    autoAdvice_ = mt::AdviceSet{};

    applyToControls(autoAdvice_);  // resets to defaults, clears dirty dots

    for (QGroupBox* box : {eqBox_, mbBox_, widthBox_, satBox_, mixbusBox_, limBox_}) {
        box->blockSignals(true);
        box->setChecked(true);
        box->blockSignals(false);
    }

    const QString dash = QString::fromUtf8("\xe2\x80\x94");
    for (int i = 0; i < kNumBands; ++i)
        eqReadouts_[i]->setText(dash);
    mbCrestLbl_->setText(dash);
    widthCorrLbl_->setText(dash);
    satCrestLbl_->setText(dash);
    mixbusRmsLbl_->setText(dash);
    limPeakLbl_->setText(dash);
}

} // namespace gui
