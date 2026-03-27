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
            auto& controller = app::getAppKernel().controller();
            withSelectedPlacement(
                controller,
                output,
                [&](placement::PlacementInstance const& selected) {
                    controller.movePlacement(selected.id, param.dx, param.dy, param.dz);
                    auto const* moved = controller.findPlacement(selected.id);
                    flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                        out.success(
                            "Moved '{}' by ({},{},{}) -> ({})",
                            selected.name,
                            param.dx,
                            param.dy,
                            param.dz,
                            moved ? moved->origin.toString() : selected.origin.toString()
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
            auto& controller = app::getAppKernel().controller();
            withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
                auto blockPos = BlockPos(origin.getExecutePosition(cmd.mVersion, param.pos));
                controller.setPlacementOrigin(selected.id, blockPos);
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
            auto& controller = app::getAppKernel().controller();
            withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
                switch (param.dir) {
                case SchemRotateParam::direction::cw90:
                    controller.rotatePlacement(selected.id, placement::PlacementInstance::Rotation::CW_90);
                    break;
                case SchemRotateParam::direction::ccw90:
                    controller.rotatePlacement(selected.id, placement::PlacementInstance::Rotation::CCW_90);
                    break;
                case SchemRotateParam::direction::r180:
                    controller.rotatePlacement(selected.id, placement::PlacementInstance::Rotation::CW_180);
                    break;
                }

                auto const* updated = controller.findPlacement(selected.id);
                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success(
                        "Rotated '{}' -> {}",
                        selected.name,
                        updated ? updated->describeTransform() : selected.describeTransform()
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
            auto& controller = app::getAppKernel().controller();
            withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
                switch (param.axis_) {
                case SchemMirrorParam::axis::x:
                    controller.setPlacementMirror(selected.id, placement::PlacementInstance::Mirror::MIRROR_X);
                    break;
                case SchemMirrorParam::axis::z:
                    controller.setPlacementMirror(selected.id, placement::PlacementInstance::Mirror::MIRROR_Z);
                    break;
                case SchemMirrorParam::axis::none:
                    controller.setPlacementMirror(selected.id, placement::PlacementInstance::Mirror::NONE);
                    break;
                }

                auto const* updated = controller.findPlacement(selected.id);
                flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                    out.success(
                        "Mirror '{}' -> {}",
                        selected.name,
                        updated ? updated->describeTransform() : selected.describeTransform()
                    );
                });
            });
        });

    schemCmd.overload().text("reset").execute([&](CommandOrigin const& origin, CommandOutput& output) {
        auto& controller = app::getAppKernel().controller();
        withSelectedPlacement(controller, output, [&](placement::PlacementInstance const& selected) {
            controller.resetPlacementTransform(selected.id);
            auto const* updated = controller.findPlacement(selected.id);
            flushPlacementRefreshAndReply(origin, output, [&](CommandOutput& out) {
                out.success(
                    "Reset transform for '{}' -> {}",
                    selected.name,
                    updated ? updated->describeTransform() : selected.describeTransform()
                );
            });
        });
    });
}

} // namespace levishematic::command
