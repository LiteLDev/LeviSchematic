#pragma once
#include <string>
#include <string_view>

namespace levischematic::ui {
inline bool containsHan(std::string_view text) {
    for (size_t i = 0; i + 2 < text.size(); ++i) {
        auto a = static_cast<unsigned char>(text[i]);
        if ((a & 0xf0) != 0xe0) continue;
        auto b = static_cast<unsigned char>(text[i + 1]), c = static_cast<unsigned char>(text[i + 2]);
        if ((b & 0xc0) != 0x80 || (c & 0xc0) != 0x80) continue;
        unsigned code = ((a & 0x0f) << 12) | ((b & 0x3f) << 6) | (c & 0x3f);
        if ((code >= 0x3400 && code <= 0x9fff) || (code >= 0xf900 && code <= 0xfaff)) return true;
    }
    return false;
}
inline std::string selectUiLanguage(std::string const& text, bool chinese) {
    std::string result;
    size_t      start = 0;
    while (start <= text.size()) {
        auto end   = text.find('\n', start);
        auto line  = text.substr(start, end == text.npos ? text.npos : end - start);
        auto slash = line.find(" / ");
        if (slash != line.npos) {
            auto left = line.substr(0, slash), right = line.substr(slash + 3);
            if (containsHan(left) != containsHan(right)) line = chinese == containsHan(left) ? left : right;
        }
        if (start) result += '\n';
        result += line;
        if (end == text.npos) break;
        start = end + 1;
    }
    return result;
}
} 
