#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

class RtMidiIn;
class RtMidiOut;

namespace gui {

// Wraps RtMidi. MIDI-in runs on RtMidi's background thread; the static
// callback emits Qt signals which Qt queues to the GUI thread via
// AutoConnection (works from non-QThread threads for registered types like int).
class MidiController : public QObject {
    Q_OBJECT
public:
    explicit MidiController(QObject* parent = nullptr);
    ~MidiController() override;

    // Port enumeration — safe to call any time, no port needs to be open.
    std::vector<QString> inputPortNames()  const;
    std::vector<QString> outputPortNames() const;

    // Open by zero-based index. Closes any currently open port first.
    // Returns false if RtMidi throws.
    bool openInput(unsigned portIndex);
    bool openOutput(unsigned portIndex);
    void closeInput();
    void closeOutput();

    int currentInputPort()  const { return inPort_;  }
    int currentOutputPort() const { return outPort_; }

    // Send a CC message to the output port. No-op if no output port is open.
    void sendCC(int channel, int cc, int value);

signals:
    // All emitted on the GUI thread via Qt's cross-thread queuing.
    void ccReceived(int channel, int cc, int value);
    void noteOnReceived(int channel, int note, int velocity);
    void noteOffReceived(int channel, int note);

private:
    static void rtCallback(double                       timestamp,
                           std::vector<unsigned char>* msg,
                           void*                       userData);

    std::unique_ptr<RtMidiIn>  in_;
    std::unique_ptr<RtMidiOut> out_;
    int inPort_  = -1;
    int outPort_ = -1;
};

} // namespace gui
