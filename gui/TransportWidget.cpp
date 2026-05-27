#include "TransportWidget.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileInfo>

#include <cstring>

namespace gui {

// Context passed to miniaudio's data callback so it can track playback position.
struct PlaybackCtx {
    ma_decoder* decoder;
    uint64_t*   framePos;  // pointer into TransportWidget::playbackFrame_
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
}

TransportWidget::TransportWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    playBtn_  = new QPushButton("▶ Play", this);
    abBtn_    = new QPushButton("Source: Original", this);
    fileLabel_ = new QLabel("(no output yet)", this);
    fileLabel_->setWordWrap(false);

    layout->addWidget(playBtn_);
    layout->addWidget(abBtn_);
    layout->addWidget(fileLabel_, 1);

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
    originalPath_.clear();
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
    if (ma_decoder_init_file(activePath.c_str(), nullptr, maDecoder_) != MA_SUCCESS) {
        delete maDecoder_; maDecoder_ = nullptr; return;
    }

    // Seek to the remembered position so A/B switches are seamless.
    ma_decoder_seek_to_pcm_frame(maDecoder_, playbackFrame_);

    auto* ctx = new PlaybackCtx{ maDecoder_, &playbackFrame_ };

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format   = maDecoder_->outputFormat;
    cfg.playback.channels = maDecoder_->outputChannels;
    cfg.sampleRate        = maDecoder_->outputSampleRate;
    cfg.dataCallback      = gui::dataCallback;
    cfg.pUserData         = ctx;

    maDevice_ = new ma_device;
    if (ma_device_init(nullptr, &cfg, maDevice_) != MA_SUCCESS ||
        ma_device_start(maDevice_) != MA_SUCCESS) {
        delete ctx;
        cleanup(); return;
    }
    playing_ = true;
    playBtn_->setText("■ Stop");
}

void TransportWidget::stop() {
    if (!playing_) return;
    // Save the PlaybackCtx pointer before cleanup so we can free it.
    PlaybackCtx* ctx = maDevice_ ? static_cast<PlaybackCtx*>(maDevice_->pUserData) : nullptr;
    cleanup();
    delete ctx;
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
        PlaybackCtx* ctx = maDevice_ ? static_cast<PlaybackCtx*>(maDevice_->pUserData) : nullptr;
        // Capture position before cleanup resets nothing (cleanup doesn't touch playbackFrame_).
        cleanup();
        delete ctx;
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
    // Note: playbackFrame_ is NOT reset here — stop() preserves position for resume.
}

} // namespace gui
