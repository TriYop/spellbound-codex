#include "mastertweak/dsp/gain_stager.hpp"

#include <algorithm>
#include <cmath>

namespace mt::dsp {

float GainStager::measurePeakDb(const std::vector<std::vector<float>>& buf, int numFrames) {
    float peak = 0.f;
    for (const auto& ch : buf)
        for (int f = 0; f < numFrames; ++f)
            peak = std::max(peak, std::abs(ch[static_cast<size_t>(f)]));
    return peak > 1e-7f ? 20.f * std::log10(peak) : -100.f;
}

float GainStager::measureRmsDb(const std::vector<std::vector<float>>& buf, int numFrames) {
    double sumSq = 0.0;
    long   count = 0;
    for (const auto& ch : buf)
        for (int f = 0; f < numFrames; ++f) {
            const double s = ch[static_cast<size_t>(f)];
            sumSq += s * s;
            ++count;
        }
    if (count == 0) return -100.f;
    const float rmsLin = static_cast<float>(std::sqrt(sumSq / static_cast<double>(count)));
    return rmsLin > 1e-7f ? 20.f * std::log10(rmsLin) : -100.f;
}

void GainStager::applyGain(std::vector<std::vector<float>>& buf, int numFrames, float gainLin) {
    for (auto& ch : buf)
        for (int f = 0; f < numFrames; ++f)
            ch[static_cast<size_t>(f)] *= gainLin;
}

StageLevelReport GainStager::restoreRms(std::vector<std::vector<float>>& buf,
                                        int numFrames, float targetRmsDb) {
    StageLevelReport r;
    r.inputPeakDb = measurePeakDb(buf, numFrames);
    r.inputRmsDb  = measureRmsDb(buf, numFrames);

    if (r.inputRmsDb <= -99.f) return r;  // silence guard

    r.trimDb = std::clamp(targetRmsDb - r.inputRmsDb, -kMaxRestoreDb, kMaxRestoreDb);
    applyGain(buf, numFrames, std::pow(10.f, r.trimDb / 20.f));

    r.outputPeakDb = measurePeakDb(buf, numFrames);
    r.outputRmsDb  = measureRmsDb(buf, numFrames);
    return r;
}

StageLevelReport GainStager::trimPeak(std::vector<std::vector<float>>& buf,
                                      int numFrames, float targetPeakDb) {
    StageLevelReport r;
    r.inputPeakDb = measurePeakDb(buf, numFrames);
    r.inputRmsDb  = measureRmsDb(buf, numFrames);

    if (r.inputPeakDb <= targetPeakDb) {
        r.outputPeakDb = r.inputPeakDb;
        r.outputRmsDb  = r.inputRmsDb;
        return r;  // trimDb stays 0
    }

    r.trimDb = targetPeakDb - r.inputPeakDb;  // always negative
    applyGain(buf, numFrames, std::pow(10.f, r.trimDb / 20.f));

    r.outputPeakDb = measurePeakDb(buf, numFrames);
    r.outputRmsDb  = measureRmsDb(buf, numFrames);
    return r;
}

} // namespace mt::dsp
