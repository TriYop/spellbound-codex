#include "mastertweak/target_level.hpp"

#include <algorithm>
#include <cctype>

namespace mt {

const std::array<TargetLevelProfile, 8> kTargetLevelProfiles = {{
    {"Spotify",              -14.f, -1.f  },
    {"YouTube",              -14.f, -1.f  },
    {"Amazon Music",         -14.f, -1.f  },
    {"Tidal",                -14.f, -1.f  },
    {"Apple Music",          -16.f, -1.f  },
    {"CD / Download",         -9.f, -0.1f },
    {"Broadcast EBU R128",   -23.f, -1.f  },
    {"Broadcast ATSC A/85",  -24.f, -2.f  },
}};

const TargetLevelProfile* findTargetLevel(std::string_view name) {
    // Convert query to lowercase for comparison
    std::string query{name};
    std::transform(query.begin(), query.end(), query.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    for (const auto& p : kTargetLevelProfiles) {
        std::string lower{p.name};
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (lower == query) return &p;
    }
    return nullptr;
}

} // namespace mt
