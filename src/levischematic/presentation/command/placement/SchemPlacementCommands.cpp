#include "levischematic/presentation/command/shared/CommandShared.h"
#include "levischematic/shared/i18n/I18n.h"

#include "levischematic/schematic/placement/model/PatchOp.h"
#include "levischematic/schematic/placement/model/PlacementModel.h"

#include "mc/server/commands/Command.h"
#include "mc/server/commands/CommandBlockNameResult.h"

namespace levischematic::command {

void registerSchemPlacementCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload().text("list").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        auto  placements       = placementService.managedPlacements();
        if (placements.empty()) {
            output.success(tr("cmd.placement.none_loaded"));
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

            flushPlacementRefreshAndReply(*session, origin, output, [&](CommandOutput& out) {
                out.success(tr("cmd.placement.removed", result.value().name, result.value().id));
            });
        }
    );

    schemCmd.overload().text("remove").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
            auto result = placementService.removePlacement(selected.id);
            if (!result) {
                replyPlacementError(output, "command.removePlacement", result.error());
                return;
            }

            flushPlacementRefreshAndReply(*session, origin, output, [&](CommandOutput& out) {
                out.success(tr("cmd.placement.removed", result.value().name, result.value().id));
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

            output.success(tr("cmd.placement.selected", selected.value()->name, param.id));
        }
    );

    schemCmd.overload().text("toggle").text("render").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
            auto updated = placementService.togglePlacementRender(selected.id);
            if (!updated) {
                replyPlacementError(output, "command.togglePlacementRender", updated.error());
                return;
            }

            flushPlacementRefreshAndReply(*session, origin, output, [&](CommandOutput& out) {
                out.success(
                    tr("cmd.placement.render_for",
                       selected.name,
                       updated.value() && updated.value()->renderEnabled ? "ON" : "OFF")
                );
            });
        });
    });

    schemCmd.overload().text("toggle").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
            auto updated = placementService.togglePlacementEnabled(selected.id);
            if (!updated) {
                replyPlacementError(output, "command.togglePlacementEnabled", updated.error());
                return;
            }

            flushPlacementRefreshAndReply(*session, origin, output, [&](CommandOutput& out) {
                out.success(
                    tr("cmd.placement.state",
                       selected.name,
                       updated.value() && updated.value()->enabled ? "ENABLED" : "DISABLED")
                );
            });
        });
    });

    schemCmd.overload<SchemBlockPosParam>().text("block").text("hide").required("pos").execute(
        [&](CommandOrigin const& origin, CommandOutput& output, SchemBlockPosParam const& param, Command const& cmd) {
            auto session = acquireSession(output);
            if (!session) return;
            auto& placementService = session->placement();
            withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
                auto worldPos = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                auto updated =
                    placementService.patchPlacementBlock(selected.id, worldPos, placement::PatchOp::remove());
                if (!updated) {
                    replyPlacementError(output, "command.patchPlacementBlock", updated.error());
                    return;
                }

                flushPlacementRefreshAndReply(*session, origin, output, [&](CommandOutput& out) {
                    out.success(tr("cmd.placement.hidden_block", worldPos.toString(), selected.name));
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
        .execute([&](CommandOrigin const&      origin,
                     CommandOutput&            output,
                     SchemBlockSetParam const& param,
                     Command const&            cmd) {
            auto session = acquireSession(output);
            if (!session) return;
            auto& placementService = session->placement();
            withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
                auto worldPos    = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                auto blockResult = param.blockName.resolveBlock(param.title_date);
                if (!blockResult.mBlock) {
                    output.error(tr("cmd.placement.resolve_failed", param.blockName.getBlockName()));
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

                flushPlacementRefreshAndReply(*session, origin, output, [&](CommandOutput& out) {
                    out.success(
                        tr("cmd.placement.set_block",
                           worldPos.toString(),
                           selected.name,
                           param.blockName.getBlockName())
                    );
                });
            });
        });

    schemCmd.overload<SchemBlockSetSimpleParam>()
        .text("block")
        .text("set")
        .required("pos")
        .required("blockName")
        .execute([&](CommandOrigin const&            origin,
                     CommandOutput&                  output,
                     SchemBlockSetSimpleParam const& param,
                     Command const&                  cmd) {
            auto session = acquireSession(output);
            if (!session) return;
            auto& placementService = session->placement();
            withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
                auto worldPos    = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                auto blockResult = param.blockName.resolveBlock(0);
                if (!blockResult.mBlock) {
                    output.error(tr("cmd.placement.resolve_failed", param.blockName.getBlockName()));
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

                flushPlacementRefreshAndReply(*session, origin, output, [&](CommandOutput& out) {
                    out.success(
                        tr("cmd.placement.set_block",
                           worldPos.toString(),
                           selected.name,
                           param.blockName.getBlockName())
                    );
                });
            });
        });

    schemCmd.overload<SchemBlockPosParam>().text("block").text("clear").required("pos").execute(
        [&](CommandOrigin const& origin, CommandOutput& output, SchemBlockPosParam const& param, Command const& cmd) {
            auto session = acquireSession(output);
            if (!session) return;
            auto& placementService = session->placement();
            withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
                auto worldPos = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                auto updated =
                    placementService.patchPlacementBlock(selected.id, worldPos, placement::PatchOp::clearOverride());
                if (!updated) {
                    replyPlacementError(output, "command.patchPlacementBlock", updated.error());
                    return;
                }

                flushPlacementRefreshAndReply(*session, origin, output, [&](CommandOutput& out) {
                    out.success(tr("cmd.placement.cleared_override", worldPos.toString(), selected.name));
                });
            });
        }
    );

    schemCmd.overload().text("info").execute([&](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
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

    schemCmd.overload().text("clear").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto& placementService = session->placement();
        auto  count            = placementService.managedPlacements().size();
        session->loads().cancelAll();
        placementService.clearPlacements();
        flushPlacementRefreshAndReply(*session, origin, output, [&](CommandOutput& out) {
            out.success(tr("cmd.placement.cleared", count));
        });
    });

    schemCmd.overload().execute([&](CommandOrigin const&, CommandOutput& output) { output.success(tr("cmd.help")); });
}

} // namespace levischematic::command
