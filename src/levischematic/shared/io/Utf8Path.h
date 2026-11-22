#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace levischematic::util {
inline std::filesystem::path pathFromUtf8(std::string_view value) {
    return std::filesystem::path(std::u8string_view{reinterpret_cast<char8_t const*>(value.data()), value.size()});
}
inline std::string utf8(std::filesystem::path const& path) {
    auto text = path.u8string();
    return std::string(reinterpret_cast<char const*>(text.data()), text.size());
}
} // namespace levischematic::util
