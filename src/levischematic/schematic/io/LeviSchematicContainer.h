#pragma once
#include <cstdint>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace levischematic::schematic::container {
inline constexpr std::string_view Magic           = std::string_view("LVSCHEM\0", 8);
inline constexpr uint16_t         Version         = 1;
inline constexpr uint32_t         MaxHeaderLength = 16u << 20;
inline constexpr size_t           PrefixLength    = 16;

struct Entry {
    std::string data;
    uint64_t    rawLength   = 0;
    uint32_t    crc32       = 0;
    std::string compression = "gzip";
};

[[nodiscard]] uint32_t crc32(std::string_view bytes);

[[nodiscard]] std::string encode(nlohmann::json header, std::span<Entry const> entries);

[[nodiscard]] nlohmann::json   decodeHeader(std::string_view file);
[[nodiscard]] nlohmann::json   readHeader(std::filesystem::path const& path);
[[nodiscard]] std::string_view entryBytes(std::string_view file, nlohmann::json const& header, size_t index);
[[nodiscard]] std::string      replaceHeader(std::string_view file, nlohmann::json header);
} // namespace levischematic::schematic::container
