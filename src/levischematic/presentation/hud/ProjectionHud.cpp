#include "levischematic/presentation/hud/ProjectionHud.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/selection/model/Box.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"

namespace levischematic::app {
namespace {
ui::HudPosition position(BlockPos const& pos) { return {pos.x, pos.y, pos.z}; }
char const*     layerKey(editor::LayerRange const& range) {
    if (!range.enabled) return "hud.layers.all";
    switch (range.mode) {
    case editor::LayerMode::Single:
        return "hud.layers.single";
    case editor::LayerMode::Range:
        return "hud.layers.range";
    case editor::LayerMode::Below:
        return "hud.layers.below";
    case editor::LayerMode::Above:
        return "hud.layers.above";
    default:
        return "hud.layers.all";
    }
}
} // namespace

std::string toolNameKey(editor::ToolMode mode) {
    static constexpr char const* keys[] = {
        "tool.none",
        "tool.placement",
        "tool.selection",
        "tool.edit",
        "tool.paste",
        "tool.grid_paste",
        "tool.fill",
        "tool.replace",
        "tool.delete",
        "tool.clone",
        "tool.move"
    };
    auto index = static_cast<size_t>(mode);
    return index < std::size(keys) ? keys[index] : "tool.none";
}

std::optional<ui::ProjectionHudSnapshot> projectionHudSnapshot(AppKernel& kernel) {
    auto lock = kernel.placement().tryLockPlacements();
    if (!lock.owns_lock()) return {};
    ui::ProjectionHudSnapshot result;
    auto const&               tool = kernel.tools().state();
    auto                      mode = tool.activeTool;
    result.toolKey                 = toolNameKey(mode);
    result.toolEnabled             = tool.enabled;
    result.editTool                = mode == editor::ToolMode::Edit;
    result.deleteTool              = mode == editor::ToolMode::Delete;
    auto client                    = ll::service::getClientInstance();
    bool creative                  = client && client->getLocalPlayer() && client->getLocalPlayer()->isCreative();
    std::vector<editor::ToolMode> cycle{
        editor::ToolMode::Selection,
        editor::ToolMode::Placement,
        editor::ToolMode::Edit
    };
    if (creative || editor::creativeTool(mode))
        for (int i = static_cast<int>(editor::ToolMode::Paste); i <= static_cast<int>(editor::ToolMode::Move); ++i)
            cycle.push_back(static_cast<editor::ToolMode>(i));
    auto found       = std::find(cycle.begin(), cycle.end(), mode);
    result.toolIndex = found == cycle.end() ? 0 : static_cast<int>(found - cycle.begin()) + 1;
    result.toolCount = static_cast<int>(cycle.size());

    if (editor::areaTool(mode)) {
        result.layout          = ui::HudToolLayout::Selection;
        auto const& state      = kernel.selection().state();
        auto const  overlay    = kernel.selection().overlay();
        result.normalSelection = state.normal;
        result.selectionName   = state.name;
        result.expandMode      = state.cornerMode == selection::tool::CornerMode::Expand;
        result.customOrigin    = state.customOrigin.has_value();
        std::optional<BlockPos> min;
        auto                    include = [&](std::optional<BlockPos> a, std::optional<BlockPos> b) {
            if (!a || !b) return;
            auto low = selection::Box{*a, *b}.getMin();
            min      = min ? BlockPos{std::min(min->x, low.x), std::min(min->y, low.y), std::min(min->z, low.z)} : low;
        };
        if (state.normal) {
            result.regionCount = state.regions.size();
            for (auto const& region : state.regions) {
                include(region.corner1, region.corner2);
                if (region.id == state.selectedRegion) result.subRegionName = region.name;
            }
        } else {
            result.regionCount   = state.corner1 || state.corner2 ? 1 : 0;
            result.subRegionName = state.name;
            include(state.corner1, state.corner2);
        }
        if (state.customOrigin) result.selectionOrigin = position(*state.customOrigin);
        else if (min) result.selectionOrigin = position(*min);
        if (overlay.corner1) result.corner1 = position(*overlay.corner1);
        if (overlay.corner2) result.corner2 = position(*overlay.corner2);
    } else {
        result.layout = mode == editor::ToolMode::Paste ? ui::HudToolLayout::Paste : ui::HudToolLayout::Placement;
        if (auto selected = kernel.placement().requireSelectedPlacement()) {
            auto const& item = *selected.value();
            if (!item.name.empty()) result.placementName = item.name;
            else {
                auto name = item.filePath.stem().u8string();
                result.placementName.emplace(reinterpret_cast<char const*>(name.data()), name.size());
            }
            result.placementOrigin  = position(item.origin);
            result.placementRegions = item.asset ? std::max<size_t>(1, item.asset->regions.size()) : 0;
            for (auto const& [id, region] : item.regions)
                if (!region.enabled || region.offset != BlockPos{0, 0, 0} || region.rotation || region.mirror)
                    result.regionsModified = true;
        }
        auto const& options  = kernel.worldEdit().options();
        result.replacePolicy = static_cast<int>(options.policy);
        result.layersKey     = options.layerOnly ? layerKey(kernel.view().currentLayerRange()) : "hud.layers.all";
    }
    return result;
}
} // namespace levischematic::app
