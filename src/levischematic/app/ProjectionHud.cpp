#include "ProjectionHud.h"
#include "AppKernel.h"
#include "levischematic/render/SchematicRenderAlgorithms.h"

namespace levischematic::app {
std::optional<ui::ProjectionHudSnapshot> projectionHudSnapshot(AppKernel& kernel) {
    auto lock = kernel.placement().tryLockPlacements();
    if (!lock.owns_lock()) return {};
    ui::ProjectionHudSnapshot result;
    auto const                selection = kernel.selection().overlay();
    result.selectionMode                = selection.selectionMode;
    auto const& tool                    = kernel.tools().state();
    result.toolDetail                   = tool.enabled ? " [ON]" : " [OFF]";
    if (tool.activeTool==editor::ToolMode::Edit) result.toolDetail+=" Edit Schematic";
    if (editor::creativeTool(tool.activeTool)) {
        constexpr char const* names[]{"Paste", "Grid Paste", "Fill", "Replace", "Delete", "Clone", "Move"};
        result.toolDetail +=
            std::string(" ") + names[static_cast<int>(tool.activeTool) - static_cast<int>(editor::ToolMode::Paste)];
        auto p             = kernel.worldEdit().progress();
        result.toolDetail += std::string(" / ") + worldedit::phaseName(p.phase) + " " + std::to_string(p.observed) + "/"
                           + std::to_string(p.total);
    }
    if (selection.selectionMode) {
        auto const& state  = kernel.selection().state();
        result.toolDetail += state.cornerMode == selection::tool::CornerMode::Corners ? " Corners" : " Expand";
        switch (state.selectedElement) {
        case selection::tool::Element::None:
            result.toolDetail += " / None";
            break;
        case selection::tool::Element::Corner1:
            result.toolDetail += " / Corner 1";
            break;
        case selection::tool::Element::Corner2:
            result.toolDetail += " / Corner 2";
            break;
        case selection::tool::Element::Box:
            result.toolDetail += " / Box";
            break;
        }
    }
    auto position                       = [](BlockPos const& pos) { return ui::HudPosition{pos.x, pos.y, pos.z}; };
    if (selection.corner1) result.corner1 = position(*selection.corner1);
    if (selection.corner2) result.corner2 = position(*selection.corner2);
    auto selected = kernel.placement().requireSelectedPlacement();
    if (selected) {
        auto const& item = *selected.value();
        auto        name = item.filePath.filename().u8string();
        result.fileName.assign(reinterpret_cast<char const*>(name.data()), name.size());
        result.origin   = position(item.origin);
        result.rotation = static_cast<int>(item.rotation);
        result.mirror   = static_cast<int>(item.mirror);
        if (item.asset) {
            auto const& size    = item.asset->size;
            auto        rotated = render::detail::rotatedStructureSize(
                {size.x, size.y, size.z},
                result.rotation == 1 || result.rotation == 3
            );
            result.size = ui::HudPosition{rotated[0], rotated[1], rotated[2]};
        }
    }
    return result;
}
} 
