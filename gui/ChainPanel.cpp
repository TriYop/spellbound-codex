#include "ChainPanel.h"
#include "AudioControl.h"
#include "RackUnit.h"
#include "RotaryKnob.h"
#include "VerticalFader.h"

#include "mastertweak/analysis.hpp"

#include <cmath>

#include <QCheckBox>
#include <QFrame>
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
    setStyleSheet("ChainPanel { background: #111; }");
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 4, 4, 4);
    outer->setSpacing(2);

    // ── Resonance EQ ──────────────────────────────────────────────────────────
    {
        auto* body    = new QWidget;
        auto* innerVbox = new QVBoxLayout(body);
        innerVbox->setContentsMargins(6, 4, 6, 4);
        innerVbox->setSpacing(2);

        resNoResLabel_ = new QLabel("No resonances detected", body);
        resNoResLabel_->setStyleSheet("color: #888; font-style: italic;");
        innerVbox->addWidget(resNoResLabel_);

        for (int i = 0; i < kMaxResonances; ++i) {
            auto* row  = new QWidget(body);
            auto* hbox = new QHBoxLayout(row);
            hbox->setContentsMargins(0, 0, 0, 0);
            hbox->setSpacing(6);

            resRowLabels_[i] = new QLabel(row);
            resRowLabels_[i]->setStyleSheet("font-size: 9pt;");
            hbox->addWidget(resRowLabels_[i]);

            resRowChecks_[i] = new QCheckBox("enable", row);
            resRowChecks_[i]->setChecked(true);
            hbox->addWidget(resRowChecks_[i]);
            hbox->addStretch();

            resRowLabels_[i]->setVisible(false);
            resRowChecks_[i]->setVisible(false);
            innerVbox->addWidget(row);

            connect(resRowChecks_[i], &QCheckBox::toggled, this, [this](bool) { emitOverride(); });
        }

        resUnit_ = new RackUnit("resEq", "Resonance EQ", body, this);
        connect(resUnit_, &RackUnit::bypassChanged, this, [this](bool) { emitOverride(); });
        outer->addWidget(resUnit_);
    }

    // ── Parametric EQ ─────────────────────────────────────────────────────────
    {
        auto* body = new QWidget;
        auto* hbox = new QHBoxLayout(body);
        hbox->setContentsMargins(6, 4, 6, 4);
        hbox->setSpacing(4);

        for (int i = 0; i < kNumBands; ++i) {
            const auto si = static_cast<size_t>(i);
            auto* col = new QVBoxLayout;
            col->setSpacing(2);

            auto* nameLbl = new QLabel(mt::AnalysisSnapshot::kBandNames[si], body);
            nameLbl->setAlignment(Qt::AlignHCenter);
            nameLbl->setStyleSheet("font-size: 8pt;");
            col->addWidget(nameLbl);

            auto* readout = new QLabel(QString::fromUtf8("\xe2\x80\x94"), body);
            readout->setAlignment(Qt::AlignHCenter);
            readout->setStyleSheet("font-size: 9pt; color: #555;");
            eqReadouts_[i] = readout;
            col->addWidget(readout);

            auto* fdr = new VerticalFader(body);
            fdr->setRange(-12.0, 12.0);
            fdr->setSingleStep(0.5);
            fdr->setSuffix(" dB");
            fdr->setValue(0.0);
            eqGainFaders_[i] = fdr;
            col->addWidget(fdr, 0, Qt::AlignHCenter);

            hbox->addLayout(col);
            connect(fdr, &AudioControl::valueChanged, this, [this](double) { emitOverride(); });
        }

        eqUnit_ = new RackUnit("eq", "Parametric EQ", body, this);
        connect(eqUnit_, &RackUnit::bypassChanged, this, [this](bool) { emitOverride(); });
        outer->addWidget(eqUnit_);
    }

    // ── Multiband Comp ────────────────────────────────────────────────────────
    {
        auto* body = new QWidget;
        auto* hbox = new QHBoxLayout(body);
        hbox->setContentsMargins(6, 4, 6, 4);
        hbox->setSpacing(4);

        for (int i = 0; i < kNumBands; ++i) {
            const auto si = static_cast<size_t>(i);
            auto* cell = new QFrame(body);
            cell->setFrameShape(QFrame::StyledPanel);
            cell->setStyleSheet(
                "QFrame { background: #1c1c1c; border: 1px solid #333; border-radius: 4px; }");
            auto* vbox = new QVBoxLayout(cell);
            vbox->setContentsMargins(4, 4, 4, 4);
            vbox->setSpacing(2);

            auto* nameLbl = new QLabel(mt::AnalysisSnapshot::kBandNames[si], cell);
            nameLbl->setAlignment(Qt::AlignHCenter);
            nameLbl->setStyleSheet("font-size: 8pt; color: #aaa;");
            vbox->addWidget(nameLbl);

            auto* thrLbl = new QLabel("Thr", cell);
            thrLbl->setAlignment(Qt::AlignHCenter);
            thrLbl->setStyleSheet("font-size: 8pt; color: #888;");
            vbox->addWidget(thrLbl);

            mbThreshFaders_[i] = new VerticalFader(cell);
            mbThreshFaders_[i]->setRange(-40.0, 0.0);
            mbThreshFaders_[i]->setSingleStep(1.0);
            mbThreshFaders_[i]->setSuffix(" dBFS");
            mbThreshFaders_[i]->setValue(-20.0);
            vbox->addWidget(mbThreshFaders_[i], 0, Qt::AlignHCenter);

            auto* sep = new QFrame(cell);
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet("background: #333;");
            sep->setFixedHeight(1);
            vbox->addWidget(sep);

            auto* ratioLbl = new QLabel("Ratio", cell);
            ratioLbl->setAlignment(Qt::AlignHCenter);
            ratioLbl->setStyleSheet("font-size: 8pt; color: #888;");
            vbox->addWidget(ratioLbl);

            mbRatioKnobs_[i] = new RotaryKnob(cell);
            mbRatioKnobs_[i]->setRange(1.0, 8.0);
            mbRatioKnobs_[i]->setSingleStep(0.1);
            mbRatioKnobs_[i]->setSuffix(":1");
            mbRatioKnobs_[i]->setValue(1.0);
            vbox->addWidget(mbRatioKnobs_[i], 0, Qt::AlignHCenter);

            hbox->addWidget(cell);
            connect(mbThreshFaders_[i], &AudioControl::valueChanged,
                    this, [this](double) { emitOverride(); });
            connect(mbRatioKnobs_[i], &AudioControl::valueChanged,
                    this, [this](double) { emitOverride(); });
        }

        mbUnit_ = new RackUnit("mbComp", "Multiband Comp", body, this);
        connect(mbUnit_, &RackUnit::bypassChanged, this, [this](bool) { emitOverride(); });
        outer->addWidget(mbUnit_);
    }

    // ── Saturator ─────────────────────────────────────────────────────────────
    {
        auto* body = new QWidget;
        auto* vbox = new QVBoxLayout(body);
        vbox->setContentsMargins(6, 4, 6, 4);

        auto* drvLbl = new QLabel("Drive", body);
        drvLbl->setAlignment(Qt::AlignHCenter);
        vbox->addWidget(drvLbl);

        satDriveKnob_ = new RotaryKnob(body);
        satDriveKnob_->setRange(0.0, 6.0);
        satDriveKnob_->setSingleStep(0.5);
        satDriveKnob_->setSuffix(" dB");
        satDriveKnob_->setValue(0.0);
        vbox->addWidget(satDriveKnob_, 0, Qt::AlignHCenter);
        vbox->addStretch();

        satUnit_ = new RackUnit("saturator", "Saturator", body, this);
        connect(satUnit_, &RackUnit::bypassChanged, this, [this](bool) { emitOverride(); });
        connect(satDriveKnob_, &AudioControl::valueChanged,
                this, [this](double) { emitOverride(); });
        outer->addWidget(satUnit_);
    }

    // ── Stereo Width ──────────────────────────────────────────────────────────
    {
        auto* body = new QWidget;
        auto* hbox = new QHBoxLayout(body);
        hbox->setContentsMargins(6, 4, 6, 4);
        hbox->setSpacing(4);

        for (int i = 0; i < kNumBands; ++i) {
            const auto si = static_cast<size_t>(i);
            auto* col = new QVBoxLayout;
            col->setSpacing(2);

            auto* nameLbl = new QLabel(mt::AnalysisSnapshot::kBandNames[si], body);
            nameLbl->setAlignment(Qt::AlignHCenter);
            nameLbl->setStyleSheet("font-size: 8pt;");
            col->addWidget(nameLbl);

            widthFaders_[i] = new VerticalFader(body);
            widthFaders_[i]->setRange(0.0, 2.0);
            widthFaders_[i]->setSingleStep(0.05);
            widthFaders_[i]->setValue(1.0);
            widthFaders_[i]->setSuffix("\xc3\x97");  // ×
            widthFaders_[i]->setStyleSheet(
                "VerticalFader QSlider::groove:vertical { background: #1a2a3a; }"
                "VerticalFader QSlider::handle:vertical { background: #2d5c8a; }");
            col->addWidget(widthFaders_[i], 0, Qt::AlignHCenter);

            hbox->addLayout(col);
            connect(widthFaders_[i], &AudioControl::valueChanged,
                    this, [this](double) { emitOverride(); });
        }

        widthUnit_ = new RackUnit("stereoWidth", "Stereo Width", body, this);
        connect(widthUnit_, &RackUnit::bypassChanged, this, [this](bool) { emitOverride(); });
        outer->addWidget(widthUnit_);
    }

    // ── Mixbus Comp ───────────────────────────────────────────────────────────
    {
        auto* body = new QWidget;
        auto* row  = new QHBoxLayout(body);
        row->setContentsMargins(6, 4, 6, 4);
        row->setSpacing(12);

        auto addKnobCol = [&](const char* label, RotaryKnob*& knobPtr,
                              double lo, double hi, double step,
                              const char* suffix, double def) {
            auto* col    = new QVBoxLayout;
            auto* lbl    = new QLabel(label, body);
            lbl->setAlignment(Qt::AlignHCenter);
            col->addWidget(lbl);
            knobPtr = new RotaryKnob(body);
            knobPtr->setRange(lo, hi);
            knobPtr->setSingleStep(step);
            knobPtr->setSuffix(QString::fromUtf8(suffix));
            knobPtr->setValue(def);
            col->addWidget(knobPtr, 0, Qt::AlignHCenter);
            row->addLayout(col);
            connect(knobPtr, &AudioControl::valueChanged,
                    this, [this](double) { emitOverride(); });
        };

        addKnobCol("Thr",   mixbusThreshKnob_, -40.0,  0.0, 1.0, " dB",  -20.0);
        addKnobCol("Ratio", mixbusRatioKnob_,    1.0,  8.0, 0.1, ":1",     2.0);
        addKnobCol("Mkup",  mixbusMakeupKnob_, -12.0, 12.0, 0.5, " dB",    0.0);
        row->addStretch();

        mixbusUnit_ = new RackUnit("mixbusComp", "Mixbus Comp", body, this);
        connect(mixbusUnit_, &RackUnit::bypassChanged, this, [this](bool) { emitOverride(); });
        outer->addWidget(mixbusUnit_);
    }

    // ── Limiter ───────────────────────────────────────────────────────────────
    {
        auto* body = new QWidget;
        auto* row  = new QHBoxLayout(body);
        row->setContentsMargins(6, 4, 6, 4);
        row->setSpacing(12);

        auto* targetCol = new QVBoxLayout;
        auto* targetLbl = new QLabel("Target", body);
        targetLbl->setAlignment(Qt::AlignHCenter);
        targetCol->addWidget(targetLbl);
        limTargetKnob_ = new RotaryKnob(body);
        limTargetKnob_->setRange(-23.0, -6.0);
        limTargetKnob_->setSingleStep(0.5);
        limTargetKnob_->setSuffix(" LUFS");
        limTargetKnob_->setValue(-14.0);
        targetCol->addWidget(limTargetKnob_, 0, Qt::AlignHCenter);
        row->addLayout(targetCol);

        auto* ceilCol = new QVBoxLayout;
        auto* ceilLbl = new QLabel("Ceiling", body);
        ceilLbl->setAlignment(Qt::AlignHCenter);
        ceilCol->addWidget(ceilLbl);
        limCeilingFader_ = new VerticalFader(body);
        limCeilingFader_->setRange(-6.0, 0.0);
        limCeilingFader_->setSingleStep(0.5);
        limCeilingFader_->setSuffix(" dBTP");
        limCeilingFader_->setValue(-1.0);
        ceilCol->addWidget(limCeilingFader_, 0, Qt::AlignHCenter);
        row->addLayout(ceilCol);
        row->addStretch();

        limUnit_ = new RackUnit("limiter", "Limiter", body, this);
        connect(limUnit_, &RackUnit::bypassChanged, this, [this](bool) { emitOverride(); });
        connect(limTargetKnob_,   &AudioControl::valueChanged,
                this, [this](double) { emitOverride(); });
        connect(limCeilingFader_, &AudioControl::valueChanged,
                this, [this](double) { emitOverride(); });
        outer->addWidget(limUnit_);
    }
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

    // ── Resonance EQ rows ─────────────────────────────────────────────────────
    {
        const int nRes = static_cast<int>(advice.resonances.size());
        resNoResLabel_->setVisible(nRes == 0);
        resBox_->blockSignals(true);
        resBox_->setChecked(nRes > 0);
        resBox_->blockSignals(false);

        for (int i = 0; i < kMaxResonances; ++i) {
            if (i < nRes) {
                const auto& p = advice.resonances[static_cast<size_t>(i)];
                resRowLabels_[i]->setText(
                    QString("%1 Hz  Q:%2  %3 dB")
                        .arg(static_cast<int>(p.freqHz))
                        .arg(static_cast<double>(p.q), 0, 'f', 1)
                        .arg(static_cast<double>(p.gainDb), 0, 'f', 1));
                resRowChecks_[i]->blockSignals(true);
                resRowChecks_[i]->setChecked(p.enabled);
                resRowChecks_[i]->blockSignals(false);
                resRowLabels_[i]->setVisible(true);
                resRowChecks_[i]->setVisible(true);
            } else {
                resRowLabels_[i]->setVisible(false);
                resRowChecks_[i]->setVisible(false);
            }
        }
    }

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
    limLraLbl_->setText(
        QString("LRA: %1 LU").arg(static_cast<double>(snap.lraLu), 0, 'f', 1));
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
    for (int i = 0; i < static_cast<int>(adv.resonances.size()); ++i)
        adv.resonances[static_cast<size_t>(i)].enabled = resRowChecks_[i]->isChecked();
    return adv;
}

void ChainPanel::populateBypassFlags(mt::RenderOptions& opts) const {
    if (resBox_) opts.bypassResonanceEq = !resBox_->isChecked();
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

    // Re-populate resonance section from autoAdvice_
    {
        const int nRes = static_cast<int>(autoAdvice_.resonances.size());
        resNoResLabel_->setVisible(nRes == 0);
        resBox_->blockSignals(true);
        resBox_->setChecked(nRes > 0);
        resBox_->blockSignals(false);

        for (int i = 0; i < kMaxResonances; ++i) {
            if (i < nRes) {
                resRowChecks_[i]->blockSignals(true);
                resRowChecks_[i]->setChecked(autoAdvice_.resonances[static_cast<size_t>(i)].enabled);
                resRowChecks_[i]->blockSignals(false);
                resRowLabels_[i]->setVisible(true);
                resRowChecks_[i]->setVisible(true);
            } else {
                resRowLabels_[i]->setVisible(false);
                resRowChecks_[i]->setVisible(false);
            }
        }
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

    // Reset resonance section
    resBox_->blockSignals(true);
    resBox_->setChecked(false);
    resBox_->blockSignals(false);
    resNoResLabel_->setVisible(true);
    for (int i = 0; i < kMaxResonances; ++i) {
        resRowLabels_[i]->setVisible(false);
        resRowChecks_[i]->setVisible(false);
    }

    const QString dash = QString::fromUtf8("\xe2\x80\x94");
    for (int i = 0; i < kNumBands; ++i)
        eqReadouts_[i]->setText(dash);
    mbCrestLbl_->setText(dash);
    widthCorrLbl_->setText(dash);
    satCrestLbl_->setText(dash);
    mixbusRmsLbl_->setText(dash);
    limPeakLbl_->setText(dash);
    limLraLbl_->setText(dash);
}

VerticalFader* ChainPanel::eqGainFader(int band)  const { return eqGainFaders_[band]; }
RotaryKnob*    ChainPanel::satDriveKnob()          const { return satDriveKnob_; }
RotaryKnob*    ChainPanel::mixbusThreshKnob()      const { return mixbusThreshKnob_; }
RotaryKnob*    ChainPanel::mixbusMakeupKnob()      const { return mixbusMakeupKnob_; }
VerticalFader* ChainPanel::limCeilingFader()       const { return limCeilingFader_; }

} // namespace gui
