#include "levischematic/schematic/io/LeviSchematicContainer.h"
#include <array>
#include <fstream>
#include <stdexcept>

namespace levischematic::schematic::container {
namespace {
void putLe(std::string& out, uint64_t value, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) out.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
}
uint64_t getLe(std::string_view in, size_t offset, size_t bytes) {
    uint64_t value = 0;
    for (size_t i = 0; i < bytes; ++i) value |= uint64_t(static_cast<unsigned char>(in[offset + i])) << (8 * i);
    return value;
}
struct Prefix {
    uint32_t headerLength = 0;
};
Prefix parsePrefix(std::string_view bytes) {
    if (bytes.size() < PrefixLength || bytes.substr(0, Magic.size()) != Magic)
        throw std::runtime_error("Not a .levischematic file.");
    auto version = static_cast<uint16_t>(getLe(bytes, 8, 2));
    if (version != Version) throw std::runtime_error("Unsupported .levischematic version " + std::to_string(version));
    auto length = static_cast<uint32_t>(getLe(bytes, 12, 4));
    if (length == 0 || length > MaxHeaderLength) throw std::runtime_error("Invalid .levischematic header length.");
    return {length};
}
nlohmann::json parseHeader(std::string_view text) {
    auto header = nlohmann::json::parse(text.begin(), text.end(), nullptr, false);
    if (header.is_discarded() || !header.is_object() || header.value("format", std::string{}) != "levischematic")
        throw std::runtime_error("Invalid .levischematic header.");
    if (!header.contains("entries") || !header.at("entries").is_array())
        throw std::runtime_error("Missing .levischematic entry table.");
    return header;
}
void validateEntries(nlohmann::json const& header, uint64_t payloadLength) {
    for (auto const& entry : header.at("entries")) {
        auto offset = entry.at("offset").get<uint64_t>(), length = entry.at("length").get<uint64_t>();
        if (offset > payloadLength || length > payloadLength - offset)
            throw std::runtime_error("Corrupted .levischematic entry bounds.");
    }
}
} // namespace

uint32_t crc32(std::string_view bytes) {
    static auto const table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char b : bytes) c = table[(c ^ b) & 0xff] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

std::string encode(nlohmann::json header, std::span<Entry const> entries) {
    header["format"]  = "levischematic";
    header["version"] = Version;
    auto     table    = nlohmann::json::array();
    uint64_t offset   = 0;
    for (auto const& e : entries) {
        table.push_back({
            {"offset",      offset       },
            {"length",      e.data.size()},
            {"rawLength",   e.rawLength  },
            {"crc32",       e.crc32      },
            {"compression", e.compression}
        });
        offset += e.data.size();
    }
    header["entries"] = std::move(table);
    auto text         = header.dump();
    if (text.size() > MaxHeaderLength) throw std::runtime_error(".levischematic header is too large.");
    std::string out;
    out.reserve(PrefixLength + text.size() + offset);
    out.append(Magic);
    putLe(out, Version, 2);
    putLe(out, 0, 2);
    putLe(out, text.size(), 4);
    out += text;
    for (auto const& e : entries) out += e.data;
    return out;
}

nlohmann::json decodeHeader(std::string_view file) {
    auto prefix = parsePrefix(file);
    if (file.size() - PrefixLength < prefix.headerLength) throw std::runtime_error("Truncated .levischematic header.");
    auto header = parseHeader(file.substr(PrefixLength, prefix.headerLength));
    validateEntries(header, file.size() - PrefixLength - prefix.headerLength);
    header["$payload"] = PrefixLength + prefix.headerLength;
    return header;
}

nlohmann::json readHeader(std::filesystem::path const& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Cannot open file.");
    auto size = static_cast<uint64_t>(input.tellg());
    input.seekg(0);
    std::string prefix(PrefixLength, '\0');
    if (!input.read(prefix.data(), PrefixLength)) throw std::runtime_error("Not a .levischematic file.");
    auto parsed = parsePrefix(prefix);
    if (size - PrefixLength < parsed.headerLength) throw std::runtime_error("Truncated .levischematic header.");
    std::string text(parsed.headerLength, '\0');
    if (!input.read(text.data(), parsed.headerLength)) throw std::runtime_error("Truncated .levischematic header.");
    auto header = parseHeader(text);
    validateEntries(header, size - PrefixLength - parsed.headerLength);
    return header;
}

std::string_view entryBytes(std::string_view file, nlohmann::json const& header, size_t index) {
    auto const& entries = header.at("entries");
    if (index >= entries.size()) throw std::runtime_error("Missing .levischematic entry.");
    auto base = header.at("$payload").get<uint64_t>();
    return file.substr(base + entries[index].at("offset").get<uint64_t>(), entries[index].at("length").get<uint64_t>());
}

std::string replaceHeader(std::string_view file, nlohmann::json header) {
    auto old  = decodeHeader(file);
    auto base = old.at("$payload").get<uint64_t>();
    header.erase("$payload");
    header["entries"] = old.at("entries");
    header["format"]  = "levischematic";
    header["version"] = Version;
    auto text         = header.dump();
    if (text.size() > MaxHeaderLength) throw std::runtime_error(".levischematic header is too large.");
    std::string out;
    out.append(Magic);
    putLe(out, Version, 2);
    putLe(out, 0, 2);
    putLe(out, text.size(), 4);
    out += text;
    out += file.substr(base);
    return out;
}
} // namespace levischematic::schematic::container
