#include "levischematic/command/CommandShared.h"

#include "levischematic/schematic/placement/PatchOp.h"
#include "levischematic/schematic/placement/PlacementModel.h"

#include "mc/server/commands/Command.h"
#include "mc/server/commands/CommandBlockNameResult.h"

namespace levischematic::command {

void registerSchemPlacementCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload().text("list").execute(
    




    [&](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        auto  placements       = placementService.managedPlacements();
        if (placements.empty()) {
            output.success("No placements loaded.");
            return;
        }

        std::string msg        = "Loaded placements (" + std::to_string(placements.size()) + "):\n";
        auto        selectedId = placementService.selectedPlacementId();
        for (auto const& placementRef : placements) {
            auto const& placement = placementRef.get();
            auto const& pos       = placement.origin;
            bool        current   = selectedId && *selectedId == placement.id;

            msg += current ? "-> " : "   ";
            msg += "[" + std::to_string(placement.id) + "] ";
            msg += "'" + placement.name + "' ";
            msg += "at (" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + ") ";
            msg += placement.describeTransform();
            msg += placement.enabled ? "" : " [DISABLED]";
            msg += placement.renderEnabled ? "" : " [HIDDEN]";
            msg += "\n";
        }

        output.success(msg);
    });

    schemCmd.overload<SchemRemoveParam>().text("remove").required("id").execute(
    





    [&](CommandOrigin const& origin, CommandOutput& output, SchemRemoveParam const& param) {
            auto session = acquireSession(output);
            if (!session) return;
            auto& placementService = session->placement();
            auto  result           = placementService.removePlacement(static_cast<placement::PlacementId>(param.id));
            if (!result) {
                replyPlacementError(output, "command.removePlacement", result.error());
                return;
            }

            flushPlacementRefreshAndReply(*session, origin, output,
            



            [&](CommandOutput& out) {
                out.success("Removed placement '{}' [ID: {}]", result.value().name, result.value().id);
            });
        }
    );

    schemCmd.overload().text("remove").execute(
    




    [&](CommandOrigin const& origin, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        withSelectedPlacement(placementService, output,
        



        [&](placement::PlacementInstance const& selected) {
            auto result = placementService.removePlacement(selected.id);
            if (!result) {
                replyPlacementError(output, "command.removePlacement", result.error());
                return;
            }

            flushPlacementRefreshAndReply(*session, origin, output,
            



            [&](CommandOutput& out) {
                out.success("Removed placement '{}' [ID: {}]", result.value().name, result.value().id);
            });
        });
    });

    schemCmd.overload<SchemSelectParam>().text("select").required("id").execute(
    





    [&](CommandOrigin const&, CommandOutput& output, SchemSelectParam const& param) {
            auto session = acquireSession(output);
            if (!session) return;
            auto& placementService = session->placement();
            auto  selected         = placementService.requirePlacement(static_cast<placement::PlacementId>(param.id));
            if (!selected) {
                replyPlacementError(output, "command.selectPlacement", selected.error());
                return;
            }

            auto selectResult = placementService.selectPlacement(static_cast<placement::PlacementId>(param.id));
            if (!selectResult) {
                replyPlacementError(output, "command.selectPlacement", selectResult.error());
                return;
            }

            output.success("Selected placement '{}' [ID: {}]", selected.value()->name, param.id);
        }
    );

    schemCmd.overload().text("toggle").text("render").execute(
    




    [&](CommandOrigin const& origin, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        withSelectedPlacement(placementService, output,
        



        [&](placement::PlacementInstance const& selected) {
            auto updated = placementService.togglePlacementRender(selected.id);
            if (!updated) {
                replyPlacementError(output, "command.togglePlacementRender", updated.error());
                return;
            }

            flushPlacementRefreshAndReply(*session, origin, output,
            



            [&](CommandOutput& out) {
                out.success(
                    "Render for '{}': {}",
                    selected.name,
                    updated.value() && updated.value()->renderEnabled ? "ON" : "OFF"
                );
            });
        });
    });

    schemCmd.overload().text("toggle").execute(
    




    [&](CommandOrigin const& origin, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        withSelectedPlacement(placementService, output,
        



        [&](placement::PlacementInstance const& selected) {
            auto updated = placementService.togglePlacementEnabled(selected.id);
            if (!updated) {
                replyPlacementError(output, "command.togglePlacementEnabled", updated.error());
                return;
            }

            flushPlacementRefreshAndReply(*session, origin, output,
            



            [&](CommandOutput& out) {
                out.success(
                    "Placement '{}': {}",
                    selected.name,
                    updated.value() && updated.value()->enabled ? "ENABLED" : "DISABLED"
                );
            });
        });
    });

    schemCmd.overload<SchemBlockPosParam>().text("block").text("hide").required("pos").execute(
    






    [&](CommandOrigin const& origin, CommandOutput& output, SchemBlockPosParam const& param, Command const& cmd) {
            auto session = acquireSession(output);
            if (!session) return;
            auto& placementService = session->placement();
            withSelectedPlacement(placementService, output,
            



            [&](placement::PlacementInstance const& selected) {
                auto worldPos = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                auto updated  = placementService.patchPlacementBlock(selected.id, worldPos, placement::PatchOp::remove());
                if (!updated) {
                    replyPlacementError(output, "command.patchPlacementBlock", updated.error());
                    return;
                }

                flushPlacementRefreshAndReply(*session, origin, output,
                



                [&](CommandOutput& out) {
                    out.success("Hidden block at ({}) for '{}'.", worldPos.toString(), selected.name);
                });
            });
        }
    );

    schemCmd.overload<SchemBlockSetParam>()
        .text("block")
        .text("set")
        .required("pos")
        .required("blockName")
        .required("title_date")
        .execute(
        






        [&](CommandOrigin const&      origin,
                     CommandOutput&            output,
                     SchemBlockSetParam const& param,
                     Command const&            cmd) {
            auto session = acquireSession(output);
            if (!session) return;
            auto& placementService = session->placement();
            withSelectedPlacement(placementService, output,
            



            [&](placement::PlacementInstance const& selected) {
                auto worldPos    = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                auto blockResult = param.blockName.resolveBlock(param.title_date);
                if (!blockResult.mBlock) {
                    output.error("Failed to resolve block '{}'.", param.blockName.getBlockName());
                    return;
                }

                auto updated = placementService.patchPlacementBlock(
                    selected.id,
                    worldPos,
                    placement::PatchOp::setBlock(blockResult.mBlock)
                );
                if (!updated) {
                    replyPlacementError(output, "command.patchPlacementBlock", updated.error());
                    return;
                }

                flushPlacementRefreshAndReply(*session, origin, output,
                



                [&](CommandOutput& out) {
                    out.success(
                        "Set block at ({}) for '{}' to '{}'.",
                        worldPos.toString(),
                        selected.name,
                        param.blockName.getBlockName()
                    );
                });
            });
        });

    schemCmd.overload<SchemBlockSetSimpleParam>()
        .text("block")
        .text("set")
        .required("pos")
        .required("blockName")
        .execute(
        






        [&](CommandOrigin const&            origin,
                     CommandOutput&                  output,
                     SchemBlockSetSimpleParam const& param,
                     Command const&                  cmd) {
            auto session = acquireSession(output);
            if (!session) return;
            auto& placementService = session->placement();
            withSelectedPlacement(placementService, output,
            



            [&](placement::PlacementInstance const& selected) {
                auto worldPos    = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                auto blockResult = param.blockName.resolveBlock(0);
                if (!blockResult.mBlock) {
                    output.error("Failed to resolve block '{}'.", param.blockName.getBlockName());
                    return;
                }

                auto updated = placementService.patchPlacementBlock(
                    selected.id,
                    worldPos,
                    placement::PatchOp::setBlock(blockResult.mBlock)
                );
                if (!updated) {
                    replyPlacementError(output, "command.patchPlacementBlock", updated.error());
                    return;
                }

                flushPlacementRefreshAndReply(*session, origin, output,
                



                [&](CommandOutput& out) {
                    out.success(
                        "Set block at ({}) for '{}' to '{}'.",
                        worldPos.toString(),
                        selected.name,
                        param.blockName.getBlockName()
                    );
                });
            });
        });

    schemCmd.overload<SchemBlockPosParam>().text("block").text("clear").required("pos").execute(
    






    [&](CommandOrigin const& origin, CommandOutput& output, SchemBlockPosParam const& param, Command const& cmd) {
            auto session = acquireSession(output);
            if (!session) return;
            auto& placementService = session->placement();
            withSelectedPlacement(placementService, output,
            



            [&](placement::PlacementInstance const& selected) {
                auto worldPos = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                auto updated =
                    placementService.patchPlacementBlock(selected.id, worldPos, placement::PatchOp::clearOverride());
                if (!updated) {
                    replyPlacementError(output, "command.patchPlacementBlock", updated.error());
                    return;
                }

                flushPlacementRefreshAndReply(*session, origin, output,
                



                [&](CommandOutput& out) {
                    out.success("Cleared block override at ({}) for '{}'.", worldPos.toString(), selected.name);
                });
            });
        }
    );

    schemCmd.overload().text("info").execute(
    




    [&](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        withSelectedPlacement(placementService, output,
        



        [&](placement::PlacementInstance const& selected) {
            auto pos              = selected.origin;
            auto [minBox, maxBox] = placement::computeEnclosingBox(selected);

            std::string msg  = "Placement Info:\n";
            msg             += "  Name: " + selected.name + "\n";
            msg             += "  ID: " + std::to_string(selected.id) + "\n";
            msg += "  Origin: (" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z)
                 + ")\n";
            msg += "  Transform: " + selected.describeTransform() + "\n";
            msg += "  Enabled: " + std::string(selected.enabled ? "Yes" : "No") + "\n";
            msg += "  Render: " + std::string(selected.renderEnabled ? "Yes" : "No") + "\n";
            msg += "  Non-air blocks: " + std::to_string(selected.totalNonAirBlocks()) + "\n";
            msg += "  Bounding box: (" + std::to_string(minBox.x) + "," + std::to_string(minBox.y) + ","
                 + std::to_string(minBox.z) + ") to (" + std::to_string(maxBox.x) + "," + std::to_string(maxBox.y) + ","
                 + std::to_string(maxBox.z) + ")\n";

            output.success(msg);
        });
    });

    schemCmd.overload().text("clear").execute(
    




    [&](CommandOrigin const& origin, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        auto  count            = placementService.managedPlacements().size();
        session->loads().cancelAll();
        placementService.clearPlacements();
        flushPlacementRefreshAndReply(*session, origin, output,
        



        [&](CommandOutput& out) {
            out.success("Cleared {} placement(s).", count);
        });
    });

    schemCmd.overload().execute(
    




    [&](CommandOrigin const&, CommandOutput& output) {
        std::string help  = "LeviSchematic Commands:\n";
        help += "  /schem ui - Open the Litematica-style main menu (M / F7)\n";
        help += "  /schem ui <main|load|placements|placement|selection|settings|keys|materials|survival|tasks|hud> - UI pages\n";
        help += "  /schem ui layout - Export the last completed menu frame geometry\n";
        help              += "  /schem ui <world|projects|signs> - P4 workflow pages\n";
        help += "  /schem world <paste|gridpaste|fill|replace|delete|clone|move> - Select actual-world tool\n";
        help += "  /schem world <prepare|start|pause|resume|cancel|status> - Preflight and task control\n";
        help += "  /schem world offset <dx dy dz> | policy <air|nonair|all> | sample <primary|secondary>\n";
        help +=
            "  /schem project <create|open|rename|save|work> <name>; checkout <version>; list|status|paste|cancel\n";
        help             += "  /schem sign <copy|copyback|paste|cancel> - Next native sign editor paste\n";
        help += "  /schem ui status - Show C++ UI backend and lifecycle diagnostics\n";
        help += "  /schem ui <save|layers|verifier|session|resources|edit> - Open a workflow page\n";
        help += "  /schem resource <status|save|reload-discard|cancel> | saveas <file>\n";
        help += "  /schem edit set <x y z> <block> - Edit shared schematic content\n";
        help += "  /schem grid enabled <bool> | grid <x|y|z> <spacing> <negative> <positive>\n";
        help += "  /schem selection normal <bool> | add <name> | region <id>\n";
        help += "  /schem save <file> - Queue progressive world selection capture (no overwrite)\n";
        help += "  /schem save-status | save-cancel - Inspect/cancel selection capture\n";
        help += "  /schem verify <start|layers|stop|reset|info> - Selected placement verification\n";
        help += "  /schem layer mode <all|single|range|below|above> - Set layer mode\n";
        help += "  /schem layer axis <x|y|z> | boundary <value> | info\n";
        help += "  M: menu; Ctrl+Alt+S: save; M+V: verifier; M+PageUp/PageDown: layer mode\n";
        help             += "  /schem load <file> [x y z] - Queue schematic load\n";
        help += "  /schem load status <taskId> - Show load progress\n";
        help += "  /schem load cancel <taskId> - Cancel an unfinished load\n";
        help += "  /schem config reload - Validate and apply config.json on next tick\n";
        help += "  /schem config save - Save effective/pending configuration\n";
        help             += "  /schem move <dx> <dy> <dz> - Move placement\n";
        help             += "  /schem origin <x> <y> <z> - Set origin\n";
        help             += "  /schem rotate <cw90|ccw90|r180> - Rotate\n";
        help             += "  /schem mirror <x|z|none> - Mirror\n";
        help             += "  /schem list - List placements\n";
        help             += "  /schem select <id> - Select placement\n";
        help             += "  /schem remove [id] - Remove placement\n";
        help             += "  /schem toggle [render] - Toggle placement state\n";
        help             += "  /schem info - Show placement info\n";
        help             += "  /schem block hide <x y z> - Hide a projected block\n";
        help             += "  /schem block set <x y z> <block> - Override one projected block\n";
        help             += "  /schem block clear <x y z> - Clear one block override\n";
        help             += "  /schem files - List available files\n";
        help             += "  /schem reset - Reset transform\n";
        help             += "  /schem clear - Clear placements and cancel unfinished loads\n";
        help             += "  /schem pos1 [x y z] - Set selection corner 1\n";
        help             += "  /schem pos2 [x y z] - Set selection corner 2\n";
        help             += "  /schem selection clear - Clear selection\n";
        help             += "  /schem selection mode - Toggle selection mode\n";
        help             += "  /schem selection info - Show selection info\n";
        help             += "  /schem view y min <value> - Set minimum visible Y and enable filtering\n";
        help             += "  /schem view y max <value> - Set maximum visible Y and enable filtering\n";
        help             += "  /schem view y range <min> <max> - Set visible Y range and enable filtering\n";
        help             += "  /schem view y enable - Enable Y range filtering\n";
        help             += "  /schem view y disable - Disable Y range filtering\n";
        help             += "  /schem view y info - Show Y range filter state\n";
        help             += "  /schem renderer stats - Show preview state and block-entity highlight routing\n";
        help += "  /schem renderer reset - Release preview resources; keep placements and rebuild next frame\n";
        output.success(help);
    });
}

} 
