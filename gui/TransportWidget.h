#pragma once

#include <QWidget>
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

    static void dataCallback(ma_device* dev, void* out, const void* /*in*/, unsigned int frameCount);

    QPushButton* playBtn_  = nullptr;
    QPushButton* abBtn_    = nullptr;
    QLabel*      fileLabel_ = nullptr;

    ma_device*   maDevice_  = nullptr;
    ma_decoder*  maDecoder_ = nullptr;
    bool         playing_   = false;
    std::string  loadedPath_;

    QString      originalPath_;
    bool         useOriginal_  = false;
    uint64_t     playbackFrame_ = 0;
};

} // namespace gui
