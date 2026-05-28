#include "TransportWidget.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileInfo>

#include "VUMeterWidget.h"

#include <cstring>
#include <atomic>
#include <cmath>

namespace gui {

// Context passed to miniaudio's data callback so it can track playback position.
struct PlaybackCtx {
    ma_decoder*         decoder;
    uint64_t*           framePos;  // pointer into TransportWidget::playbackFrame_
    std::atomic<float>* atomicRmsL;
    std::atomic<float>* atomicRmsR;
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
        *ctx->framePos += framesRead;
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

TransportWidget::TransportWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    playBtn_   = new QPushButton("▶ Play", this);
    abBtn_     = new QPushButton("Source: Original", this);
    fileLabel_ = new QLabel("(no output yet)", this);
    fileLabel_->setWordWrap(false);

    vuMeter_ = new VUMeterWidget(&atomicRmsL_, &atomicRmsR_, this);

    layout->addWidget(playBtn_);
    layout->addWidget(abBtn_);
    layout->addWidget(fileLabel_, 1);
    layout->addWidget(vuMeter_);

    playBtn_->setEnabled(false);
    abBtn_->setEnabled(false);
    abBtn_->setVisible(false);

    connect(playBtn_, &QPushButton::clicked, this, &TransportWidget::onPlayStop);
    connect(abBtn_,   &QPushButton::clicked, this, &TransportWidget::onAbToggle);
}

TransportWidget::~TransportWidget() {
    cleanup();
}

void TransportWidget::loadFile(const QString& path) {
    cleanup();
    playbackFrame_ = 0;
    loadedPath_ = path.toStdString();
    fileLabel_->setText(QFileInfo(path).fileName());
    playBtn_->setEnabled(true);
    playBtn_->setText("▶ Play");
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
    useOriginal_   = false;
    playbackFrame_ = 0;
    fileLabel_->setText("(no output yet)");
    playBtn_->setEnabled(false);
    playBtn_->setText("▶ Play");
    abBtn_->setText("Source: Original");
    abBtn_->setEnabled(false);
    abBtn_->setVisible(false);
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
    ma_decoder_seek_to_pcm_frame(maDecoder_, playbackFrame_);

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

void TransportWidget::updateAbButton() {
    const bool bothAvailable = !loadedPath_.empty() && !originalPath_.isEmpty();
    abBtn_->setVisible(bothAvailable);
    abBtn_->setEnabled(bothAvailable);
    abBtn_->setText(useOriginal_ ? "Source: Original" : "Source: Rendered");
}

void TransportWidget::cleanup() {
    if (maDevice_) {
        delete static_cast<PlaybackCtx*>(maDevice_->pUserData);
        maDevice_->pUserData = nullptr;
        ma_device_stop(maDevice_);
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
    // Note: playbackFrame_ is NOT reset here — stop() preserves position for resume.
}

} // namespace gui
