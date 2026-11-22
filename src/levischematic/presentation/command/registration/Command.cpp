#include "levischematic/presentation/command/registration/Command.h"
#include "levischematic/shared/i18n/I18n.h"

#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/presentation/command/shared/CommandShared.h"
#include "levischematic/presentation/ui/model/Ui.h"

namespace levischematic::command {
void registerSchemCoreCommands(ll::command::CommandHandle&);
void registerSchemOrganizationCommands(ll::command::CommandHandle&);
void registerSchemAdvancedCommands(ll::command::CommandHandle&);
namespace {

auto& getLogger() { return levischematic::LeviSchematic::getInstance().getSelf().getLogger(); }

} // namespace

void registerCommands(bool isClient) {
    auto& schemCmd = getOrCreateSchemCommand(isClient);
    schemCmd.overload().text("ui").text("status").execute([](CommandOrigin const&, CommandOutput& output) {
        output.success(ui::diagnostics());
    });
    schemCmd.overload().text("ui").text("layout").execute([](CommandOrigin const&, CommandOutput& output) {
        output.success(ui::exportLayout());
    });
    for (auto const& [name, page] : {
             std::pair{"load",       ui::PageId::Load            },
             {"tasks",      ui::PageId::Tasks           },
             {"placements", ui::PageId::Projection      },
             {"settings",   ui::PageId::Settings        },
             {"keys",       ui::PageId::KeyBindings     },
             {"main",       ui::PageId::MainMenu        },
             {"hud",        ui::PageId::HudLayout       },
             {"placement",  ui::PageId::Placement       },
             {"selection",  ui::PageId::Selection       },
             {"materials",  ui::PageId::Materials       },
             {"survival",   ui::PageId::Survival        },
             {"manager",    ui::PageId::SchematicManager},
             {"areas",      ui::PageId::AreaSelections  },
             {"subregion",  ui::PageId::SubRegion       },
             {"grid",       ui::PageId::PlacementGrid   }
    })
        schemCmd.overload().text("ui").text(name).execute([page](CommandOrigin const&, CommandOutput& output) {
            auto session = acquireSession(output);
            if (!session) return;
            ui::requestOpen(page);
            output.success(tr("cmd.registration.requested_ui_page"));
        });
    schemCmd.overload().text("ui").execute([](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        ui::requestOpen();
        output.success(tr("cmd.registration.requested_ui"));
    });
    registerSchemLoadCommands(schemCmd);
    registerSchemTransformCommands(schemCmd);
    registerSchemPlacementCommands(schemCmd);
    registerSchemSelectionCommands(schemCmd);
    registerSchemViewCommands(schemCmd);
    registerSchemConfigCommands(schemCmd);
    registerSchemRendererCommands(schemCmd);
    registerSchemCoreCommands(schemCmd);
    registerSchemOrganizationCommands(schemCmd);
    registerSchemAdvancedCommands(schemCmd);

    getLogger().info("LeviSchematic commands registered");
}

} // namespace levischematic::command
