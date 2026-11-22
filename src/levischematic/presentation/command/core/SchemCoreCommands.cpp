#include "levischematic/presentation/command/shared/CommandShared.h"
#include "levischematic/presentation/ui/model/Ui.h"
#include "levischematic/shared/i18n/I18n.h"
#include "mc/world/level/dimension/Dimension.h"

namespace levischematic::command {
struct LayerModeParam {
    enum class Mode { all, single, range, below, above } value;
};
struct LayerAxisParam {
    enum class Axis { x, y, z } value;
};
void registerSchemCoreCommands(ll::command::CommandHandle& command) {
    for (auto [name, page] : {
             std::pair{"save",      ui::PageId::SaveSelection},
             {"layers",    ui::PageId::Layers       },
             {"verifier",  ui::PageId::Verifier     },
             {"session",   ui::PageId::Session      },
             {"resources", ui::PageId::Resources    },
             {"edit",      ui::PageId::Edit         }
    })
        command.overload().text("ui").text(name).execute([page](CommandOrigin const&, CommandOutput& output) {
            auto s = acquireSession(output);
            if (!s) return;
            ui::requestOpen(page);
            output.success(tr("cmd.core.requested_ui"));
        });
    command.overload<LayerModeParam>().text("layer").text("mode").required("value").execute(
        [](CommandOrigin const& origin, CommandOutput& output, LayerModeParam const& parameter) {
            auto s = acquireSession(output);
            if (!s) return;
            auto range  = s->view().currentLayerRange();
            range.mode  = static_cast<editor::LayerMode>(parameter.value);
            auto result = s->view().configure(range);
            if (!result) {
                output.error(result.error().describe());
                return;
            }
            refreshProjectionStateForOrigin(*s, origin);
            output.success(tr("cmd.core.layer_mode_updated"));
        }
    );
    command.overload<LayerAxisParam>().text("layer").text("axis").required("value").execute(
        [](CommandOrigin const& origin, CommandOutput& output, LayerAxisParam const& parameter) {
            auto s = acquireSession(output);
            if (!s) return;
            auto range  = s->view().currentLayerRange();
            range.axis  = static_cast<editor::LayerAxis>(parameter.value);
            auto result = s->view().configure(range);
            if (!result) {
                output.error(result.error().describe());
                return;
            }
            refreshProjectionStateForOrigin(*s, origin);
            output.success(tr("cmd.core.layer_axis_updated"));
        }
    );
    command.overload<SchemViewYValueParam>()
        .text("layer")
        .text("boundary")
        .required("value")
        .execute([](CommandOrigin const& origin, CommandOutput& output, SchemViewYValueParam const& parameter) {
            auto s = acquireSession(output);
            if (!s) return;
            auto range       = s->view().currentLayerRange();
            range.boundary() = parameter.value;
            auto result      = s->view().configure(range);
            if (!result) {
                output.error(result.error().describe());
                return;
            }
            refreshProjectionStateForOrigin(*s, origin);
            output.success(tr("cmd.core.layer_boundary_updated"));
        });
    command.overload().text("layer").text("info").execute([](CommandOrigin const&, CommandOutput& output) {
        auto s = acquireSession(output);
        if (!s) return;
        auto r = s->view().currentLayerRange();
        output.success(
            tr("cmd.core.layers_info",
               "XYZ"[static_cast<int>(r.axis)],
               static_cast<int>(r.mode),
               r.boundary(),
               r.minY,
               r.maxY,
               r.followPlayer)
        );
    });
    for (auto [name, layers] : {
             std::pair{"start",  false},
             {"layers", true }
    })
        command.overload().text("verify").text(name).execute(
            [layers](CommandOrigin const& origin, CommandOutput& output) {
                auto s = acquireSession(output);
                if (!s) return;
                auto selected  = s->placement().requireSelectedPlacement();
                auto dimension = origin.getDimension();
                if (!selected || !dimension
                    || selected.value()->dimensionId != static_cast<int>(dimension->getDimensionId())) {
                    output.error(tr("cmd.core.select_placement_in_dimension"));
                    return;
                }
                if (!s->verifier().startTask(selected.value()->id, layers))
                    output.error(tr("cmd.core.placement_disabled"));
                else output.success(tr("cmd.core.verification_started", selected.value()->id));
            }
        );
    for (auto operation : {"stop", "reset", "info"})
        command.overload().text("verify").text(operation).execute(
            [operation](CommandOrigin const& origin, CommandOutput& output) {
                auto s = acquireSession(output);
                if (!s) return;
                auto id = s->placement().selectedPlacementId();
                if (!id) {
                    output.error(tr("cmd.core.no_placement_selected"));
                    return;
                }
                if (std::string_view(operation) == "stop") s->verifier().stopTask(*id);
                if (std::string_view(operation) == "reset") s->verifier().resetTask(*id);
                auto r = s->verifier().report(*id, BlockPos(origin.getWorldPosition()), 0);
                output.success(
                    tr("cmd.core.verifier_info",
                       *id,
                       r.running,
                       r.building,
                       r.stale,
                       r.scanned,
                       r.total,
                       r.counts[0],
                       r.counts[1],
                       r.counts[2],
                       r.counts[3],
                       r.counts[4],
                       r.counts[5],
                       r.ignored)
                );
            }
        );
}
} // namespace levischematic::command
