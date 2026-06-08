#include "preset_builder/adapters/embedded_tag_metadata_provider.hpp"

#include <taglib/fileref.h>
#include <taglib/tag.h>

namespace pb {

std::optional<TrackMetadata> EmbeddedTagMetadataProvider::lookup(const std::string& path) {
    TagLib::FileRef f(path.c_str());
    if (f.isNull() || !f.tag())
        return std::nullopt;

    auto* tag = f.tag();
    TrackMetadata m;
    m.source = MetadataSource::embedded_tags;

    if (!tag->title().isEmpty())  m.title  = tag->title().to8Bit(true);
    if (!tag->artist().isEmpty()) m.artist = tag->artist().to8Bit(true);
    if (!tag->album().isEmpty())  m.album  = tag->album().to8Bit(true);
    if (!tag->genre().isEmpty())  m.genre  = tag->genre().to8Bit(true);
    if (tag->year() != 0)         m.year   = static_cast<int>(tag->year());

    return m;
}

} // namespace pb
