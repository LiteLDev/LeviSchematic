#include "levishematic/command/CommandShared.h"

#include "levishematic/schematic/placement/PlacementModel.h"

#include "mc/server/commands/Command.h"
#include "mc/world/level/BlockPos.h"

namespace levishematic::command {

void registerSchemTransformCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload<SchemMoveParam>()
        .text("move")
        .required("dx")
        .required("dy")
        .required("dz")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemMoveParam const& param) {
            auto& placementService = app::getAppKernel().placement();
            withSelectedPlacement(
                placementService,
                output,
                [&](placement::PlacementInstance const& selected) {
                    auto moved = placementService.movePlacement(selected.id, param.dx, param.dy, param.dz);
                    if (!moved) {
                        replyPlacementError(output, "command.movePlacement", moved.error());
                        return;
                    }

                    flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                        out.success(
                            "Moved '{}' by ({},{},{}) -> ({})",
                            selected.name,
                            param.dx,
                            param.dy,
                            param.dz,
                            moved.value() ? moved.value()->origin.toString() : selected.origin.toString()
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
            auto& placementService = app::getAppKernel().placement();
            withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
                auto blockPos = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                auto updated  = placementService.setPlacementOrigin(selected.id, blockPos);
                if (!updated) {
                    replyPlacementError(output, "command.setPlacementOrigin", updated.error());
                    return;
                }

                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success("Set origin of '{}' to ({})", selected.name, blockPos.toString());
                });
            });
        });

    schemCmd.overload<SchemRotateParam>()
        .text("rotate")
        .required("dir")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemRotateParam const& param) {
            auto& placementService = app::getAppKernel().placement();
            withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
                auto updated = [&]() {
                    switch (param.dir) {
                    case SchemRotateParam::direction::cw90:
                        return placementService.rotatePlacement(selected.id, placement::PlacementInstance::Rotation::CW_90);
                    case SchemRotateParam::direction::ccw90:
                        return placementService.rotatePlacement(selected.id, placement::PlacementInstance::Rotation::CCW_90);
                    case SchemRotateParam::direction::r180:
                        return placementService.rotatePlacement(selected.id, placement::PlacementInstance::Rotation::CW_180);
                    }

                    return placementService.rotatePlacement(selected.id, placement::PlacementInstance::Rotation::CW_90);
                }();

                if (!updated) {
                    replyPlacementError(output, "command.rotatePlacement", updated.error());
                    return;
                }

                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success(
                        "Rotated '{}' -> {}",
                        selected.name,
                        updated.value() ? updated.value()->describeTransform() : selected.describeTransform()
                    );
                });
            });
        });

    schemCmd.overload<SchemMirrorParam>()
        .text("mirror")
        .required("axis_")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemMirrorParam const& param) {
            auto& placementService = app::getAppKernel().placement();
            withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
                auto updated = [&]() {
                    switch (param.axis_) {
                    case SchemMirrorParam::axis::x:
                        return placementService.setPlacementMirror(selected.id, placement::PlacementInstance::Mirror::MIRROR_X);
                    case SchemMirrorParam::axis::z:
                        return placementService.setPlacementMirror(selected.id, placement::PlacementInstance::Mirror::MIRROR_Z);
                    case SchemMirrorParam::axis::none:
                        return placementService.setPlacementMirror(selected.id, placement::PlacementInstance::Mirror::NONE);
                    }

                    return placementService.setPlacementMirror(selected.id, placement::PlacementInstance::Mirror::NONE);
                }();

                if (!updated) {
                    replyPlacementError(output, "command.setPlacementMirror", updated.error());
                    return;
                }

                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success(
                        "Mirror '{}' -> {}",
                        selected.name,
                        updated.value() ? updated.value()->describeTransform() : selected.describeTransform()
                    );
                });
            });
        });

    schemCmd.overload().text("reset").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto& placementService = app::getAppKernel().placement();
        withSelectedPlacement(placementService, output, [&](placement::PlacementInstance const& selected) {
            auto updated = placementService.resetPlacementTransform(selected.id);
            if (!updated) {
                replyPlacementError(output, "command.resetPlacementTransform", updated.error());
                return;
            }

            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success(
                    "Reset transform for '{}' -> {}",
                    selected.name,
                    updated.value() ? updated.value()->describeTransform() : selected.describeTransform()
                );
            });
        });
    });
}

} // namespace levishematic::command
