#include "preset_builder/services/similarity_service.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <map>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace pb {

// ---------------------------------------------------------------------------
// distance()
// ---------------------------------------------------------------------------

float SimilarityService::distance(const TrackAnalysis& a, const TrackAnalysis& b) {
    float sum = 0.f;

    constexpr float normRms       = 40.f;
    constexpr float normCorr      = 2.f;
    constexpr float normTransient = 20.f;

    for (size_t i = 0; i < 7; ++i) {
        float d;
        d = (a.bandRmsDb[i] - b.bandRmsDb[i]) / normRms;
        sum += d * d;
        d = (a.bandCorr[i] - b.bandCorr[i]) / normCorr;
        sum += d * d;
        d = (a.bandTransientDb[i] - b.bandTransientDb[i]) / normTransient;
        sum += d * d;
    }

    {
        float d = (a.overallRmsDb - b.overallRmsDb) / normRms;
        sum += d * d;
    }
    {
        float d = (a.overallCorr - b.overallCorr) / normCorr;
        sum += d * d;
    }

    return std::sqrt(sum);
}

// ---------------------------------------------------------------------------
// discover()  — threshold-based connected-component clustering
//
// For each pair (i,j) whose distance < threshold, add an edge. Union-find
// extracts connected components in O(N² / nThreads + K·α(N)) where K is the
// number of edges. Scales to tens of thousands of tracks; avoids the O(N³)
// agglomerative merge loop entirely.
// ---------------------------------------------------------------------------

std::vector<SimilarityGroup> SimilarityService::discover(
    const std::vector<Track>& tracks,
    float threshold,
    std::function<void(int)> progress) const
{
    const size_t N = tracks.size();
    if (N <= 1) return {};

    // Phase 1 (0–90 %): parallel edge collection.
    // Each thread owns a private edge list → no shared writes, no locks.
    const size_t nThreads = std::max(size_t(1),
        size_t(std::thread::hardware_concurrency()));
    std::vector<std::vector<std::pair<size_t,size_t>>> threadEdges(nThreads);
    std::atomic<size_t> rowsDone{0};
    {
        std::vector<std::thread> threads(nThreads);
        for (size_t t = 0; t < nThreads; ++t) {
            threads[t] = std::thread([&, t]() {
                auto& edges = threadEdges[t];
                for (size_t i = t; i < N; i += nThreads) {
                    for (size_t j = i + 1; j < N; ++j) {
                        if (distance(tracks[i].analysis, tracks[j].analysis) < threshold)
                            edges.emplace_back(i, j);
                    }
                    const size_t done = ++rowsDone;
                    if (progress) progress(static_cast<int>(done * 90 / N));
                }
            });
        }
        for (auto& th : threads) th.join();
    }

    // Phase 2 (90–100 %): union-find on collected edges.
    std::vector<size_t> parent(N);
    std::iota(parent.begin(), parent.end(), 0);

    // Path-halving find.
    auto find = [&](size_t x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };

    for (size_t t = 0; t < nThreads; ++t)
        for (auto [i, j] : threadEdges[t]) {
            size_t pi = find(i), pj = find(j);
            if (pi != pj) parent[pi] = pj;
        }
    if (progress) progress(100);

    // Group tracks by root.
    std::map<size_t, std::vector<size_t>> components;
    for (size_t i = 0; i < N; ++i)
        components[find(i)].push_back(i);

    // Build result: only components with >= 2 tracks.
    std::vector<SimilarityGroup> result;
    for (const auto& [root, members] : components) {
        if (members.size() < 2) continue;
        SimilarityGroup grp;
        grp.tracks.reserve(members.size());
        for (size_t idx : members)
            grp.tracks.push_back(tracks[idx]);
        result.push_back(std::move(grp));
    }

    // Sort largest-first.
    std::sort(result.begin(), result.end(),
              [](const SimilarityGroup& a, const SimilarityGroup& b) {
                  return a.tracks.size() > b.tracks.size();
              });

    // Assign suggested names after sorting so fallback "Group N" numbers match order.
    for (size_t k = 0; k < result.size(); ++k) {
        result[k].suggestedName = suggestName(result[k].tracks, static_cast<int>(k + 1));
    }

    return result;
}

// ---------------------------------------------------------------------------
// suggestName()
// ---------------------------------------------------------------------------

static std::string mostFrequentIfDominant(const std::vector<std::string>& items,
                                           bool checkDistinct = false,
                                           size_t distinctThreshold = 5)
{
    if (items.empty()) return {};

    // Count frequencies.
    std::map<std::string, size_t> freq;
    for (const auto& s : items)
        ++freq[s];

    if (checkDistinct && freq.size() >= distinctThreshold)
        return {};

    // Find most frequent.
    auto it = std::max_element(freq.begin(), freq.end(),
                               [](const auto& a, const auto& b) {
                                   return a.second < b.second;
                               });

    // Must appear in > 50% of tracks that have metadata.
    if (it->second * 2 > items.size())
        return it->first;

    return {};
}

std::string SimilarityService::suggestName(const std::vector<Track>& tracks, int fallbackN) {
    // Collect genres, artists, years from tracks that have them.
    std::vector<std::string> genres, artists;
    std::vector<int>         years;

    for (const auto& t : tracks) {
        if (t.metadata.genre.has_value())
            genres.push_back(*t.metadata.genre);
        if (t.metadata.artist.has_value())
            artists.push_back(*t.metadata.artist);
        if (t.metadata.year.has_value())
            years.push_back(*t.metadata.year);
    }

    // Dominant genre: > 50% of tracks that have genre metadata.
    std::string dominant_genre = mostFrequentIfDominant(genres);

    // Dominant artist: > 50% of tracks with artist metadata AND < 5 distinct artists.
    std::string dominant_artist = mostFrequentIfDominant(
        artists, /*checkDistinct=*/true, /*distinctThreshold=*/5);

    // Decade: only if year range <= 15.
    std::string decade;
    if (!years.empty()) {
        int mn = *std::min_element(years.begin(), years.end());
        int mx = *std::max_element(years.begin(), years.end());
        if (mx - mn <= 15) {
            // Median year: sort, lower-middle for even count.
            std::vector<int> sorted_years = years;
            std::sort(sorted_years.begin(), sorted_years.end());
            int median_year = sorted_years[(sorted_years.size() - 1) / 2];
            int dec = (median_year / 10) * 10;
            decade = std::to_string(dec) + "s";
        }
    }

    // Concatenate non-empty parts: artist genre decade.
    std::string name;
    for (const std::string& part : {dominant_artist, dominant_genre, decade}) {
        if (part.empty()) continue;
        if (!name.empty()) name += ' ';
        name += part;
    }

    // Trim (leading/trailing spaces).
    const auto first = name.find_first_not_of(' ');
    if (first == std::string::npos) return "Group " + std::to_string(fallbackN);
    const auto last = name.find_last_not_of(' ');
    name = name.substr(first, last - first + 1);

    if (name.empty()) return "Group " + std::to_string(fallbackN);
    return name;
}

} // namespace pb
