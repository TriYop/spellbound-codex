#pragma once

#include <QWidget>
#include <atomic>
#include <cstdint>
#include <string>

QT_BEGIN_NAMESPACE
class QPushButton;
class QLabel;
class QSlider;
QT_END_NAMESPACE

struct ma_device;
struct ma_decoder;

namespace gui {

class VUMeterWidget;

// Simple audio transport: play / stop the last rendered output file via miniaudio.
class TransportWidget : public QWidget {
    Q_OBJECT
public:
    explicit TransportWidget(QWidget* parent = nullptr);
    ~TransportWidget() override;

    // Load a rendered file for preview playback.
    void loadFile(const QString& path);
    // Set the original (pre-mastering) file for A/B comparison.
    void setOriginalFile(const QString& path);
    void unload();

public slots:
    void play();
    void stop();

private slots:
    void onPlayStop();
    void onAbToggle();

private:
    void cleanup();
    void updateAbButton();

    QPushButton* playBtn_  = nullptr;
    QPushButton* abBtn_    = nullptr;
    QLabel*      fileLabel_ = nullptr;

    ma_device*   maDevice_  = nullptr;
    ma_decoder*  maDecoder_ = nullptr;
    bool         playing_   = false;
    std::string  loadedPath_;

    QString      originalPath_;
    bool         useOriginal_  = false;
    std::atomic<uint64_t> playbackFrame_{0};

    std::atomic<float> atomicRmsL_{0.f};
    std::atomic<float> atomicRmsR_{0.f};
    VUMeterWidget*     vuMeter_ = nullptr;
};

} // namespace gui
