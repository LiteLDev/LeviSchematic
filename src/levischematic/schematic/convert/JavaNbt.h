#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace levischematic::convert::nbt {
enum class TagType : uint8_t {
    End       = 0,
    Byte      = 1,
    Short     = 2,
    Int       = 3,
    Long      = 4,
    Float     = 5,
    Double    = 6,
    ByteArray = 7,
    String    = 8,
    List      = 9,
    Compound  = 10,
    IntArray  = 11,
    LongArray = 12,
};

struct Tag;
struct List {
    TagType          elementType = TagType::End;
    std::vector<Tag> items;
};
struct Compound {
    std::vector<std::pair<std::string, Tag>> entries;

    [[nodiscard]] Tag const* find(std::string_view key) const;
    [[nodiscard]] Tag*       find(std::string_view key);
    Tag&                     put(std::string key, Tag value);
};

struct Tag {
    using Value = std::variant<
        std::monostate,
        int8_t,
        int16_t,
        int32_t,
        int64_t,
        float,
        double,
        std::vector<int8_t>,
        std::string,
        List,
        Compound,
        std::vector<int32_t>,
        std::vector<int64_t>>;
    Value value;

    Tag() = default;
    template <typename T>
        requires std::is_constructible_v<Value, T&&>
    Tag(T&& v) : value(std::forward<T>(v)) {}

    [[nodiscard]] TagType type() const { return static_cast<TagType>(value.index()); }
    template <typename T>
    [[nodiscard]] T const* as() const {
        return std::get_if<T>(&value);
    }
    template <typename T>
    [[nodiscard]] T* as() {
        return std::get_if<T>(&value);
    }
    [[nodiscard]] bool integer(int64_t& out) const;
};

struct NamedRoot {
    std::string name;
    Tag         tag;
};

NamedRoot   readBigEndian(std::string_view bytes);
std::string writeBigEndian(NamedRoot const& root);
} // namespace levischematic::convert::nbt
