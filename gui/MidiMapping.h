#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

namespace gui {

// Controllable parameters — order is stable (used as array index).
enum class MidiParam {
    EqBand0, EqBand1, EqBand2, EqBand3, EqBand4, EqBand5, EqBand6,
    SatDrive,
    MixbusThresh,
    MixbusMakeup,
    LimCeiling,
    CcCount,   // sentinel: number of CC-controlled params
    TransportPlay = static_cast<int>(CcCount),
    TransportStop,
    Count
};

inline constexpr int kMidiParamCcCount = static_cast<int>(MidiParam::CcCount);
inline constexpr int kMidiParamCount   = static_cast<int>(MidiParam::Count);

struct CcBinding {
    int cc      = -1;  // -1 = unbound
    int channel = -1;  // -1 = any channel
};

struct NoteBinding {
    int note    = -1;  // MIDI note number, OR CC# for CC-based transport (e.g. nanoKONTROL2); -1 = unbound
    int channel = -1;  // -1 = any channel
};

struct MidiMap {
    std::array<CcBinding,   kMidiParamCcCount> ccBindings{};
    std::array<NoteBinding, 2>                 noteBindings{};  // [0]=Play [1]=Stop

    CcBinding& ccFor(MidiParam p) {
        assert(static_cast<int>(p) < kMidiParamCcCount && "ccFor: param is not CC-controlled");
        return ccBindings[static_cast<int>(p)];
    }
    const CcBinding& ccFor(MidiParam p) const {
        assert(static_cast<int>(p) < kMidiParamCcCount && "ccFor: param is not CC-controlled");
        return ccBindings[static_cast<int>(p)];
    }
    NoteBinding& noteFor(MidiParam p) {
        assert(static_cast<int>(p) >= kMidiParamCcCount && static_cast<int>(p) < kMidiParamCount
               && "noteFor: param is not Note-controlled");
        return noteBindings[static_cast<int>(p) - kMidiParamCcCount];
    }
    const NoteBinding& noteFor(MidiParam p) const {
        assert(static_cast<int>(p) >= kMidiParamCcCount && static_cast<int>(p) < kMidiParamCount
               && "noteFor: param is not Note-controlled");
        return noteBindings[static_cast<int>(p) - kMidiParamCcCount];
    }

    // Korg nanoKONTROL2 — Scene 1 defaults
    // Faders 1-7: CC 0-6 → EQ bands 0-6
    // Knob 1: CC 16 → SatDrive,  Knob 2: CC 17 → MixbusThresh
    // Knob 3: CC 18 → MixbusMakeup,  Knob 4: CC 19 → LimCeiling
    // Transport: nanoKONTROL2 sends CC (not Note) for Play/Stop.
    // We store the CC# in noteBindings[].note and check it in the CC handler.
    // Play=CC41 (value>63 = pressed), Stop=CC42
    static MidiMap nanoKontrol2() {
        MidiMap m;
        for (int i = 0; i < 7; ++i)
            m.ccBindings[i] = {i, -1};
        m.ccFor(MidiParam::SatDrive)     = {16, -1};
        m.ccFor(MidiParam::MixbusThresh) = {17, -1};
        m.ccFor(MidiParam::MixbusMakeup) = {18, -1};
        m.ccFor(MidiParam::LimCeiling)   = {19, -1};
        m.noteBindings[0] = {41, -1};  // Play (CC# stored in .note)
        m.noteBindings[1] = {42, -1};  // Stop
        return m;
    }

    // Behringer X-Touch Mini — GM Mode, Layer A
    // Encoders 1-7: CC 1-7 → EQ bands 0-6
    // Encoder 8: CC 8 → SatDrive
    // Encoders 9-11: CC 9-11 → MixbusThresh / MixbusMakeup / LimCeiling
    // Upper buttons: Note 89=Play, Note 90=Stop
    static MidiMap xTouchMini() {
        MidiMap m;
        for (int i = 0; i < 7; ++i)
            m.ccBindings[i] = {i + 1, -1};
        m.ccFor(MidiParam::SatDrive)     = {8,  -1};
        m.ccFor(MidiParam::MixbusThresh) = {9,  -1};
        m.ccFor(MidiParam::MixbusMakeup) = {10, -1};
        m.ccFor(MidiParam::LimCeiling)   = {11, -1};
        m.noteBindings[0] = {89, -1};  // Play
        m.noteBindings[1] = {90, -1};  // Stop
        return m;
    }

    static MidiMap defaultMap() { return nanoKontrol2(); }
};

// MIDI CC (0-127) → parameter value in [min, max]
inline double ccToValue(int cc, double min, double max) {
    return min + (std::clamp(cc, 0, 127) / 127.0) * (max - min);
}

// Parameter value → MIDI CC (0-127)
inline int valueToCC(double value, double min, double max) {
    if (max <= min) return 0;
    const double t = (value - min) / (max - min);
    return static_cast<int>(std::round(std::clamp(t, 0.0, 1.0) * 127.0));
}

} // namespace gui
