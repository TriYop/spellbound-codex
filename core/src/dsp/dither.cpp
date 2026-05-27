#include "mastertweak/dsp/dither.hpp"

#include <cmath>

namespace mt::dsp {

void Dither::prepare(int outputBitDepth, int /*numChannels*/) {
    active_    = (outputBitDepth <= 24);
    // 1 LSB in the output format = 2 / 2^outputBitDepth
    amplitude_ = active_ ? 2.f / std::pow(2.f, static_cast<float>(outputBitDepth)) : 0.f;
    reset();
}

void Dither::reset() {
    rng_ = 0x12345678u;
}

float Dither::nextRandom() noexcept {
    // Xorshift32
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    // Map [0, 0xFFFFFFFF] → [-1, 1]
    return static_cast<float>(rng_) / 2147483648.f - 1.f;
}

void Dither::process(std::vector<std::vector<float>>& samples, int numFrames) {
    if (!active_) return;
    const float amp = amplitude_;
    for (auto& ch : samples)
        for (int f = 0; f < numFrames; ++f) {
            // TPDF = two uncorrelated rectangular PDFs summed → triangular
            const float d = (nextRandom() + nextRandom()) * 0.5f * amp;
            ch[static_cast<size_t>(f)] += d;
        }
}

} // namespace mt::dsp
