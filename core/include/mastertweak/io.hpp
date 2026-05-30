#pragma once

#include <optional>
#include <string>
#include <vector>

namespace mt {

enum class SourceFormat { wav, flac, aiff, ogg, mp3, unknown };

struct AudioFile {
    std::vector<std::vector<float>> samples;  // samples[channel][frame]
    int          numChannels  = 0;
    int          numFrames    = 0;
    int          sampleRate   = 0;
    int          bitDepth     = 0;
    SourceFormat sourceFormat = SourceFormat::unknown;
};

// Read WAV / FLAC / AIFF / OGG / MP3 (anything libsndfile supports).
// Returns nullopt on failure; fills errOut if provided.
std::optional<AudioFile> readAudioFile(const std::string& path,
                                        std::string* errOut = nullptr);

struct WriteOptions {
    int  bitDepth = 24;    // 16, 24, or 32
    bool flac     = false; // false = WAV, true = FLAC
};

// Write deinterleaved float audio to WAV or FLAC.
// Returns false on failure; fills errOut if provided.
bool writeAudioFile(const std::string& path,
                    const AudioFile& audio,
                    WriteOptions opts = {},
                    std::string* errOut = nullptr);

} // namespace mt
