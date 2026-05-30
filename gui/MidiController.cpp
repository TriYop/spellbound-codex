#include "MidiController.h"

#include "RtMidi.h"

namespace gui {

MidiController::MidiController(QObject* parent)
    : QObject(parent)
    , in_(std::make_unique<RtMidiIn>())
    , out_(std::make_unique<RtMidiOut>())
{
    in_->setCallback(&MidiController::rtCallback, this);
    // Ignore timing clock and active sensing; pass SysEx through (ignored anyway)
    in_->ignoreTypes(false, true, true);
}

MidiController::~MidiController() {
    closeInput();
    closeOutput();
}

std::vector<QString> MidiController::inputPortNames() const {
    std::vector<QString> names;
    const unsigned n = in_->getPortCount();
    names.reserve(n);
    for (unsigned i = 0; i < n; ++i)
        names.push_back(QString::fromStdString(in_->getPortName(i)));
    return names;
}

std::vector<QString> MidiController::outputPortNames() const {
    std::vector<QString> names;
    const unsigned n = out_->getPortCount();
    names.reserve(n);
    for (unsigned i = 0; i < n; ++i)
        names.push_back(QString::fromStdString(out_->getPortName(i)));
    return names;
}

bool MidiController::openInput(unsigned portIndex) {
    closeInput();
    try {
        in_->openPort(portIndex);
        inPort_ = static_cast<int>(portIndex);
        return true;
    } catch (...) {
        return false;
    }
}

bool MidiController::openOutput(unsigned portIndex) {
    closeOutput();
    try {
        out_->openPort(portIndex);
        outPort_ = static_cast<int>(portIndex);
        return true;
    } catch (...) {
        return false;
    }
}

void MidiController::closeInput() {
    if (in_->isPortOpen()) in_->closePort();
    inPort_ = -1;
}

void MidiController::closeOutput() {
    if (out_->isPortOpen()) out_->closePort();
    outPort_ = -1;
}

void MidiController::sendCC(int channel, int cc, int value) {
    if (!out_->isPortOpen()) return;
    std::vector<unsigned char> msg = {
        static_cast<unsigned char>(0xB0 | (channel & 0x0F)),
        static_cast<unsigned char>(cc    & 0x7F),
        static_cast<unsigned char>(value & 0x7F)
    };
    try { out_->sendMessage(&msg); } catch (...) {}
}

void MidiController::rtCallback(double /*timestamp*/,
                                 std::vector<unsigned char>* msg,
                                 void*                       userData) {
    if (!msg || msg->size() < 2) return;
    auto* self = static_cast<MidiController*>(userData);

    const int status  = (*msg)[0];
    const int data1   = (*msg)[1];
    const int data2   = (msg->size() >= 3) ? static_cast<int>((*msg)[2]) : 0;
    const int channel = status & 0x0F;
    const int type    = status & 0xF0;

    // Qt AutoConnection detects cross-thread signals and queues them automatically.
    // 'int' is a registered metatype, so no qRegisterMetaType call is needed.
    if (type == 0xB0)
        emit self->ccReceived(channel, data1, data2);
    else if (type == 0x90 && data2 > 0)
        emit self->noteOnReceived(channel, data1, data2);
    else if (type == 0x80 || (type == 0x90 && data2 == 0))
        emit self->noteOffReceived(channel, data1);
}

} // namespace gui
