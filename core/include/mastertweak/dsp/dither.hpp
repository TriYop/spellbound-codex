#pragma once

#include <cstdint>
#include <vector>

namespace mt::dsp {

// TPDF (Triangular Probability Density Function) dither.
// Applied only when outputting to integer formats (16- or 24-bit).
// A 24-bit output has noise floor ~ -144 dBFS; TPDF dither is barely audible.
// For 32-bit float or 32-bit int output, dither is a no-op.
class Dither {
public:
    void prepare(int outputBitDepth, int numChannels);
    void reset();

    // Add TPDF dither in-place. No-op if bitDepth > 24.
    void process(std::vector<std::vector<float>>& samples, int numFrames);

private:
    bool  active_    = false;
    float amplitude_ = 0.f;   // dither amplitude in linear, = 1 LSB
    uint32_t rng_    = 0x12345678u;

    float nextRandom() noexcept;  // returns uniform float in [-1, 1]
};

} // namespace mt::dsp
