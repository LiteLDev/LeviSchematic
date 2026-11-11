#pragma once
#include <filesystem>
#include <string_view>
namespace levischematic::util {
inline std::filesystem::path pathFromUtf8(std::string_view value) {
    return std::filesystem::path(std::u8string_view{reinterpret_cast<char8_t const*>(value.data()), value.size()});
}
} 
