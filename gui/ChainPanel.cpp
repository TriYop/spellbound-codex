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
    auto setCtrl = [](AudioControl* c, double v) {
        c->blockSignals(true);
        c->setValue(v);
        c->setClean(c->value());
        c->blockSignals(false);
    };

    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        setCtrl(eqGainFaders_[i],   static_cast<double>(adv.eq[si].gainDb));
        setCtrl(mbThreshFaders_[i], static_cast<double>(adv.mbComp[si].thresholdDb));
        setCtrl(mbRatioKnobs_[i],   static_cast<double>(adv.mbComp[si].ratio));
        setCtrl(widthFaders_[i],    static_cast<double>(adv.width[si].width));
    }
    setCtrl(satDriveKnob_,     static_cast<double>(adv.saturator.driveDb));
    setCtrl(mixbusThreshKnob_, static_cast<double>(adv.mixbusComp.thresholdDb));
    setCtrl(mixbusRatioKnob_,  static_cast<double>(adv.mixbusComp.ratio));
    setCtrl(mixbusMakeupKnob_, static_cast<double>(adv.mixbusComp.makeupDb));
    setCtrl(limTargetKnob_,    static_cast<double>(adv.limiter.targetLufsApprox));
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
        resUnit_->blockSignals(true);
        resUnit_->setBypassed(nRes == 0);
        resUnit_->blockSignals(false);

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

        if (nRes > 0) {
            const auto& p0 = advice.resonances[0];
            resUnit_->setStats(
                QString("%1 peak(s) · top %2 dB @ %3 Hz")
                    .arg(nRes)
                    .arg(static_cast<double>(p0.gainDb), 0, 'f', 1)
                    .arg(static_cast<int>(p0.freqHz)));
        } else {
            resUnit_->setStats("No resonances detected");
        }
        resUnit_->setBadge("advised");
    }

    // ── EQ readouts ───────────────────────────────────────────────────────────
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        eqReadouts_[i]->setText(
            QString("%1\xe2\x86\x92%2 dB")
                .arg(static_cast<int>(std::round(preset.bandRmsDb[si])))
                .arg(static_cast<int>(std::round(snap.bands[si].avgRmsDb))));
    }
    eqUnit_->setStats(
        QString("Sub %1  Lows %2  Lo-Mid %3  Mids %4  Hi-Mid %5  Highs %6  Air %7 dB")
            .arg(static_cast<double>(advice.eq[0].gainDb), 0, 'f', 1)
            .arg(static_cast<double>(advice.eq[1].gainDb), 0, 'f', 1)
            .arg(static_cast<double>(advice.eq[2].gainDb), 0, 'f', 1)
            .arg(static_cast<double>(advice.eq[3].gainDb), 0, 'f', 1)
            .arg(static_cast<double>(advice.eq[4].gainDb), 0, 'f', 1)
            .arg(static_cast<double>(advice.eq[5].gainDb), 0, 'f', 1)
            .arg(static_cast<double>(advice.eq[6].gainDb), 0, 'f', 1));
    eqUnit_->setBadge("advised");

    // ── Multiband Comp stats ──────────────────────────────────────────────────
    float crestSum = 0.f;
    for (int i = 0; i < kNumBands; ++i)
        crestSum += snap.bands[static_cast<size_t>(i)].crestDb;
    const float avgCrest = crestSum / static_cast<float>(kNumBands);

    mbUnit_->setStats(
        QString("Sub %1/%2:1  Mids %3/%4:1  Air %5/%6:1 · crest %7 dB")
            .arg(static_cast<double>(advice.mbComp[0].thresholdDb), 0, 'f', 1)
            .arg(static_cast<double>(advice.mbComp[0].ratio),       0, 'f', 1)
            .arg(static_cast<double>(advice.mbComp[3].thresholdDb), 0, 'f', 1)
            .arg(static_cast<double>(advice.mbComp[3].ratio),       0, 'f', 1)
            .arg(static_cast<double>(advice.mbComp[6].thresholdDb), 0, 'f', 1)
            .arg(static_cast<double>(advice.mbComp[6].ratio),       0, 'f', 1)
            .arg(static_cast<double>(avgCrest), 0, 'f', 1));
    mbUnit_->setBadge("advised");

    // ── Saturator stats ───────────────────────────────────────────────────────
    satUnit_->setStats(
        QString("drive %1 dB · crest %2 dB")
            .arg(static_cast<double>(advice.saturator.driveDb), 0, 'f', 1)
            .arg(static_cast<double>(avgCrest), 0, 'f', 1));
    satUnit_->setBadge("advised");

    // ── Stereo Width stats ────────────────────────────────────────────────────
    widthUnit_->setStats(
        QString("corr %1 · Lows %2\xc3\x97  Mids %3\xc3\x97  Highs %4\xc3\x97")
            .arg(static_cast<double>(snap.overallCorr),        0, 'f', 2)
            .arg(static_cast<double>(advice.width[1].width),   0, 'f', 2)
            .arg(static_cast<double>(advice.width[3].width),   0, 'f', 2)
            .arg(static_cast<double>(advice.width[5].width),   0, 'f', 2));
    widthUnit_->setBadge("advised");

    // ── Mixbus Comp stats ─────────────────────────────────────────────────────
    mixbusUnit_->setStats(
        QString("%1 dB / %2:1 / %3 mkup · rms %4 dBFS")
            .arg(static_cast<double>(advice.mixbusComp.thresholdDb), 0, 'f', 1)
            .arg(static_cast<double>(advice.mixbusComp.ratio),       0, 'f', 1)
            .arg(static_cast<double>(advice.mixbusComp.makeupDb),    0, 'f', 1)
            .arg(static_cast<double>(snap.overallAvgDb),             0, 'f', 1));
    mixbusUnit_->setBadge("advised");

    // ── Limiter stats ─────────────────────────────────────────────────────────
    limUnit_->setStats(
        QString("%1 LUFS · ceil %2 dBTP · peak %3 dBFS · LRA %4 LU")
            .arg(static_cast<double>(advice.limiter.targetLufsApprox), 0, 'f', 1)
            .arg(static_cast<double>(advice.limiter.ceilingDb),        0, 'f', 1)
            .arg(static_cast<double>(snap.overallPeakDb),              0, 'f', 1)
            .arg(static_cast<double>(snap.lraLu),                      0, 'f', 1));
    limUnit_->setBadge("advised");
}

mt::AdviceSet ChainPanel::currentAdvice() const {
    mt::AdviceSet adv = autoAdvice_;
    for (int i = 0; i < kNumBands; ++i) {
        const auto si = static_cast<size_t>(i);
        adv.eq[si].gainDb          = static_cast<float>(eqGainFaders_[i]->value());
        adv.mbComp[si].thresholdDb = static_cast<float>(mbThreshFaders_[i]->value());
        adv.mbComp[si].ratio       = static_cast<float>(mbRatioKnobs_[i]->value());
        adv.width[si].width        = static_cast<float>(widthFaders_[i]->value());
    }
    adv.saturator.driveDb        = static_cast<float>(satDriveKnob_->value());
    adv.mixbusComp.thresholdDb   = static_cast<float>(mixbusThreshKnob_->value());
    adv.mixbusComp.ratio         = static_cast<float>(mixbusRatioKnob_->value());
    adv.mixbusComp.makeupDb      = static_cast<float>(mixbusMakeupKnob_->value());
    adv.limiter.targetLufsApprox = static_cast<float>(limTargetKnob_->value());
    adv.limiter.ceilingDb        = static_cast<float>(limCeilingFader_->value());
    for (int i = 0; i < static_cast<int>(adv.resonances.size()); ++i)
        adv.resonances[static_cast<size_t>(i)].enabled = resRowChecks_[i]->isChecked();
    return adv;
}

void ChainPanel::populateBypassFlags(mt::RenderOptions& opts) const {
    opts.bypassResonanceEq = resUnit_->isBypassed();
    opts.bypassEq          = eqUnit_->isBypassed();
    opts.bypassMbComp      = mbUnit_->isBypassed();
    opts.bypassSaturator   = satUnit_->isBypassed();
    opts.bypassWidth       = widthUnit_->isBypassed();
    opts.bypassMixbusComp  = mixbusUnit_->isBypassed();
    opts.bypassLimiter     = limUnit_->isBypassed();
}

void ChainPanel::resetToAdvice() {
    if (!hasAdvice_) return;
    applyToControls(autoAdvice_);

    for (RackUnit* u : {eqUnit_, mbUnit_, satUnit_, widthUnit_, mixbusUnit_, limUnit_}) {
        u->blockSignals(true);
        u->setBypassed(false);
        u->blockSignals(false);
    }

    const int nRes = static_cast<int>(autoAdvice_.resonances.size());
    resNoResLabel_->setVisible(nRes == 0);
    resUnit_->blockSignals(true);
    resUnit_->setBypassed(nRes == 0);
    resUnit_->blockSignals(false);

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

void ChainPanel::clear() {
    hasAdvice_  = false;
    autoAdvice_ = mt::AdviceSet{};

    applyToControls(autoAdvice_);

    for (RackUnit* u : {resUnit_, eqUnit_, mbUnit_, satUnit_, widthUnit_, mixbusUnit_, limUnit_}) {
        u->blockSignals(true);
        u->setBypassed(false);
        u->blockSignals(false);
        u->setStats(QString::fromUtf8("\xe2\x80\x94"));
        u->setBadge(QString{});
    }
    resUnit_->blockSignals(true);
    resUnit_->setBypassed(true);   // no analysis → resonance unit is always inactive after clear
    resUnit_->blockSignals(false);

    resNoResLabel_->setVisible(true);
    for (int i = 0; i < kMaxResonances; ++i) {
        resRowLabels_[i]->setVisible(false);
        resRowChecks_[i]->setVisible(false);
    }

    const QString dash = QString::fromUtf8("\xe2\x80\x94");
    for (int i = 0; i < kNumBands; ++i)
        eqReadouts_[i]->setText(dash);
}

VerticalFader* ChainPanel::eqGainFader(int band)   const { return eqGainFaders_[band]; }
RotaryKnob*    ChainPanel::satDriveKnob()           const { return satDriveKnob_; }
RotaryKnob*    ChainPanel::mixbusThreshKnob()       const { return mixbusThreshKnob_; }
RotaryKnob*    ChainPanel::mixbusMakeupKnob()       const { return mixbusMakeupKnob_; }
VerticalFader* ChainPanel::limCeilingFader()        const { return limCeilingFader_; }
RotaryKnob*    ChainPanel::mbRatioKnob(int band)    const { return mbRatioKnobs_[band]; }
VerticalFader* ChainPanel::mbThreshFader(int band)  const { return mbThreshFaders_[band]; }
VerticalFader* ChainPanel::widthFader(int band)     const { return widthFaders_[band]; }
RotaryKnob*    ChainPanel::limTargetKnob()          const { return limTargetKnob_; }
RotaryKnob*    ChainPanel::mixbusRatioKnob()        const { return mixbusRatioKnob_; }

} // namespace gui
