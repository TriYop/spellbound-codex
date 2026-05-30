#pragma once

#include "mastertweak/io.hpp"

#include <array>

namespace mt {

// Estimate per-band RMS correction (dB) for a lossy-encoded audio file.
// Uses FFT-based spectral rolloff detection. Corrections are always non-negative
// (only adds energy, never subtracts). Returns all-zeros for lossless or
// silent/bass-only files where no correction is warranted.
//
// Band indices: Sub(0) Lows(1) LowMids(2) Mids(3) HiMids(4) Highs(5) Air(6)
// Max corrections:  0      0      0        0       +0.5      +1.5     +6  dB
std::array<float, 7> computeCodecCorrection(const AudioFile& audio);

} // namespace mt
