#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/args.h>
#include <fmt/format.h>

namespace levischematic::i18n {
void        reload(std::filesystem::path const& builtin, std::filesystem::path const& overrides, std::string locale);
std::string locale();
std::string pattern(std::string_view key);
inline std::string tr(std::string_view key) { return pattern(key); }
template <class... Args>
    requires(sizeof...(Args) > 0)
std::string tr(std::string_view key, Args&&... args) {
    auto text = pattern(key);
    try {
        fmt::dynamic_format_arg_store<fmt::format_context> store;
        (store.push_back(std::forward<Args>(args)), ...);
        return fmt::vformat(text, store);
    } catch (...) {
        return text;
    }
}
} // namespace levischematic::i18n

namespace levischematic {
using i18n::tr;
}
