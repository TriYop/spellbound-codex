#include "TransportWidget.h"
#include "VUMeterWidget.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <QHBoxLayout>
#include <QFileInfo>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstring>

namespace gui {

// Context passed to miniaudio's data callback so it can track playback position.
struct PlaybackCtx {
    ma_decoder*              decoder;
    std::atomic<uint64_t>*   framePos;  // pointer into TransportWidget::playbackFrame_
    std::atomic<float>*      atomicRmsL;
    std::atomic<float>*      atomicRmsR;
};

static void dataCallback(ma_device* dev, void* out, const void* /*in*/, unsigned int frameCount) {
    auto* ctx = static_cast<PlaybackCtx*>(dev->pUserData);
    if (!ctx || !ctx->decoder) {
        std::memset(out, 0, frameCount * ma_get_bytes_per_frame(dev->playback.format, dev->playback.channels));
        return;
    }
    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(ctx->decoder, out, frameCount, &framesRead);
    if (ctx->framePos)
        ctx->framePos->fetch_add(framesRead, std::memory_order_relaxed);
    if (framesRead < frameCount)
        std::memset(static_cast<char*>(out) + framesRead * ma_get_bytes_per_frame(dev->playback.format, dev->playback.channels),
                    0,
                    (frameCount - framesRead) * ma_get_bytes_per_frame(dev->playback.format, dev->playback.channels));

    // Compute per-channel RMS (output is f32 because decoder is configured that way).
    if (ctx->atomicRmsL && ctx->atomicRmsR && framesRead > 0) {
        const auto*        samples = static_cast<const float*>(out);
        const unsigned int ch      = dev->playback.channels;
        double sumL = 0.0, sumR = 0.0;
        for (ma_uint64 i = 0; i < framesRead; ++i) {
            float l = samples[i * ch];
            float r = (ch > 1) ? samples[i * ch + 1] : l;
            sumL += static_cast<double>(l) * l;
            sumR += static_cast<double>(r) * r;
        }
        ctx->atomicRmsL->store(static_cast<float>(std::sqrt(sumL / static_cast<double>(framesRead))),
                               std::memory_order_relaxed);
        ctx->atomicRmsR->store(static_cast<float>(std::sqrt(sumR / static_cast<double>(framesRead))),
                               std::memory_order_relaxed);
    }
}

static QString formatTime(uint64_t frames, uint32_t sampleRate) {
    if (sampleRate == 0) return "--:--";
    const uint64_t totalSecs = frames / sampleRate;
    const uint64_t h = totalSecs / 3600;
    const uint64_t m = (totalSecs % 3600) / 60;
    const uint64_t s = totalSecs % 60;
    if (h > 0)
        return QString("%1:%2:%3")
            .arg(static_cast<qulonglong>(h))
            .arg(static_cast<qulonglong>(m), 2, 10, QChar('0'))
            .arg(static_cast<qulonglong>(s), 2, 10, QChar('0'));
    return QString("%1:%2")
        .arg(static_cast<qulonglong>(m), 2, 10, QChar('0'))
        .arg(static_cast<qulonglong>(s), 2, 10, QChar('0'));
}

TransportWidget::TransportWidget(QWidget* parent) : QWidget(parent) {
    // ── Row 1: play controls + filename ──────────────────────────────────
    auto* row1 = new QHBoxLayout;
    row1->setContentsMargins(0, 0, 0, 0);

    playBtn_   = new QPushButton("▶ Play", this);
    abBtn_     = new QPushButton("Source: Original", this);
    fileLabel_ = new QLabel("(no output yet)", this);
    fileLabel_->setWordWrap(false);

    row1->addWidget(playBtn_);
    row1->addWidget(abBtn_);
    row1->addWidget(fileLabel_, 1);

    // ── Row 2: clock + scrub bar ─────────────────────────────────────────
    auto* row2 = new QHBoxLayout;
    row2->setContentsMargins(0, 0, 0, 0);

    clockLabel_ = new QLabel("--:-- / --:--", this);
    {
        QFont f("monospace", 14, QFont::Bold);
        clockLabel_->setFont(f);
        clockLabel_->setStyleSheet("color: #ffffff;");
        clockLabel_->setFixedWidth(
            clockLabel_->fontMetrics().horizontalAdvance("0:00:00 / 0:00:00") + 8);
    }
    clockLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    scrubSlider_ = new QSlider(Qt::Horizontal, this);
    scrubSlider_->setRange(0, 1);
    scrubSlider_->setValue(0);
    scrubSlider_->setEnabled(false);
    scrubSlider_->setStyleSheet(
        "QSlider::groove:horizontal {"
        "  height: 4px; background: #3a3a3a; border-radius: 2px; }"
        "QSlider::sub-page:horizontal {"
        "  background: #2a6099; border-radius: 2px; }"
        "QSlider::handle:horizontal {"
        "  width: 12px; height: 12px; margin: -4px 0;"
        "  background: #ffffff; border-radius: 6px; }"
    );

    row2->addWidget(clockLabel_);
    row2->addWidget(scrubSlider_, 1);

    // ── Position timer (not started yet — started in play()) ─────────────
    posTimer_ = new QTimer(this);
    posTimer_->setInterval(100);  // 10 Hz

    // ── Left column (both rows) ──────────────────────────────────────────
    auto* leftCol = new QVBoxLayout;
    leftCol->setContentsMargins(0, 0, 0, 0);
    leftCol->setSpacing(4);
    leftCol->addLayout(row1);
    leftCol->addLayout(row2);

    // ── VU meter spans both rows on the right ────────────────────────────
    vuMeter_ = new VUMeterWidget(&atomicRmsL_, &atomicRmsR_, this);

    // ── Outer layout ─────────────────────────────────────────────────────
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(8);
    outer->addLayout(leftCol, 1);
    outer->addWidget(vuMeter_);

    // ── Initial button state ─────────────────────────────────────────────
    playBtn_->setEnabled(false);
    abBtn_->setEnabled(false);
    abBtn_->setVisible(false);

    // ── Signal connections ────────────────────────────────────────────────
    connect(playBtn_,    &QPushButton::clicked,    this, &TransportWidget::onPlayStop);
    connect(abBtn_,      &QPushButton::clicked,    this, &TransportWidget::onAbToggle);
    connect(scrubSlider_,&QSlider::sliderReleased, this, &TransportWidget::onScrubReleased);
    connect(posTimer_,   &QTimer::timeout,         this, &TransportWidget::onPositionTick);
}

TransportWidget::~TransportWidget() {
    cleanup();
}

void TransportWidget::loadFile(const QString& path) {
    cleanup();
    playbackFrame_.store(0, std::memory_order_relaxed);
    loadedPath_ = path.toStdString();
    fileLabel_->setText(QFileInfo(path).fileName());
    playBtn_->setEnabled(true);
    playBtn_->setText("▶ Play");

    // Query total duration via a temporary decoder (opened/closed immediately).
    totalFrames_ = 0;
    sampleRate_  = 0;
    ma_decoder   tmpDecoder;
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
    if (ma_decoder_init_file(loadedPath_.c_str(), &cfg, &tmpDecoder) == MA_SUCCESS) {
        ma_uint64 len = 0;
        if (ma_decoder_get_length_in_pcm_frames(&tmpDecoder, &len) == MA_SUCCESS)
            totalFrames_ = len;
        sampleRate_ = tmpDecoder.outputSampleRate;
        ma_decoder_uninit(&tmpDecoder);
    }

    if (totalFrames_ > 0 && sampleRate_ > 0) {
        scrubSlider_->setRange(0, static_cast<int>(
            std::min(totalFrames_, static_cast<uint64_t>(INT_MAX))));
        scrubSlider_->setValue(0);
        scrubSlider_->setEnabled(true);
        clockLabel_->setText("00:00 / " + formatTime(totalFrames_, sampleRate_));
    } else {
        scrubSlider_->setRange(0, 1);
        scrubSlider_->setValue(0);
        scrubSlider_->setEnabled(false);
        clockLabel_->setText("--:-- / --:--");
    }

    updateAbButton();
}

void TransportWidget::setOriginalFile(const QString& path) {
    originalPath_ = path;
    // Show original filename in label so user knows a source is loaded.
    if (!path.isEmpty())
        fileLabel_->setText(QFileInfo(path).fileName());
    updateAbButton();
}

void TransportWidget::unload() {
    cleanup();
    loadedPath_.clear();
    // originalPath_ is NOT cleared here; it is managed by setOriginalFile().
    useOriginal_  = false;
    playbackFrame_.store(0, std::memory_order_relaxed);
    totalFrames_  = 0;
    sampleRate_   = 0;
    fileLabel_->setText("(no output yet)");
    playBtn_->setEnabled(false);
    playBtn_->setText("▶ Play");
    abBtn_->setText("Source: Original");
    abBtn_->setEnabled(false);
    abBtn_->setVisible(false);
    scrubSlider_->setRange(0, 1);
    scrubSlider_->setValue(0);
    scrubSlider_->setEnabled(false);
    clockLabel_->setText("--:-- / --:--");
}

void TransportWidget::play() {
    // Decide which file to open based on A/B state.
    const std::string activePath = (useOriginal_ && !originalPath_.isEmpty())
        ? originalPath_.toStdString()
        : loadedPath_;

    if (activePath.empty()) return;
    if (playing_) return;

    maDecoder_ = new ma_decoder;
    ma_decoder_config decoderCfg = ma_decoder_config_init(ma_format_f32, 0, 0);
    if (ma_decoder_init_file(activePath.c_str(), &decoderCfg, maDecoder_) != MA_SUCCESS) {
        delete maDecoder_; maDecoder_ = nullptr; return;
    }

    // Seek to the remembered position so A/B switches are seamless.
    ma_decoder_seek_to_pcm_frame(maDecoder_, playbackFrame_.load(std::memory_order_relaxed));

    auto* ctx = new PlaybackCtx{ maDecoder_, &playbackFrame_, &atomicRmsL_, &atomicRmsR_ };

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format   = maDecoder_->outputFormat;
    cfg.playback.channels = maDecoder_->outputChannels;
    cfg.sampleRate        = maDecoder_->outputSampleRate;
    cfg.dataCallback      = gui::dataCallback;
    cfg.pUserData         = ctx;

    maDevice_ = new ma_device;
    if (ma_device_init(nullptr, &cfg, maDevice_) != MA_SUCCESS) {
        // maDevice_ was not initialized: do NOT touch maDevice_ struct fields (UB).
        // Free ctx manually and tear down decoder; skip ma_device_stop/uninit.
        delete ctx;
        delete maDevice_; maDevice_ = nullptr;
        ma_decoder_uninit(maDecoder_);
        delete maDecoder_; maDecoder_ = nullptr;
        return;
    }
    // From here maDevice_->pUserData == ctx; cleanup() owns deletion.
    if (ma_device_start(maDevice_) != MA_SUCCESS) {
        cleanup(); return;
    }
    playing_ = true;
    playBtn_->setText("■ Stop");
    vuMeter_->startMeter();
    posTimer_->start();
}

void TransportWidget::stop() {
    if (!playing_) return;
    cleanup();
    // playbackFrame_ is intentionally preserved so next play() resumes here.
    playBtn_->setText("▶ Play");
}

void TransportWidget::onPlayStop() {
    if (playing_) stop();
    else          play();
}

void TransportWidget::onAbToggle() {
    useOriginal_ = !useOriginal_;
    updateAbButton();

    if (playing_) {
        // Stop current device, then restart from the same position with the other file.
        // cleanup() owns PlaybackCtx deletion.
        cleanup();
        playBtn_->setText("▶ Play"); // cleanup sets playing_=false; play() will re-set to Stop
        play();
    }
}

void TransportWidget::onPositionTick() {
    if (sampleRate_ == 0) return;
    const uint64_t frame = playbackFrame_.load(std::memory_order_relaxed);
    if (!scrubSlider_->isSliderDown())
        scrubSlider_->setValue(static_cast<int>(
            std::min(frame, static_cast<uint64_t>(INT_MAX))));
    const uint64_t displayFrame = scrubSlider_->isSliderDown()
        ? static_cast<uint64_t>(scrubSlider_->value())
        : frame;
    clockLabel_->setText(
        formatTime(displayFrame, sampleRate_) + " / " +
        formatTime(totalFrames_, sampleRate_));
}

void TransportWidget::onScrubReleased() {
    const uint64_t frame = static_cast<uint64_t>(scrubSlider_->value());
    const bool wasPlaying = playing_;
    if (wasPlaying) cleanup();
    playbackFrame_.store(frame, std::memory_order_relaxed);
    if (wasPlaying) {
        play();
    } else {
        // Update clock to reflect new position even when stopped.
        if (sampleRate_ > 0)
            clockLabel_->setText(
                formatTime(frame, sampleRate_) + " / " +
                formatTime(totalFrames_, sampleRate_));
    }
}

void TransportWidget::updateAbButton() {
    const bool bothAvailable = !loadedPath_.empty() && !originalPath_.isEmpty();
    abBtn_->setVisible(bothAvailable);
    abBtn_->setEnabled(bothAvailable);
    abBtn_->setText(useOriginal_ ? "Source: Original" : "Source: Rendered");
}

void TransportWidget::cleanup() {
    if (maDevice_) {
        ma_device_stop(maDevice_);   // blocks until audio thread exits — must precede ctx delete
        delete static_cast<PlaybackCtx*>(maDevice_->pUserData);
        maDevice_->pUserData = nullptr;
        ma_device_uninit(maDevice_);
        delete maDevice_;
        maDevice_ = nullptr;
    }
    if (maDecoder_) {
        ma_decoder_uninit(maDecoder_);
        delete maDecoder_;
        maDecoder_ = nullptr;
    }
    playing_ = false;
    atomicRmsL_.store(0.f, std::memory_order_relaxed);
    atomicRmsR_.store(0.f, std::memory_order_relaxed);
    if (vuMeter_)  vuMeter_->stopMeter();
    if (posTimer_) posTimer_->stop();
    // Note: playbackFrame_ is NOT reset here — stop() preserves position for resume.
}

} // namespace gui
