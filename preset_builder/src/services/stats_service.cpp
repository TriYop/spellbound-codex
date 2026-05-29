#include "preset_builder/services/stats_service.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace pb {

PresetStats StatsService::compute(const std::vector<Track>& tracks) const {
    if (tracks.empty()) return {};

    const auto n  = static_cast<double>(tracks.size());
    const auto ni = tracks.size();
    PresetStats s;

    for (int bi = 0; bi < 7; ++bi) {
        const auto b = static_cast<size_t>(bi);

        // Mean for bandRmsDb
        double sum = 0.0;
        for (const auto& t : tracks) sum += t.analysis.bandRmsDb[b];
        s.bandRmsDb[b] = static_cast<float>(sum / n);

        // p10 for bandCorrMin
        std::vector<float> corrs;
        corrs.reserve(ni);
        for (const auto& t : tracks) corrs.push_back(t.analysis.bandCorr[b]);
        std::sort(corrs.begin(), corrs.end());
        s.bandCorrMin[b] = corrs[static_cast<size_t>(std::floor(0.1 * n))];

        // Median for bandTransientDb
        std::vector<float> trans;
        trans.reserve(ni);
        for (const auto& t : tracks) trans.push_back(t.analysis.bandTransientDb[b]);
        std::sort(trans.begin(), trans.end());
        const size_t mid = ni / 2;
        s.bandTransientDb[b] = (ni % 2 == 1)
            ? trans[mid]
            : (trans[mid - 1] + trans[mid]) / 2.f;
    }

    // Mean for overallRmsDb
    double sumRms = 0.0;
    for (const auto& t : tracks) sumRms += t.analysis.overallRmsDb;
    s.overallRmsDb = static_cast<float>(sumRms / n);

    // p10 for overallCorrMin
    std::vector<float> corrs;
    corrs.reserve(ni);
    for (const auto& t : tracks) corrs.push_back(t.analysis.overallCorr);
    std::sort(corrs.begin(), corrs.end());
    s.overallCorrMin = corrs[static_cast<size_t>(std::floor(0.1 * n))];

    return s;
}

} // namespace pb
