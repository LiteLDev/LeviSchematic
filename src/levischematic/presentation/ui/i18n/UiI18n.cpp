#include "levischematic/presentation/ui/i18n/UiI18n.h"
#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/presentation/ui/model/Ui.h"
#include "levischematic/shared/i18n/I18n.h"
#include "ll/api/i18n/I18n.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/options/IOptionRegistry.h"
#include <iomanip>
#include <map>
#include <sstream>

namespace levischematic::ui {
void reloadTranslations() {
    std::string code;
    if (auto client = ll::service::getClientInstance()) code = client->getOptions().getLanguage();
    else code = std::string(ll::i18n::getDefaultLocaleCode());
    auto& self = LeviSchematic::getInstance().getSelf();
    i18n::reload(self.getModDir() / "assets" / "lang", self.getConfigDir() / "lang", std::move(code));
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
} // namespace levischematic::ui
