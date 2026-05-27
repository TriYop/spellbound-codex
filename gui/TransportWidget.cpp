#include "TransportWidget.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <cstring>

namespace gui {

static void dataCallback(ma_device* dev, void* out, const void* /*in*/, unsigned int frameCount) {
    auto* decoder = static_cast<ma_decoder*>(dev->pUserData);
    if (!decoder) { std::memset(out, 0, frameCount * ma_get_bytes_per_frame(dev->playback.format, dev->playback.channels)); return; }
    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(decoder, out, frameCount, &framesRead);
    if (framesRead < frameCount)
        std::memset(static_cast<char*>(out) + framesRead * ma_get_bytes_per_frame(dev->playback.format, dev->playback.channels),
                    0,
                    (frameCount - framesRead) * ma_get_bytes_per_frame(dev->playback.format, dev->playback.channels));
}

TransportWidget::TransportWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    playBtn_  = new QPushButton("▶ Play", this);
    fileLabel_ = new QLabel("(no output yet)", this);
    fileLabel_->setWordWrap(false);

    layout->addWidget(playBtn_);
    layout->addWidget(fileLabel_, 1);

    playBtn_->setEnabled(false);

    connect(playBtn_, &QPushButton::clicked, this, &TransportWidget::onPlayStop);
}

TransportWidget::~TransportWidget() {
    cleanup();
}

void TransportWidget::loadFile(const QString& path) {
    cleanup();
    loadedPath_ = path.toStdString();
    fileLabel_->setText(path);
    playBtn_->setEnabled(true);
    playBtn_->setText("▶ Play");
}

void TransportWidget::unload() {
    cleanup();
    loadedPath_.clear();
    fileLabel_->setText("(no output yet)");
    playBtn_->setEnabled(false);
    playBtn_->setText("▶ Play");
}

void TransportWidget::play() {
    if (loadedPath_.empty()) return;
    if (playing_) return;

    maDecoder_ = new ma_decoder;
    if (ma_decoder_init_file(loadedPath_.c_str(), nullptr, maDecoder_) != MA_SUCCESS) {
        delete maDecoder_; maDecoder_ = nullptr; return;
    }

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format   = maDecoder_->outputFormat;
    cfg.playback.channels = maDecoder_->outputChannels;
    cfg.sampleRate        = maDecoder_->outputSampleRate;
    cfg.dataCallback      = gui::dataCallback;
    cfg.pUserData         = maDecoder_;

    maDevice_ = new ma_device;
    if (ma_device_init(nullptr, &cfg, maDevice_) != MA_SUCCESS ||
        ma_device_start(maDevice_) != MA_SUCCESS) {
        cleanup(); return;
    }
    playing_ = true;
    playBtn_->setText("■ Stop");
}

void TransportWidget::stop() {
    if (!playing_) return;
    cleanup();
    playBtn_->setText("▶ Play");
}

void TransportWidget::onPlayStop() {
    if (playing_) stop();
    else          play();
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
}

} // namespace gui
