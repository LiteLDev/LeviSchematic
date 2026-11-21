#include "levischematic/schematic/convert/JavaNbt.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>

namespace levischematic::convert::nbt {
namespace {
constexpr int MaxDepth = 512;

std::string fromModifiedUtf8(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    auto byte = [&](size_t i) { return static_cast<unsigned char>(in[i]); };
    for (size_t i = 0; i < in.size();) {
        auto c = byte(i);
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            ++i;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < in.size()) {
            uint32_t cp = ((c & 0x1Fu) << 6) | (byte(i + 1) & 0x3Fu);
            if (cp == 0) out.push_back('\0');
            else out.append(in.substr(i, 2));
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < in.size()) {
            uint32_t cp = ((c & 0x0Fu) << 12) | ((byte(i + 1) & 0x3Fu) << 6) | (byte(i + 2) & 0x3Fu);
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 5 < in.size() && byte(i + 3) == 0xED) {
                uint32_t low = ((byte(i + 3) & 0x0Fu) << 12) | ((byte(i + 4) & 0x3Fu) << 6) | (byte(i + 5) & 0x3Fu);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    uint32_t full = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    out.push_back(static_cast<char>(0xF0 | (full >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((full >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((full >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (full & 0x3F)));
                    i += 6;
                    continue;
                }
            }
            out.append(in.substr(i, 3));
            i += 3;
        } else {
            out.push_back(static_cast<char>(c));
            ++i;
        }
    }
    return out;
}

std::string toModifiedUtf8(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    auto byte = [&](size_t i) { return static_cast<unsigned char>(in[i]); };
    for (size_t i = 0; i < in.size();) {
        auto c = byte(i);
        if (c == 0) {
            out.append("\xC0\x80");
            ++i;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < in.size()) {
            uint32_t cp  = ((c & 0x07u) << 18) | ((byte(i + 1) & 0x3Fu) << 12) | ((byte(i + 2) & 0x3Fu) << 6)
                         | (byte(i + 3) & 0x3Fu);
            cp          -= 0x10000;
            for (uint32_t unit : {0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF)}) {
                out.push_back(static_cast<char>(0xE0 | (unit >> 12)));
                out.push_back(static_cast<char>(0x80 | ((unit >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
            }
            i += 4;
        } else {
            out.push_back(static_cast<char>(c));
            ++i;
        }
    }
    return out;
}

class Reader {
public:
    explicit Reader(std::string_view data) : mData(data) {}

    template <typename T>
    T number() {
        need(sizeof(T));
        std::array<char, sizeof(T)> raw{};
        std::memcpy(raw.data(), mData.data() + mPos, sizeof(T));
        mPos += sizeof(T);
        if constexpr (std::endian::native == std::endian::little) std::reverse(raw.begin(), raw.end());
        return std::bit_cast<T>(raw);
    }
    std::string string() {
        auto length = number<uint16_t>();
        need(length);
        auto result  = fromModifiedUtf8(mData.substr(mPos, length));
        mPos        += length;
        return result;
    }
    size_t count(size_t elementSize) {
        auto length = number<int32_t>();
        if (length < 0) throw std::runtime_error("NBT: negative array length.");
        if (elementSize > 0 && static_cast<size_t>(length) > (mData.size() - mPos) / elementSize)
            throw std::runtime_error("NBT: array length exceeds data.");
        return static_cast<size_t>(length);
    }
    Tag payload(TagType type, int depth) {
        if (depth > MaxDepth) throw std::runtime_error("NBT: nesting too deep.");
        switch (type) {
        case TagType::Byte:
            return number<int8_t>();
        case TagType::Short:
            return number<int16_t>();
        case TagType::Int:
            return number<int32_t>();
        case TagType::Long:
            return number<int64_t>();
        case TagType::Float:
            return number<float>();
        case TagType::Double:
            return number<double>();
        case TagType::ByteArray: {
            std::vector<int8_t> v(count(1));
            for (auto& e : v) e = number<int8_t>();
            return v;
        }
        case TagType::String:
            return string();
        case TagType::List: {
            List list;
            list.elementType = checkedType(number<uint8_t>());
            auto n           = count(list.elementType == TagType::End ? 0 : 1);
            if (list.elementType == TagType::End && n > 0) throw std::runtime_error("NBT: list of End tags.");
            list.items.reserve(n);
            for (size_t i = 0; i < n; ++i) list.items.push_back(payload(list.elementType, depth + 1));
            return list;
        }
        case TagType::Compound: {
            Compound compound;
            for (;;) {
                auto child = checkedType(number<uint8_t>());
                if (child == TagType::End) break;
                auto name = string();
                compound.entries.emplace_back(std::move(name), payload(child, depth + 1));
            }
            return compound;
        }
        case TagType::IntArray: {
            std::vector<int32_t> v(count(4));
            for (auto& e : v) e = number<int32_t>();
            return v;
        }
        case TagType::LongArray: {
            std::vector<int64_t> v(count(8));
            for (auto& e : v) e = number<int64_t>();
            return v;
        }
        default:
            throw std::runtime_error("NBT: unexpected End tag.");
        }
    }
    [[nodiscard]] bool atEnd() const { return mPos == mData.size(); }

private:
    static TagType checkedType(uint8_t raw) {
        if (raw > static_cast<uint8_t>(TagType::LongArray)) throw std::runtime_error("NBT: unknown tag type.");
        return static_cast<TagType>(raw);
    }
    void need(size_t n) const {
        if (mData.size() - mPos < n) throw std::runtime_error("NBT: unexpected end of data.");
    }
    std::string_view mData;
    size_t           mPos = 0;
};

class Writer {
public:
    template <typename T>
    void number(T value) {
        auto raw = std::bit_cast<std::array<char, sizeof(T)>>(value);
        if constexpr (std::endian::native == std::endian::little) std::reverse(raw.begin(), raw.end());
        mOut.append(raw.data(), raw.size());
    }
    void string(std::string_view value) {
        auto encoded = toModifiedUtf8(value);
        if (encoded.size() > UINT16_MAX) throw std::runtime_error("NBT: string too long.");
        number(static_cast<uint16_t>(encoded.size()));
        mOut += encoded;
    }
    void length(size_t n) {
        if (n > static_cast<size_t>(INT32_MAX)) throw std::runtime_error("NBT: array too long.");
        number(static_cast<int32_t>(n));
    }
    void payload(Tag const& tag) {
        std::visit(
            [&](auto const& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) throw std::runtime_error("NBT: cannot write End tag.");
                else if constexpr (std::is_arithmetic_v<T>) number(v);
                else if constexpr (std::is_same_v<T, std::string>) string(v);
                else if constexpr (std::is_same_v<T, List>) {
                    auto type = v.items.empty() ? v.elementType : v.items.front().type();
                    for (auto const& item : v.items)
                        if (item.type() != type) throw std::runtime_error("NBT: mixed list element types.");
                    number(static_cast<uint8_t>(type));
                    length(v.items.size());
                    for (auto const& item : v.items) payload(item);
                } else if constexpr (std::is_same_v<T, Compound>) {
                    for (auto const& [name, child] : v.entries) {
                        number(static_cast<uint8_t>(child.type()));
                        string(name);
                        payload(child);
                    }
                    number(uint8_t{0});
                } else {
                    length(v.size());
                    for (auto e : v) number(e);
                }
            },
            tag.value
        );
    }
    std::string take() { return std::move(mOut); }

private:
    std::string mOut;
};
}

Tag const* Compound::find(std::string_view key) const {
    for (auto const& [name, tag] : entries)
        if (name == key) return &tag;
    return nullptr;
}
Tag* Compound::find(std::string_view key) {
    for (auto& [name, tag] : entries)
        if (name == key) return &tag;
    return nullptr;
}
Tag& Compound::put(std::string key, Tag value) {
    if (auto* existing = find(key)) return *existing = std::move(value);
    return entries.emplace_back(std::move(key), std::move(value)).second;
}
bool Tag::integer(int64_t& out) const {
    switch (type()) {
    case TagType::Byte:
        out = std::get<int8_t>(value);
        return true;
    case TagType::Short:
        out = std::get<int16_t>(value);
        return true;
    case TagType::Int:
        out = std::get<int32_t>(value);
        return true;
    case TagType::Long:
        out = std::get<int64_t>(value);
        return true;
    default:
        return false;
    }
}

NamedRoot readBigEndian(std::string_view bytes) {
    Reader reader(bytes);
    auto   type = reader.number<uint8_t>();
    if (type != static_cast<uint8_t>(TagType::Compound)) throw std::runtime_error("NBT: root is not a compound.");
    NamedRoot root;
    root.name = reader.string();
    root.tag  = reader.payload(TagType::Compound, 0);
    return root;
}
std::string writeBigEndian(NamedRoot const& root) {
    if (root.tag.type() != TagType::Compound) throw std::runtime_error("NBT: root is not a compound.");
    Writer writer;
    writer.number(static_cast<uint8_t>(TagType::Compound));
    writer.string(root.name);
    writer.payload(root.tag);
    return writer.take();
}
}
