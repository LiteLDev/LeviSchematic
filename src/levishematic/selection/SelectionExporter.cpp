#include "SelectionExporter.h"

#include "levishematic/LeviShematic.h"
#include "levishematic/schematic/SchematicPathResolver.h"

#include "ll/api/io/FileUtils.h"
#include "ll/api/service/Bedrock.h"

#include "mc/nbt/CompoundTag.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/levelgen/structure/StructureSettings.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"

#include <algorithm>
#include <filesystem>
#include <memory>

namespace levishematic::selection {
namespace {

auto& getLogger() {
    return levishematic::LeviShematic::getInstance().getSelf().getLogger();
}

} // namespace

bool hasCompleteSelection(SelectionState const& state) {
    return state.corner1.has_value() && state.corner2.has_value();
}

std::optional<Box> getSelectionBox(SelectionState const& state) {
    if (!hasCompleteSelection(state)) {
        return std::nullopt;
    }
    return Box(*state.corner1, *state.corner2);
}

BlockPos getMinCorner(SelectionState const& state) {
    auto const& corner1 = *state.corner1;
    auto const& corner2 = *state.corner2;
    return {
        std::min(corner1.x, corner2.x),
        std::min(corner1.y, corner2.y),
        std::min(corner1.z, corner2.z),
    };
}

BlockPos getSize(SelectionState const& state) {
    auto const& corner1 = *state.corner1;
    auto const& corner2 = *state.corner2;
    return {
        std::abs(corner1.x - corner2.x) + 1,
        std::abs(corner1.y - corner2.y) + 1,
        std::abs(corner1.z - corner2.z) + 1,
    };
}

SelectionOverlayView makeOverlayView(SelectionState const& state) {
    SelectionOverlayView view;
    view.selectionMode = state.selectionMode;
    view.corner1       = state.corner1;
    view.corner2       = state.corner2;

    if (!hasCompleteSelection(state) || *state.corner1 == *state.corner2) {
        return view;
    }

    view.origin = getMinCorner(state);
    view.size   = getSize(state);
    return view;
}

bool SelectionExporter::saveToMcstructure(
    SelectionState const& state,
    std::string_view      name,
    Dimension&            dimension
) const {
    if (!hasCompleteSelection(state)) {
        getLogger().error("Cannot save selection: selection is incomplete");
        return false;
    }
    if (state.schematicDirectory.empty()) {
        getLogger().error("Cannot save selection: schematic directory is not configured");
        return false;
    }

    try {
        auto minPos = getMinCorner(state);
        auto size   = getSize(state);

        StructureSettings settings;
        settings.mStructureOffset = BlockPos::ZERO();
        settings.mStructureSize   = size;
        settings.mIgnoreEntities  = false;
        settings.mIgnoreBlocks    = false;
        settings.mAllowNonTickingPlayerAndTickingAreaChunks = false;

        auto structureTemplate = std::make_unique<StructureTemplate>(
            std::string(name),
            ll::service::getLevel()->getUnknownBlockTypeRegistry()
        );
        structureTemplate->fillFromWorld(
            dimension.getBlockSourceFromMainChunkSource(),
            minPos,
            settings
        );

        auto nbtTag = structureTemplate->save();

        std::error_code ec;
        std::filesystem::create_directories(state.schematicDirectory, ec);

        auto structurePath = schematic::SchematicPathResolver(state.schematicDirectory)
            .makeFilePath(std::filesystem::path{std::string(name)});

        ll::utils::file_utils::writeFile(structurePath, nbtTag->toBinaryNbt(), true);

        getLogger().info(
            "Saved selection to {} (size: {}x{}x{})",
            structurePath.string(),
            size.x,
            size.y,
            size.z
        );
        return true;
    } catch (std::exception const& e) {
        getLogger().error("Exception while saving selection: {}", e.what());
        return false;
    }
}

} // namespace levishematic::selection
