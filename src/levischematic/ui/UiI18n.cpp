#include "UiI18n.h"
#include "UiCatalog.h"
#include "levischematic/LeviSchematic.h"
#include "ll/api/i18n/I18n.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/options/IOptionRegistry.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace levischematic::ui {
namespace {
ll::i18n::I18n& translations() {
    static ll::i18n::I18n value;
    return value;
}
std::string locale = "en_US";
} 
void reloadTranslations() {
    auto& table = translations();
    table.clear();
    nlohmann::ordered_json en, zh;
    for (auto const& item : uiTranslations) {
        table.set("en_US", item.key, item.en);
        table.set("zh_CN", item.key, item.zh);
        en[item.key] = item.en;
        zh[item.key] = item.zh;
    }
    if (auto client = ll::service::getClientInstance()) locale = client->getOptions().getLanguage();
    else locale = std::string(ll::i18n::getDefaultLocaleCode());
    auto directory = LeviSchematic::getInstance().getSelf().getConfigDir() / "i18n" / "ui";
    try {
        std::filesystem::create_directories(directory);
        for (auto const& [name, data] : {
                 std::pair{"en_US.json", en},
                 {"zh_CN.json", zh}
        }) {
            auto file = directory / name;
            if (!std::filesystem::exists(file)) {
                std::ofstream out(file, std::ios::binary);
                out << data.dump(2) << '\n';
                if (!out) throw std::runtime_error("Cannot write default UI translations");
            }
        }
        auto result = table.load(directory);
        if (!result)
            LeviSchematic::getInstance().getSelf().getLogger().warn(
                "Cannot load custom UI translations; using available defaults"
            );
    } catch (std::exception const& e) {
        LeviSchematic::getInstance().getSelf().getLogger().warn("UI translations: {}", e.what());
    }
}
std::string tr(std::string_view key) { return std::string(translations().get(key, locale)); }
} 
