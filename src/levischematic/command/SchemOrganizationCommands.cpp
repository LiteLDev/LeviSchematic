#include "CommandShared.h"
#include "levischematic/render/ActorProjection.h"
#include "levischematic/schematic/placement/ProjectionQuery.h"
#include "mc/server/commands/Command.h"
#include "mc/server/commands/CommandBlockNameResult.h"

namespace levischematic::command {
struct OrganizationFlag {
    bool enabled;
};
struct OrganizationRegion {
    int id;
};
struct OrganizationGrid {
    enum class Axis { x, y, z } axis;
    int spacing, negative, positive;
};
void registerSchemOrganizationCommands(ll::command::CommandHandle& cmd) {
    cmd.overload().text("resource").text("status").execute([](CommandOrigin const&, CommandOutput& out) {
        auto s = acquireSession(out);
        if (!s) return;
        std::string message;
        for (auto const& r : s->placement().resources().list())
            message += std::to_string(r.id) + " " + r.path.string() + (r.content->modified ? " *" : "") + "\n";
        auto progress  = s->placement().resources().progress();
        message       += progress.detail + " scanned=" + std::to_string(progress.scanned)
                       + " changed=" + std::to_string(progress.changed);
        auto actors    = s->actors().stats();
        message += "\nActors loaded=" + std::to_string(actors.loaded) + " skipped=" + std::to_string(actors.skipped)
                 + " submitted=" + std::to_string(actors.submitted)
                 + " highlighted=" + std::to_string(actors.highlighted) + " " + actors.detail;
        out.success(message);
    });
    for (auto [name, discard] : {
             std::pair{"save",           false},
             {"reload-discard", true }
    })
        cmd.overload().text("resource").text(name).execute([discard](CommandOrigin const&, CommandOutput& out) {
            auto s = acquireSession(out);
            if (!s) return;
            (void)s->placement().resources().list();
            auto p = s->placement().requireSelectedPlacement();
            if (!p) {
                out.error(p.error().describe());
                return;
            }
            auto id = p.value()->asset->resourceId;
            auto result =
                discard ? s->placement().resources().reload(id, true) : s->placement().resources().save(id, {}, true);
            if (result) out.success(discard ? "Reload queued." : "Resource saved.");
            else out.error(result.error());
        });
    cmd.overload().text("resource").text("cancel").execute([](CommandOrigin const&, CommandOutput& out) {
        auto s = acquireSession(out);
        if (!s) return;
        s->placement().resources().cancel();
        out.success("Content operation cancelled.");
    });
    cmd.overload<SchemNamedParam>()
        .text("resource")
        .text("saveas")
        .required("filename")
        .execute([](CommandOrigin const&, CommandOutput& out, SchemNamedParam const& p) {
            auto s = acquireSession(out);
            if (!s) return;
            auto id = s->placement().selectedPlacementId();
            if (!id) {
                out.error("Select a placement first.");
                return;
            }
            auto result = s->placement().resources().saveAs(*id, s->runtime().resolveSchematicPath(p.filename), false);
            if (result) out.success("Saved a new resource; selected placement now uses it.");
            else out.error(result.error());
        });
    cmd.overload<SchemBlockSetSimpleParam>()
        .text("edit")
        .text("set")
        .required("pos")
        .required("blockName")
        .execute([](CommandOrigin const&            origin,
                    CommandOutput&                  out,
                    SchemBlockSetSimpleParam const& p,
                    Command const&                  command) {
            auto s = acquireSession(out);
            if (!s) return;
            auto selected = s->placement().requireSelectedPlacement();
            if (!selected) {
                out.error(selected.error().describe());
                return;
            }
            auto pos   = BlockPos(origin.getExecutePosition(command.mVersion, p.pos));
            auto block = p.blockName.resolveBlock(0);
            if (!block.mBlock) {
                out.error("Unknown block.");
                return;
            }
            auto hit  = placement::projectionAt(s->placement().orderedPlacements(), selected.value()->dimensionId, pos);
            auto leaf = hit ? s->placement().findPlacement(hit->owner) : nullptr;
            if (!leaf || (leaf->parentId ? leaf->parentId : leaf->id) != selected.value()->id) {
                out.error("Position is outside selected placement.");
                return;
            }
            auto result = s->placement().resources().edit({leaf->id, pos, block.mBlock}, s->view().currentLayerRange());
            if (result) out.success("Shared resource edit queued.");
            else out.error(result.error());
        });
    cmd.overload<OrganizationFlag>().text("grid").text("enabled").required("enabled").execute(
        [](CommandOrigin const&, CommandOutput& out, OrganizationFlag const& p) {
            auto s = acquireSession(out);
            if (!s) return;
            auto selected = s->placement().requireSelectedPlacement();
            if (!selected) {
                out.error(selected.error().describe());
                return;
            }
            auto grid    = selected.value()->grid;
            grid.enabled = p.enabled;
            auto result  = s->placement().setPlacementGrid(selected.value()->id, grid);
            if (result) out.success("Grid updated.");
            else out.error(result.error().describe());
        }
    );
    cmd.overload<OrganizationGrid>()
        .text("grid")
        .required("axis")
        .required("spacing")
        .required("negative")
        .required("positive")
        .execute([](CommandOrigin const&, CommandOutput& out, OrganizationGrid const& p) {
            auto s = acquireSession(out);
            if (!s) return;
            auto selected = s->placement().requireSelectedPlacement();
            if (!selected) {
                out.error(selected.error().describe());
                return;
            }
            auto grid           = selected.value()->grid;
            auto axis           = static_cast<int>(p.axis);
            grid.spacing[axis]  = p.spacing;
            grid.negative[axis] = p.negative;
            grid.positive[axis] = p.positive;
            auto result         = s->placement().setPlacementGrid(selected.value()->id, grid);
            if (result) out.success("Grid updated.");
            else out.error(result.error().describe());
        });
    cmd.overload<OrganizationFlag>()
        .text("selection")
        .text("normal")
        .required("enabled")
        .execute([](CommandOrigin const&, CommandOutput& out, OrganizationFlag const& p) {
            auto s = acquireSession(out);
            if (!s) return;
            s->selection().setNormal(p.enabled);
            out.success("Selection mode updated.");
        });
    cmd.overload<SchemNamedParam>()
        .text("selection")
        .text("add")
        .required("filename")
        .execute([](CommandOrigin const&, CommandOutput& out, SchemNamedParam const& p) {
            auto s = acquireSession(out);
            if (!s) return;
            if (s->selection().addRegion(p.filename)) out.success("Region added.");
            else out.error("Empty or duplicate region name.");
        });
    cmd.overload<OrganizationRegion>()
        .text("selection")
        .text("region")
        .required("id")
        .execute([](CommandOrigin const&, CommandOutput& out, OrganizationRegion const& p) {
            auto s = acquireSession(out);
            if (!s) return;
            if (p.id > 0 && s->selection().selectRegion(static_cast<uint32_t>(p.id))) out.success("Region selected.");
            else out.error("Region not found.");
        });
}
} 
