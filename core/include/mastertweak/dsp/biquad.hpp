#pragma once

#include <cmath>
#include <numbers>

namespace mt::dsp {

// Direct Form 2 Transposed biquad. Normalised so a0 = 1.
struct BiquadCoeffs {
    float b0 = 1.f, b1 = 0.f, b2 = 0.f;
    float              a1 = 0.f, a2 = 0.f;

    // Audio EQ Cookbook (RBJ) formulas — all normalised to a0.
    static BiquadCoeffs lowpass(float fc, float q, float sr) noexcept;
    static BiquadCoeffs highpass(float fc, float q, float sr) noexcept;
    static BiquadCoeffs bell(float fc, float q, float gainDb, float sr) noexcept;
    static BiquadCoeffs lowShelf(float fc, float gainDb, float sr) noexcept;
    static BiquadCoeffs highShelf(float fc, float gainDb, float sr) noexcept;
};

// Per-channel state for one biquad stage.
struct BiquadState {
    float s0 = 0.f, s1 = 0.f;
    void reset() noexcept { s0 = s1 = 0.f; }
};

[[nodiscard]] inline float biquadProcess(const BiquadCoeffs& c,
                                          BiquadState& st,
                                          float x) noexcept {
    const float y = c.b0 * x + st.s0;
    st.s0         = c.b1 * x - c.a1 * y + st.s1;
    st.s1         = c.b2 * x - c.a2 * y;
    return y;
}

// ── Coefficient factories ────────────────────────────────────────────────────

inline BiquadCoeffs BiquadCoeffs::lowpass(float fc, float q, float sr) noexcept {
    const float w0    = 2.f * std::numbers::pi_v<float> * fc / sr;
    const float cosw0 = std::cos(w0);
    const float alpha = std::sin(w0) / (2.f * q);
    const float a0    = 1.f + alpha;
    const float b0    = (1.f - cosw0) * 0.5f / a0;
    return { b0, 2.f * b0, b0,
             -2.f * cosw0 / a0, (1.f - alpha) / a0 };
}

inline BiquadCoeffs BiquadCoeffs::highpass(float fc, float q, float sr) noexcept {
    const float w0    = 2.f * std::numbers::pi_v<float> * fc / sr;
    const float cosw0 = std::cos(w0);
    const float alpha = std::sin(w0) / (2.f * q);
    const float a0    = 1.f + alpha;
    const float b0    = (1.f + cosw0) * 0.5f / a0;
    return { b0, -2.f * b0, b0,
             -2.f * cosw0 / a0, (1.f - alpha) / a0 };
}

inline BiquadCoeffs BiquadCoeffs::bell(float fc, float q, float gainDb, float sr) noexcept {
    const float A     = std::pow(10.f, gainDb / 40.f);
    const float w0    = 2.f * std::numbers::pi_v<float> * fc / sr;
    const float cosw0 = std::cos(w0);
    const float alpha = std::sin(w0) / (2.f * q);
    const float a0    = 1.f + alpha / A;
    return { (1.f + alpha * A) / a0,
             -2.f * cosw0      / a0,
             (1.f - alpha * A) / a0,
             -2.f * cosw0      / a0,
             (1.f - alpha / A) / a0 };
}

inline BiquadCoeffs BiquadCoeffs::lowShelf(float fc, float gainDb, float sr) noexcept {
    const float A     = std::pow(10.f, gainDb / 40.f);
    const float w0    = 2.f * std::numbers::pi_v<float> * fc / sr;
    const float cosw0 = std::cos(w0);
    const float alpha = std::sin(w0) / 2.f * std::sqrt((A + 1.f / A) * (1.f / 0.9f - 1.f) + 2.f);
    const float a0 = (A + 1.f) + (A - 1.f) * cosw0 + 2.f * std::sqrt(A) * alpha;
    return { A * ((A + 1.f) - (A - 1.f) * cosw0 + 2.f * std::sqrt(A) * alpha) / a0,
             2.f * A * ((A - 1.f) - (A + 1.f) * cosw0) / a0,
             A * ((A + 1.f) - (A - 1.f) * cosw0 - 2.f * std::sqrt(A) * alpha) / a0,
             -2.f * ((A - 1.f) + (A + 1.f) * cosw0) / a0,
             ((A + 1.f) + (A - 1.f) * cosw0 - 2.f * std::sqrt(A) * alpha) / a0 };
}

inline BiquadCoeffs BiquadCoeffs::highShelf(float fc, float gainDb, float sr) noexcept {
    const float A     = std::pow(10.f, gainDb / 40.f);
    const float w0    = 2.f * std::numbers::pi_v<float> * fc / sr;
    const float cosw0 = std::cos(w0);
    const float alpha = std::sin(w0) / 2.f * std::sqrt((A + 1.f / A) * (1.f / 0.9f - 1.f) + 2.f);
    const float a0 = (A + 1.f) - (A - 1.f) * cosw0 + 2.f * std::sqrt(A) * alpha;
    return { A * ((A + 1.f) + (A - 1.f) * cosw0 + 2.f * std::sqrt(A) * alpha) / a0,
             -2.f * A * ((A - 1.f) + (A + 1.f) * cosw0) / a0,
             A * ((A + 1.f) + (A - 1.f) * cosw0 - 2.f * std::sqrt(A) * alpha) / a0,
             2.f * ((A - 1.f) - (A + 1.f) * cosw0) / a0,
             ((A + 1.f) - (A - 1.f) * cosw0 - 2.f * std::sqrt(A) * alpha) / a0 };
}

} // namespace mt::dsp
