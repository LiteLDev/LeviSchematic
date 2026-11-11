#include "CommandShared.h"
#include "levischematic/ui/Ui.h"
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
             std::pair{"save",     ui::PageId::SaveSelection},
             {"layers",   ui::PageId::Layers       },
             {"verifier", ui::PageId::Verifier     },
             {"session",  ui::PageId::Session      }
             ,{"resources",ui::PageId::Resources},{"edit",ui::PageId::Edit}
    })
        command.overload().text("ui").text(name).execute([page](CommandOrigin const&, CommandOutput& output) {
            auto s = acquireSession(output);
            if (!s) return;
            ui::requestOpen(page);
            output.success("Requested core workflow UI.");
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
            output.success("Layer mode updated.");
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
            output.success("Layer axis updated.");
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
            output.success("Layer boundary updated.");
        });
    command.overload().text("layer").text("info").execute([](CommandOrigin const&, CommandOutput& output) {
        auto s = acquireSession(output);
        if (!s) return;
        auto r = s->view().currentLayerRange();
        output.success(
            "Layers: axis={} mode={} boundary={} range={}..{} follow={}",
            "XYZ"[static_cast<int>(r.axis)],
            static_cast<int>(r.mode),
            r.boundary(),
            r.minY,
            r.maxY,
            r.followPlayer
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
                    output.error("Select a placement in the current dimension.");
                    return;
                }
                if (!s->verifier().startTask(selected.value()->id, layers)) output.error("Placement is disabled.");
                else output.success("Verification started for placement {}.", selected.value()->id);
            }
        );
    for (auto operation : {"stop", "reset", "info"})
        command.overload().text("verify").text(operation).execute(
            [operation](CommandOrigin const& origin, CommandOutput& output) {
                auto s = acquireSession(output);
                if (!s) return;
                auto id = s->placement().selectedPlacementId();
                if (!id) {
                    output.error("No placement selected.");
                    return;
                }
                if (std::string_view(operation) == "stop") s->verifier().stopTask(*id);
                if (std::string_view(operation) == "reset") s->verifier().resetTask(*id);
                auto r = s->verifier().report(*id, BlockPos(origin.getWorldPosition()), 0);
                output.success(
                    "Verifier #{}: running={} building={} stale={} scanned={}/{} Unknown={} Matched={} Missing={} "
                    "WrongState={} WrongBlock={} Extra={} Ignored={}",
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
                    r.ignored
                );
            }
        );
}
} 
