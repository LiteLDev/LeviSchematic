#include "levishematic/command/CommandShared.h"

#include "levishematic/core/DataManager.h"
#include "levishematic/schematic/placement/SchematicPlacement.h"

#include "mc/server/commands/Command.h"
#include "mc/world/level/BlockPos.h"

namespace levishematic::command {

void registerSchemTransformCommands(ll::command::CommandHandle& schemCmd) {
    auto& pm = core::DataManager::getInstance().getPlacementManager();

    schemCmd.overload<SchemMoveParam>()
        .text("move")
        .required("dx")
        .required("dy")
        .required("dz")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemMoveParam const& param) {
            withSelectedPlacement(
                pm,
                output,
                [&](placement::SchematicPlacement& selected) {
                    selected.move(param.dx, param.dy, param.dz);
                    auto movedTo = selected.getOrigin();
                    flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                        out.success(
                            "Moved '{}' by ({},{},{}) -> ({})",
                            selected.getName(),
                            param.dx,
                            param.dy,
                            param.dz,
                            movedTo.toString()
                        );
                    });
                },
                "No placement selected. Use /schem load or /schem select first."
            );
        });

    schemCmd.overload<SchemOriginParam>()
        .text("origin")
        .required("pos")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemOriginParam const& param,
                     Command const&      cmd) {
            withSelectedPlacement(pm, output, [&](placement::SchematicPlacement& selected) {
                auto blockPos = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                selected.setOrigin(blockPos);
                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success("Set origin of '{}' to ({})", selected.getName(), blockPos.toString());
                });
            });
        });

    schemCmd.overload<SchemRotateParam>()
        .text("rotate")
        .required("dir")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemRotateParam const& param) {
            withSelectedPlacement(pm, output, [&](placement::SchematicPlacement& selected) {
                switch (param.dir) {
                case SchemRotateParam::direction::cw90:
                    selected.rotateCW90();
                    break;
                case SchemRotateParam::direction::ccw90:
                    selected.rotateCCW90();
                    break;
                case SchemRotateParam::direction::r180:
                    selected.rotate180();
                    break;
                }

                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success("Rotated '{}' -> {}", selected.getName(), selected.describeTransform());
                });
            });
        });

    schemCmd.overload<SchemMirrorParam>()
        .text("mirror")
        .required("axis_")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemMirrorParam const& param) {
            withSelectedPlacement(pm, output, [&](placement::SchematicPlacement& selected) {
                switch (param.axis_) {
                case SchemMirrorParam::axis::x:
                    selected.setMirror(placement::SchematicPlacement::Mirror::MIRROR_X);
                    break;
                case SchemMirrorParam::axis::z:
                    selected.setMirror(placement::SchematicPlacement::Mirror::MIRROR_Z);
                    break;
                case SchemMirrorParam::axis::none:
                    selected.setMirror(placement::SchematicPlacement::Mirror::NONE);
                    break;
                }

                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success("Mirror '{}' -> {}", selected.getName(), selected.describeTransform());
                });
            });
        });

    schemCmd.overload().text("reset").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        withSelectedPlacement(pm, output, [&](placement::SchematicPlacement& selected) {
            selected.resetTransform();
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success("Reset transform for '{}' -> {}", selected.getName(), selected.describeTransform());
            });
        });
    });
}

} // namespace levishematic::command
