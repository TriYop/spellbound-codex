#include "preset_builder/services/ingest_service.hpp"

#include "mastertweak/analysis.hpp"
#include "mastertweak/codec_correction.hpp"
#include "mastertweak/io.hpp"

#include "picosha2.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

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
                                   mt::ProgressCallback progress) const {
    IngestReport report;
    const auto files = collectAudioFiles(path);
    const auto total = static_cast<float>(files.size());

    for (size_t i = 0; i < files.size(); ++i) {
        const std::string filePath = files[i].string();

        if (progress)
            progress(static_cast<float>(i) / total,
                     "Ingesting: " + files[i].filename().string());

        // 1. Hash
        std::string hash;
        try {
            hash = sha256File(filePath);
        } catch (...) {
            ++report.failed;
            report.errors.emplace_back(filePath, "Failed to hash file");
            continue;
        }

        // 2. Skip if already in DB
        if (repo.find(TrackId{hash})) {
            ++report.skipped;
            continue;
        }

        // 3. Analyse
        std::string err;
        const auto audio = mt::readAudioFile(filePath, &err);
        if (!audio) {
            ++report.failed;
            report.errors.emplace_back(filePath, "Read failed: " + err);
            continue;
        }
        const auto snap = mt::analyseFile(*audio);
        auto analysis   = toTrackAnalysis(snap);

        // Boost per-band RMS to compensate for lossy codec rolloff
        if (audio->sourceFormat == mt::SourceFormat::mp3 ||
            audio->sourceFormat == mt::SourceFormat::ogg) {
            const auto corr = mt::computeCodecCorrection(*audio);
            for (size_t j = 0; j < 7; ++j)
                analysis.bandRmsDb[j] += corr[j];
        }

        // 4. Metadata (provider → filename fallback)
        auto meta = metaProvider.lookup(filePath);
        if (!meta) meta = parseFilenameMetadata(filePath);

        // 5. Persist
        Track track;
        track.id       = TrackId{hash};
        track.path     = filePath;
        track.metadata = *meta;
        track.analysis = analysis;
        track.addedAt  = utcNow();
        repo.save(track);

        ++report.added;
    }

    if (progress) progress(1.f, "Done");
    return report;
}

} // namespace pb
