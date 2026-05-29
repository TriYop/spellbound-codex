#pragma once

#include <cctype>
#include <string>

namespace gui {

// Convert an arbitrary string into a filename-safe lowercase stem (max 24 chars).
inline std::string sanitizePresetName(const std::string& name) {
    std::string s;
    s.reserve(name.size());
    for (char ch : name) {
        const auto c = static_cast<unsigned char>(ch);
        s += std::isalnum(c) ? static_cast<char>(std::tolower(c)) : '_';
    }
    std::string out;
    bool prevUnder = false;
    for (char c : s) {
        if (c == '_') { if (!prevUnder) out += c; prevUnder = true; }
        else          { out += c; prevUnder = false; }
    }
    auto start = out.find_first_not_of('_');
    if (start == std::string::npos) return "preset";
    out = out.substr(start);
    auto end = out.find_last_not_of('_');
    if (end != std::string::npos) out = out.substr(0, end + 1);
    if (out.size() > 24) out.resize(24);
    auto end2 = out.find_last_not_of('_');
    if (end2 != std::string::npos) out = out.substr(0, end2 + 1);
    return out.empty() ? "preset" : out;
}

} // namespace gui
