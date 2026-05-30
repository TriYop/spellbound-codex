#include "preset_builder/services/ingest_service.hpp"

#include "mastertweak/analysis.hpp"
#include "mastertweak/codec_correction.hpp"
#include "mastertweak/io.hpp"

#include "picosha2.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace pb {

// ─── helpers ──────────────────────────────────────────────────────────────────

static std::string sha256File(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("sha256File: cannot open '" + path + "'");
    std::vector<unsigned char> hash(picosha2::k_digest_size);
    picosha2::hash256(ifs, hash.begin(), hash.end());
    return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}

static std::string utcNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    gmtime_r(&t, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static TrackAnalysis toTrackAnalysis(const mt::AnalysisSnapshot& snap) {
    TrackAnalysis a;
    for (int i = 0; i < 7; ++i) {
        const auto si = static_cast<size_t>(i);
        a.bandRmsDb[si]       = snap.bands[si].avgRmsDb;
        a.bandCorr[si]        = snap.bands[si].correlation;
        a.bandTransientDb[si] = snap.bands[si].crestDb;
    }
    a.overallRmsDb = snap.overallAvgDb;
    a.overallCorr  = snap.overallCorr;
    return a;
}

static bool isAudioExtension(const fs::path& p) {
    std::string ext = p.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".wav"  || ext == ".flac" || ext == ".aiff" || ext == ".aif"
        || ext == ".mp3"  || ext == ".ogg"  || ext == ".oga";
}

static std::vector<fs::path> collectAudioFiles(const std::string& root) {
    std::vector<fs::path> files;
    if (fs::is_regular_file(root)) {
        if (isAudioExtension(root)) files.push_back(root);
        return files;
    }
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && isAudioExtension(entry.path()))
            files.push_back(entry.path());
    }
    return files;
}

// ─── public API ───────────────────────────────────────────────────────────────

TrackMetadata IngestService::parseFilenameMetadata(const std::string& filePath) {
    TrackMetadata m;
    m.source = MetadataSource::filename;

    fs::path p(filePath);
    std::string stem = p.stem().string();

    const std::string sepAscii  = " - ";
    const std::string sepEmDash = " \xe2\x80\x93 ";

    auto trySplit = [&](const std::string& sep) -> bool {
        const auto pos = stem.find(sep);
        if (pos == std::string::npos) return false;
        m.artist = stem.substr(0, pos);
        m.title  = stem.substr(pos + sep.size());
        return true;
    };

    if (!trySplit(sepAscii) && !trySplit(sepEmDash))
        m.title = stem;

    return m;
}

IngestReport IngestService::ingest(const std::string& path,
                                   TrackRepository&   repo,
                                   MetadataProvider&  metaProvider,
                                   mt::ProgressCallback progress,
                                   std::atomic<bool>* cancel,
                                   ErrorCallback      onError) const {
    IngestReport report;
    const auto files = collectAudioFiles(path);
    if (files.empty()) {
        if (progress) progress(1.f, "Done");
        return report;
    }
    const float total = static_cast<float>(files.size());

    const unsigned      nThreads = std::max(1u, std::thread::hardware_concurrency());
    std::atomic<size_t> nextIndex{0};
    std::atomic<size_t> doneCount{0};
    std::mutex          mutex;

    // Increment done counter and fire progress outside any lock.
    auto reportProgress = [&](const std::string& msg) {
        const size_t n = doneCount.fetch_add(1, std::memory_order_relaxed) + 1;
        if (progress) progress(static_cast<float>(n) / total, msg);
    };

    // Per-file pipeline — uses return for early exits so the worker loop stays clean.
    // Heavy CPU work (hash / decode / analyse) runs lock-free.
    // DB and MetadataProvider calls are serialised behind mutex (single SQLite connection;
    // MetadataProvider may use Qt network internals that are not thread-safe).
    auto processFile = [&](size_t i) {
        const std::string filePath = files[i].string();
        const std::string filename = files[i].filename().string();

        // 1. Hash — pure I/O, no shared state
        std::string hash;
        try { hash = sha256File(filePath); }
        catch (...) {
            const std::string msg = "Failed to hash file";
            { std::lock_guard lk(mutex); ++report.failed; report.errors.emplace_back(filePath, msg); }
            if (onError) onError(filePath, msg);
            reportProgress("Failed: " + filename);
            return;
        }

        // 2. DB skip-check — serialised
        bool inDb = false;
        { std::lock_guard lk(mutex); inDb = static_cast<bool>(repo.find(TrackId{hash})); if (inDb) ++report.skipped; }
        if (inDb) { reportProgress("Skipped: " + filename); return; }

        // 3. Decode + analyse — lock-free, per-file AudioFile owns its data
        std::string err;
        const auto audio = mt::readAudioFile(filePath, &err);
        if (!audio) {
            const std::string msg = "Read failed: " + err;
            { std::lock_guard lk(mutex); ++report.failed; report.errors.emplace_back(filePath, msg); }
            if (onError) onError(filePath, msg);
            reportProgress("Failed: " + filename);
            return;
        }
        const auto snap = mt::analyseFile(*audio);
        auto analysis   = toTrackAnalysis(snap);
        if (audio->sourceFormat == mt::SourceFormat::mp3 ||
            audio->sourceFormat == mt::SourceFormat::ogg) {
            const auto corr = mt::computeCodecCorrection(*audio);
            for (size_t j = 0; j < 7; ++j)
                analysis.bandRmsDb[j] += corr[j];
        }

        // 4. Metadata — serialised
        std::optional<TrackMetadata> meta;
        { std::lock_guard lk(mutex); meta = metaProvider.lookup(filePath); }
        if (!meta) meta = parseFilenameMetadata(filePath);

        // 5. Persist — serialised
        Track track;
        track.id       = TrackId{hash};
        track.path     = filePath;
        track.metadata = *meta;
        track.analysis = analysis;
        track.addedAt  = utcNow();
        { std::lock_guard lk(mutex); repo.save(track); ++report.added; }
        reportProgress("Ingested: " + filename);
    };

    // Worker: pull file indices atomically, wrap processFile for exception safety
    // (unexpected throws from repo/provider must not escape std::thread).
    auto workerFn = [&]() noexcept {
        while (true) {
            if (cancel && cancel->load(std::memory_order_relaxed)) break;
            const size_t i = nextIndex.fetch_add(1, std::memory_order_relaxed);
            if (i >= files.size()) break;
            try {
                processFile(i);
            } catch (const std::exception& ex) {
                std::lock_guard lk(mutex);
                ++report.failed;
                report.errors.emplace_back(files[i].string(), ex.what());
            } catch (...) {
                std::lock_guard lk(mutex);
                ++report.failed;
                report.errors.emplace_back(files[i].string(), "unknown error");
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(nThreads);
    for (unsigned t = 0; t < nThreads; ++t)
        threads.emplace_back(workerFn);
    for (auto& t : threads) t.join();

    if (cancel && cancel->load(std::memory_order_relaxed))
        report.cancelled = true;

    if (progress) progress(1.f, report.cancelled ? "Cancelled" : "Done");
    return report;
}

} // namespace pb
