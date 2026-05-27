#include "mastertweak/dsp/limiter.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <stdexcept>
#include <vector>

namespace mt::dsp {

float Limiter::dbToLinear(float db) noexcept  { return std::pow(10.f, db / 20.f); }
float Limiter::linearToDb(float lin) noexcept { return lin > 1e-7f ? 20.f * std::log10(lin) : -100.f; }

void Limiter::prepare(float sampleRate, int numChannels, int /*maxBlockSize*/) {
    sampleRate_  = sampleRate;
    numChannels_ = numChannels;
    // 5 ms lookahead window (used as the sliding-window width)
    delayLen_    = static_cast<int>(0.005f * sampleRate + 0.5f);
    // 50 ms release
    releaseCoef_ = std::exp(-1.f / (0.05f * sampleRate));
    gainEnv_     = 1.f;
}

void Limiter::setAdvice(const mt::LimiterParams& p) {
    ceiling_ = dbToLinear(p.ceilingDb);
}

void Limiter::reset() {
    gainEnv_ = 1.f;
}

void Limiter::process(std::vector<std::vector<float>>& samples, int numFrames) {
    if (static_cast<int>(samples.size()) < numChannels_)
        throw std::logic_error("Limiter: channel count mismatch");

    const auto N = static_cast<size_t>(numFrames);

    // ── Step 1: per-frame required gain ──────────────────────────────────────
    // requiredGain[f] = ceiling / peak[f], clamped to [0, 1]
    std::vector<float> required(N, 1.f);
    for (int f = 0; f < numFrames; ++f) {
        float peak = 0.f;
        for (int ch = 0; ch < numChannels_; ++ch)
            peak = std::max(peak, std::abs(samples[static_cast<size_t>(ch)][static_cast<size_t>(f)]));
        if (peak > ceiling_)
            required[static_cast<size_t>(f)] = ceiling_ / peak;
    }

    // ── Step 2: lookahead attack — sliding window minimum ─────────────────────
    // attack[f] = min over required[f .. f+delayLen_-1]
    // Guarantees gain is already at the needed level when the peak arrives.
    // Algorithm: reverse → standard forward sliding min → reverse (O(N)).
    std::vector<float> attack(N, 1.f);
    if (delayLen_ > 0 && numFrames > 0) {
        // Reverse required into rev[]
        std::vector<float> rev(N);
        for (size_t i = 0; i < N; ++i)
            rev[i] = required[N - 1 - i];

        // Standard forward sliding minimum on rev, window = delayLen_
        std::vector<float> revMin(N);
        std::deque<int> dq;
        for (int i = 0; i < numFrames; ++i) {
            // Evict front elements that have left the window
            while (!dq.empty() && dq.front() < i - delayLen_ + 1)
                dq.pop_front();
            // Maintain monotone: evict back elements with value >= rev[i]
            while (!dq.empty() && rev[static_cast<size_t>(dq.back())] >= rev[static_cast<size_t>(i)])
                dq.pop_back();
            dq.push_back(i);
            revMin[static_cast<size_t>(i)] = rev[static_cast<size_t>(dq.front())];
        }

        // Reverse revMin back to get attack[] in forward time
        for (size_t i = 0; i < N; ++i)
            attack[i] = revMin[N - 1 - i];
    } else {
        attack = required;
    }

    // ── Step 3: forward release smoothing + apply ─────────────────────────────
    for (int f = 0; f < numFrames; ++f) {
        const size_t fs = static_cast<size_t>(f);
        if (attack[fs] < gainEnv_)
            gainEnv_ = attack[fs];  // instantaneous attack
        else
            gainEnv_ = releaseCoef_ * gainEnv_ + (1.f - releaseCoef_) * attack[fs];

        for (int ch = 0; ch < numChannels_; ++ch)
            samples[static_cast<size_t>(ch)][fs] *= gainEnv_;
    }
}

} // namespace mt::dsp
