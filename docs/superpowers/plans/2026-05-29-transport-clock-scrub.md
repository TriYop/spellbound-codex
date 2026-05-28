# Transport Clock & Scrub Bar — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a large monospace playback-position clock and a seekable horizontal scrub bar to `TransportWidget`, with the VU meter spanning the full height of both rows.

**Architecture:** All changes are inline in `gui/TransportWidget.h` and `gui/TransportWidget.cpp` — no new files. `playbackFrame_` becomes `std::atomic<uint64_t>` (fixing a pre-existing data race). A 10 Hz `QTimer` drives the clock label and slider. Scrubbing uses the existing stop→seek→restart pattern from A/B toggle.

**Tech Stack:** C++20, Qt6 (QSlider, QLabel, QTimer, QVBoxLayout, QHBoxLayout), miniaudio 0.11.21 (already vendored).

---

## File Map

| Action | Path | What changes |
|---|---|---|
| Modify | `gui/TransportWidget.h` | `playbackFrame_` type, 5 new members, 2 new private slots |
| Modify | `gui/TransportWidget.cpp` | All implementation changes (layout, formatTime, loadFile, unload, play, cleanup, new slots) |

---

## Task 1: Fix `playbackFrame_` data race — make it `std::atomic<uint64_t>`

**Files:**
- Modify: `gui/TransportWidget.h`
- Modify: `gui/TransportWidget.cpp`

This is a pre-existing data race: `playbackFrame_` is written by the audio callback thread and read by the main thread without synchronisation. Change it to `std::atomic<uint64_t>` before adding new readers.

- [ ] **Step 1: Update `gui/TransportWidget.h`**

Change the `playbackFrame_` member declaration (currently around line 57):

```cpp
// Change:
uint64_t     playbackFrame_ = 0;
// To:
std::atomic<uint64_t> playbackFrame_{0};
```

- [ ] **Step 2: Update `PlaybackCtx` in `gui/TransportWidget.cpp`**

`PlaybackCtx` is a struct local to `TransportWidget.cpp`. Change `framePos` to an atomic pointer:

```cpp
// Change:
struct PlaybackCtx {
    ma_decoder*         decoder;
    uint64_t*           framePos;  // pointer into TransportWidget::playbackFrame_
    std::atomic<float>* atomicRmsL;
    std::atomic<float>* atomicRmsR;
};
// To:
struct PlaybackCtx {
    ma_decoder*              decoder;
    std::atomic<uint64_t>*   framePos;
    std::atomic<float>*      atomicRmsL;
    std::atomic<float>*      atomicRmsR;
};
```

- [ ] **Step 3: Update the audio callback to use `fetch_add`**

In `dataCallback`, replace the plain pointer increment:

```cpp
// Change:
if (ctx->framePos)
    *ctx->framePos += framesRead;
// To:
if (ctx->framePos)
    ctx->framePos->fetch_add(framesRead, std::memory_order_relaxed);
```

- [ ] **Step 4: Update all main-thread reads and writes of `playbackFrame_`**

In `gui/TransportWidget.cpp`, make every access explicit. Apply these changes:

In `loadFile()`:
```cpp
// Change:
playbackFrame_ = 0;
// To:
playbackFrame_.store(0, std::memory_order_relaxed);
```

In `unload()`:
```cpp
// Change:
playbackFrame_ = 0;
// To:
playbackFrame_.store(0, std::memory_order_relaxed);
```

In `play()` — the seek call:
```cpp
// Change:
ma_decoder_seek_to_pcm_frame(maDecoder_, playbackFrame_);
// To:
ma_decoder_seek_to_pcm_frame(maDecoder_, playbackFrame_.load(std::memory_order_relaxed));
```

In `play()` — the `PlaybackCtx` construction (pass address of the atomic, type already matches):
```cpp
// No change needed — &playbackFrame_ still works; PlaybackCtx::framePos is now std::atomic<uint64_t>*
```

- [ ] **Step 5: Build to verify**

```bash
cd /home/yvan/Projects/AudioPlugins/MasterTweak
cmake --build build --parallel 2>&1 | grep -E "error:|Linking" | head -10
```

Expected: `Linking CXX executable gui/MasterTweak` with no errors.

- [ ] **Step 6: Run tests**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -5
```

Expected: `100% tests passed, 0 tests failed out of 1`

- [ ] **Step 7: Commit**

```bash
git add gui/TransportWidget.h gui/TransportWidget.cpp
git commit -m "fix: make playbackFrame_ atomic to eliminate audio/UI thread data race"
```

---

## Task 2: Add `formatTime()` helper

**Files:**
- Modify: `gui/TransportWidget.cpp`

Add the `formatTime` static free function. It lives in `TransportWidget.cpp` only — it is a TU-local implementation detail not accessible from the test executable, so no unit test is added here. Its logic is verified manually in Task 5.

- [ ] **Step 1: Add `formatTime()` before the `TransportWidget` constructor**

In `gui/TransportWidget.cpp`, add this function in the `gui` namespace, after the `dataCallback` function and before the constructor:

```cpp
static QString formatTime(uint64_t frames, uint32_t sampleRate) {
    if (sampleRate == 0) return "--:--";
    const uint64_t totalSecs = frames / sampleRate;
    const uint64_t h = totalSecs / 3600;
    const uint64_t m = (totalSecs % 3600) / 60;
    const uint64_t s = totalSecs % 60;
    if (h > 0)
        return QString("%1:%2:%3")
            .arg(h)
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    return QString("%1:%2")
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}
```

- [ ] **Step 2: Add required includes at the top of `TransportWidget.cpp`**

After the existing Qt includes, add the missing headers:

```cpp
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileInfo>
#include <QFont>        // add
#include <QSlider>      // add
#include <QTimer>       // add
#include <QVBoxLayout>  // add
```

And in the stdlib block:
```cpp
#include <algorithm>  // add — for std::min
#include <atomic>
#include <climits>    // add — for INT_MAX
#include <cmath>
#include <cstring>
```

- [ ] **Step 3: Build to verify**

```bash
cmake --build build --parallel 2>&1 | grep -E "error:|Linking" | head -10
```

Expected: links cleanly.

- [ ] **Step 4: Commit**

```bash
git add gui/TransportWidget.cpp
git commit -m "feat: add formatTime() helper for MM:SS / H:MM:SS clock display"
```

---

## Task 3: Add new members to header; update `loadFile()` and `unload()`

**Files:**
- Modify: `gui/TransportWidget.h`
- Modify: `gui/TransportWidget.cpp`

Declare the five new members and two new private slots. Implement duration discovery in `loadFile()` and reset logic in `unload()`. The widgets are created but not laid out yet (that is Task 4).

- [ ] **Step 1: Update `gui/TransportWidget.h`**

Add forward declarations and new members. The full updated private section should be:

```cpp
QT_BEGIN_NAMESPACE
class QPushButton;
class QLabel;
class QSlider;   // already forward-declared — keep it
class QTimer;    // add
QT_END_NAMESPACE
```

Add two new private slots (inside the `private slots:` section, after `onAbToggle`):

```cpp
private slots:
    void onPlayStop();
    void onAbToggle();
    void onPositionTick();   // add
    void onScrubReleased();  // add
```

Add new private data members after `vuMeter_`:

```cpp
    VUMeterWidget*        vuMeter_      = nullptr;

    // Position display
    uint64_t              totalFrames_  = 0;
    uint32_t              sampleRate_   = 0;
    QTimer*               posTimer_     = nullptr;
    QLabel*               clockLabel_   = nullptr;
    QSlider*              scrubSlider_  = nullptr;
```

- [ ] **Step 2: Update `loadFile()` in `gui/TransportWidget.cpp`**

Replace the body of `loadFile()` with:

```cpp
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
```

- [ ] **Step 3: Update `unload()` in `gui/TransportWidget.cpp`**

Replace the body of `unload()` with:

```cpp
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
```

- [ ] **Step 4: Build to verify**

```bash
cmake --build build --parallel 2>&1 | grep -E "error:|Linking" | head -10
```

Expected: links cleanly. The new pointer members are null at this point — `loadFile()` and `unload()` will dereference them, but those methods are never called before the constructor completes. The app must not be run until Task 4 constructs the widgets.

- [ ] **Step 5: Commit**

```bash
git add gui/TransportWidget.h gui/TransportWidget.cpp
git commit -m "feat: add clock/scrub members to header; duration scan in loadFile; reset in unload"
```

---

## Task 4: Restructure layout and construct new widgets

**Files:**
- Modify: `gui/TransportWidget.cpp` — rewrite constructor

This replaces the single `QHBoxLayout` with a two-row left column + VU meter on the right.

- [ ] **Step 1: Rewrite the constructor in `gui/TransportWidget.cpp`**

Replace the entire `TransportWidget::TransportWidget(QWidget* parent)` body:

```cpp
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
        // Fix width to the widest possible string so the scrub bar doesn't jump.
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
```

- [ ] **Step 2: Build**

```bash
cmake --build build --parallel 2>&1 | grep -E "error:|Linking" | head -10
```

Expected: links cleanly (`Linking CXX executable gui/MasterTweak`).

- [ ] **Step 3: Run the app and visually verify the layout**

```bash
./build/gui/MasterTweak
```

Check:
- Two rows in the transport: top row has Play/A-B/filename; bottom row has `--:-- / --:--` and a disabled grey slider
- VU meter on the right, visually taller (spans both rows)
- No layout warnings in the terminal

- [ ] **Step 4: Commit**

```bash
git add gui/TransportWidget.cpp
git commit -m "feat: restructure transport layout — two-row left column + VU meter spanning full height"
```

---

## Task 5: Implement `onPositionTick()`, `onScrubReleased()`, and timer lifecycle

**Files:**
- Modify: `gui/TransportWidget.cpp`

Wire up the 10 Hz timer and scrub slot, start/stop the timer in `play()`/`cleanup()`.

- [ ] **Step 1: Add `onPositionTick()` to `gui/TransportWidget.cpp`**

Add this method after `updateAbButton()`:

```cpp
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
```

- [ ] **Step 2: Add `onScrubReleased()` to `gui/TransportWidget.cpp`**

Add this method after `onPositionTick()`:

```cpp
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
```

- [ ] **Step 3: Start `posTimer_` in `play()`**

In `play()`, after `vuMeter_->startMeter()`:

```cpp
    playing_ = true;
    playBtn_->setText("■ Stop");
    vuMeter_->startMeter();
    posTimer_->start();  // add this line
```

- [ ] **Step 4: Stop `posTimer_` in `cleanup()`**

In `cleanup()`, after `if (vuMeter_) vuMeter_->stopMeter();`:

```cpp
    if (vuMeter_)   vuMeter_->stopMeter();
    if (posTimer_)  posTimer_->stop();   // add this line
```

- [ ] **Step 5: Build**

```bash
cmake --build build --parallel 2>&1 | grep -E "error:|Linking" | head -10
```

Expected: links cleanly.

- [ ] **Step 6: Run tests**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -5
```

Expected: `100% tests passed, 0 tests failed out of 1`

- [ ] **Step 7: Manual verification**

```bash
./build/gui/MasterTweak
```

Work through the checklist:
- [ ] Load a WAV/FLAC file → clock shows `00:00 / MM:SS`, slider enabled and at left
- [ ] Click Play → clock counts up, slider moves right in sync
- [ ] Click Stop → clock and slider freeze at current position
- [ ] Click Play again → resumes from frozen position (not from start)
- [ ] While playing: drag slider to a new position → clock updates to preview time as you drag; on release, playback briefly stops then resumes from new position
- [ ] While stopped: drag slider → clock updates; click Play → starts from dragged position
- [ ] Click Unload (or load a new file) → clock resets to `--:-- / --:--`, slider resets and disables

- [ ] **Step 8: Commit**

```bash
git add gui/TransportWidget.cpp
git commit -m "feat: add position clock and scrub bar — tick timer, seek-on-release"
```

---

## Task 6: Reset clock on stop (not just on cleanup)

**Files:**
- Modify: `gui/TransportWidget.cpp`

When the user clicks Stop, the clock should stay at the current position (resume support). This is already correct — `cleanup()` does NOT reset `playbackFrame_`, so the clock shows the stopped position. No code change needed here. Verify during manual check in Task 5.

If the file reaches its end naturally and playback stops, the clock will show the final position. This is correct behaviour.

*This task is a verification checkpoint only — no code to write.*

- [ ] **Step 1: Confirm during Task 5 manual verification that stopping and resuming works**

Already covered by Task 5 Step 7. Mark complete after verifying.

---

## Task 7: Update plan docs

**Files:**
- Modify: `docs/superpowers/plans/2026-05-29-transport-clock-scrub.md` — mark tasks complete as you go (handled by checkbox tracking)

No separate commit needed — this file is the plan itself.
