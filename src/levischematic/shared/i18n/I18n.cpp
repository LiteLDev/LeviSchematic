#include "levischematic/shared/i18n/I18n.h"
#include "ll/api/i18n/I18n.h"
#include <mutex>
#include <shared_mutex>

namespace levischematic::i18n {
namespace {
struct State {
    std::shared_mutex lock;
    ll::i18n::I18n    table;
    std::string       locale = "en_US";
};
State& state() {
    static State value;
    return value;
}
} // namespace

void reload(std::filesystem::path const& builtin, std::filesystem::path const& overrides, std::string code) {
    auto&            s = state();
    std::unique_lock guard(s.lock);
    s.table.clear();
    std::error_code error;
    if (std::filesystem::is_directory(builtin, error)) (void)s.table.load(builtin);
    if (std::filesystem::is_directory(overrides, error)) (void)s.table.load(overrides);
    s.locale = code.empty() ? std::string("en_US") : std::move(code);
}

std::string locale() {
    auto&            s = state();
    std::shared_lock guard(s.lock);
    return s.locale;
}

std::string pattern(std::string_view key) {
    auto&            s = state();
    std::shared_lock guard(s.lock);
    for (std::string_view code : {std::string_view(s.locale), std::string_view("en_US")}) {
        auto value = s.table.get(key, code);
        if (!value.empty() && value != key) return std::string(value);
    }
    return std::string(key);
}
} // namespace levischematic::i18n
