#pragma once

#include <vector>

namespace mt::dsp {

struct StageLevelReport {
    float inputPeakDb  = -100.f;
    float inputRmsDb   = -100.f;
    float trimDb       = 0.f;        // gain applied (negative = trim, positive = restore)
    float outputPeakDb = -100.f;
    float outputRmsDb  = -100.f;
};

class GainStager {
public:
    // Bring broadband RMS to targetRmsDb. Clamped to ±kMaxRestoreDb.
    StageLevelReport restoreRms(std::vector<std::vector<float>>& buf,
                                int numFrames, float targetRmsDb);

    // Trim only if peak > targetPeakDb. Never boosts.
    StageLevelReport trimPeak(std::vector<std::vector<float>>& buf,
                              int numFrames, float targetPeakDb = -3.f);

    // Public so pipeline.cpp can snapshot the reference RMS without an instance.
    static float measureRmsDb(const std::vector<std::vector<float>>& buf, int numFrames);
    static float measurePeakDb(const std::vector<std::vector<float>>& buf, int numFrames);

private:
    static constexpr float kSilenceDb    = -100.f;
    static constexpr float kMaxRestoreDb = 12.f;
    static void applyGain(std::vector<std::vector<float>>& buf, int numFrames, float gainLin);
};

} // namespace mt::dsp
