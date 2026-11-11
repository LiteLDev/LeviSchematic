#include "UiI18n.h"
#include "UiLanguage.h"
#include "Ui.h"
#include "UiCatalog.h"
#include "levischematic/LeviSchematic.h"
#include "ll/api/i18n/I18n.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/options/IOptionRegistry.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
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
bool uiChinese() { return locale.starts_with("zh"); }
std::string localizedText(std::string text) {
    return selectUiLanguage(text, uiChinese());
}
std::string describeRow(Row const& row) {
    static std::map<std::string, std::string> const descriptions{
        {"/visibility/renderDistanceBlocks",    "distance"    },
        {"/visibility/frustumCulling",          "frustum"     },
        {"/projection/alpha",                   "alpha"       },
        {"/projection/texture",                 "texture"     },
        {"/hull/expansion",                     "expansion"   },
        {"/hull/depthBias",                     "depth"       },
        {"/hull/slopeBias",                     "slope"       },
        {"/performance/tickBudgetMs",           "tick"        },
        {"/performance/frameBudgetMs",          "frame"       },
        {"/performance/maxLoadRequests",        "loads"       },
        {"/performance/geometryBuildsPerFrame", "builds"      },
        {"/performance/cameraResortsPerFrame",  "resorts"     },
        {"/performance/queuedChunkBuilds",      "queue"       },
        {"/diagnostics",                        "diagnostics" },
        {"enabled",                             "enabled"     },
        {"render",                              "render"      },
        {"layers",                              "layers"      },
        {"file",                                "file"        },
        {"refresh",                             "refresh"     },
        {"layer_range",                         "range"       },
        {"hud_position",                        "hud_position"},
        {"/hud/enabled",                        "hud_enabled" }
    };
    std::string kind;
    if (auto found = descriptions.find(row.id); found != descriptions.end()) kind = found->second;
    else if (row.id.starts_with("origin")) kind = "origin";
    else if (row.id.starts_with("move")) kind = "move";
    else if (row.id == "rotate" || row.id == "mirror") kind = "transform";
    else if (row.kind == RowKind::Color) kind = "color";
    else if (row.kind == RowKind::KeyBinding) kind = "key";
    else if (row.kind == RowKind::Toggle) kind = "toggle";
    else if (row.kind == RowKind::Action) kind = "action";
    else kind = "input";
    std::string result = tr("ui.desc." + kind);
    if (row.kind == RowKind::Slider) {
        std::ostringstream value;
        value << std::setprecision(6) << std::stod(row.value) * row.displayScale << row.unit << " ("
              << row.minimum * row.displayScale << " ~ " << row.maximum * row.displayScale << row.unit << ")";
        result = tr("ui.current") + value.str() + ". " + result;
    } else if (row.kind == RowKind::Toggle) {
        result = tr(row.value == "true" ? "ui.state_on" : "ui.state_off") + ". " + result;
    }
    return result;
}
} 
