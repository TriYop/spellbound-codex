#include "mastertweak/io.hpp"

#include <sndfile.h>

#include <algorithm>
#include <vector>
#include <cstring>

namespace mt {

std::optional<AudioFile> readAudioFile(const std::string& path, std::string* errOut) {
    SF_INFO info{};
    SNDFILE* sf = sf_open(path.c_str(), SFM_READ, &info);
    if (!sf) {
        if (errOut) *errOut = sf_strerror(nullptr);
        return std::nullopt;
    }

    AudioFile out;
    out.numChannels = info.channels;
    out.numFrames   = static_cast<int>(info.frames);
    out.sampleRate  = info.samplerate;
    // Decode source bit-depth from format flags
    const int subtype = info.format & SF_FORMAT_SUBMASK;
    if      (subtype == SF_FORMAT_PCM_16) out.bitDepth = 16;
    else if (subtype == SF_FORMAT_PCM_24) out.bitDepth = 24;
    else if (subtype == SF_FORMAT_PCM_32 || subtype == SF_FORMAT_FLOAT) out.bitDepth = 32;
    else                                  out.bitDepth = 24;  // sensible default

    // Read interleaved, then deinterleave
    const int totalSamples = out.numFrames * out.numChannels;
    std::vector<float> interleaved(static_cast<size_t>(totalSamples));
    const sf_count_t read = sf_read_float(sf, interleaved.data(), totalSamples);
    sf_close(sf);

    if (read != totalSamples) {
        if (errOut) *errOut = "Short read: expected " + std::to_string(totalSamples) +
                              " samples, got " + std::to_string(read);
        return std::nullopt;
    }

    out.samples.resize(static_cast<size_t>(out.numChannels),
                       std::vector<float>(static_cast<size_t>(out.numFrames)));
    for (int f = 0; f < out.numFrames; ++f)
        for (int c = 0; c < out.numChannels; ++c)
            out.samples[static_cast<size_t>(c)][static_cast<size_t>(f)] =
                interleaved[static_cast<size_t>(f * out.numChannels + c)];

    return out;
}

bool writeAudioFile(const std::string& path, const AudioFile& audio,
                    WriteOptions opts, std::string* errOut) {
    if (audio.numChannels == 0 || audio.numFrames == 0) {
        if (errOut) *errOut = "Empty audio";
        return false;
    }

    int subtype;
    if (opts.flac) {
        subtype = (opts.bitDepth == 16) ? SF_FORMAT_PCM_16 : SF_FORMAT_PCM_24;
    } else {
        switch (opts.bitDepth) {
            case 16:  subtype = SF_FORMAT_PCM_16; break;
            case 32:  subtype = SF_FORMAT_FLOAT;  break;
            default:  subtype = SF_FORMAT_PCM_24; break;
        }
    }
    const int format = (opts.flac ? SF_FORMAT_FLAC : SF_FORMAT_WAV) | subtype;

    SF_INFO info{};
    info.samplerate = audio.sampleRate;
    info.channels   = audio.numChannels;
    info.frames     = audio.numFrames;
    info.format     = format;

    SNDFILE* sf = sf_open(path.c_str(), SFM_WRITE, &info);
    if (!sf) {
        if (errOut) *errOut = sf_strerror(nullptr);
        return false;
    }

    // Interleave before writing
    const size_t total = static_cast<size_t>(audio.numFrames * audio.numChannels);
    std::vector<float> interleaved(total);
    for (int f = 0; f < audio.numFrames; ++f)
        for (int c = 0; c < audio.numChannels; ++c)
            interleaved[static_cast<size_t>(f * audio.numChannels + c)] =
                audio.samples[static_cast<size_t>(c)][static_cast<size_t>(f)];

    // Clipping before integer conversion: libsndfile clips internally for PCM formats,
    // but explicit clip prevents saturation artifacts.
    if (opts.bitDepth <= 24)
        for (auto& s : interleaved)
            s = std::clamp(s, -1.f, 1.f);

    const sf_count_t written = sf_write_float(sf, interleaved.data(),
                                               static_cast<sf_count_t>(total));
    sf_close(sf);

    if (written != static_cast<sf_count_t>(total)) {
        if (errOut) *errOut = "Short write";
        return false;
    }
    return true;
}

} // namespace mt
