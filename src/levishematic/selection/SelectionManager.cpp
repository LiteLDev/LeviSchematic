// ============================================================
// SelectionManager.cpp
// 选区管理器实现（Phase 4）
// ============================================================

#include "levishematic/selection/SelectionManager.h"

#include "levishematic/LeviShematic.h"
#include "levishematic/core/DataManager.h"
#include "levishematic/schematic/SchematicPathResolver.h"

#include "ll/api/io/FileUtils.h"
#include "ll/api/service/Bedrock.h"

#include "mc/nbt/CompoundTag.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/levelgen/structure/StructureSettings.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"

#include <algorithm>
#include <filesystem>

static auto& getLogger() {
    return levishematic::LeviShematic::getInstance().getSelf().getLogger();
}

namespace levishematic::selection {

SelectionManager& SelectionManager::getInstance() {
    static SelectionManager instance;
    return instance;
}

void SelectionManager::setCorner1(const BlockPos& pos) {
    mCorner1    = pos;
    mHasCorner1 = true;
    refreshWireframeStateFromSelection();
    getLogger().info("Corner 1 set to ({}, {}, {})", pos.x, pos.y, pos.z);
}

void SelectionManager::setCorner2(const BlockPos& pos) {
    mCorner2    = pos;
    mHasCorner2 = true;
    refreshWireframeStateFromSelection();
    getLogger().info("Corner 2 set to ({}, {}, {})", pos.x, pos.y, pos.z);
}

std::optional<Box> SelectionManager::getSelectionBox() const {
    if (!hasCompleteSelection()) {
        return std::nullopt;
    }
    return Box(mCorner1, mCorner2);
}

BlockPos SelectionManager::getMinCorner() const {
    return BlockPos(
        std::min(mCorner1.x, mCorner2.x),
        std::min(mCorner1.y, mCorner2.y),
        std::min(mCorner1.z, mCorner2.z)
    );
}

BlockPos SelectionManager::getSize() const {
    return BlockPos(
        std::abs(mCorner1.x - mCorner2.x) + 1,
        std::abs(mCorner1.y - mCorner2.y) + 1,
        std::abs(mCorner1.z - mCorner2.z) + 1
    );
}

void SelectionManager::refreshWireframeStateFromSelection() {
    if (!hasCompleteSelection()) {
        mWireframeState.reset();
        return;
    }

    if (mCorner1 == mCorner2) {
        mWireframeState.reset();
        return;
    }

    mWireframeState = SelectionWireframeState{getMinCorner(), getSize()};
}

void SelectionManager::clearSelection() {
    mHasCorner1 = false;
    mHasCorner2 = false;
    mCorner1    = BlockPos(0, 0, 0);
    mCorner2    = BlockPos(0, 0, 0);
    mWireframeState.reset();
    getLogger().info("Selection cleared");
}

bool SelectionManager::saveToMcstructure(const std::string& name, Dimension& dimension) {
    if (!hasCompleteSelection()) {
        getLogger().error("Cannot save: selection is incomplete");
        return false;
    }

    try {
        BlockPos minPos = getMinCorner();
        BlockPos size   = getSize();

        StructureSettings setting;
        setting.mStructureOffset = BlockPos::ZERO();
        setting.mStructureSize   = size;
        setting.mIgnoreEntities  = false;
        setting.mIgnoreBlocks    = false;
        setting.mAllowNonTickingPlayerAndTickingAreaChunks = false;

        // 创建 StructureTemplate 并从世界中填充方块
        auto structureTemplate = std::make_unique<StructureTemplate>(
            name,
            ll::service::getLevel()->getUnknownBlockTypeRegistry()
        );
        structureTemplate->fillFromWorld(
            dimension.getBlockSourceFromMainChunkSource(),
            minPos,
            setting
        );

        // 序列化为 CompoundTag
        auto nbtTag = structureTemplate->save();

        auto& dm            = core::DataManager::getInstance();
        auto  structurePath = dm.createSchematicPathResolver().makeFilePath(std::filesystem::path{name});
        dm.ensureSchematicDirectory();

        ll::utils::file_utils::writeFile(structurePath, nbtTag->toBinaryNbt(), true);

        getLogger().info(
            "Saved selection to {} (size: {}x{}x{})",
            structurePath.string(),
            size.x,
            size.y,
            size.z
        );

        return true;
    } catch (const std::exception& e) {
        getLogger().error("Exception while saving: {}", e.what());
        return false;
    }
}

} // namespace levishematic::selection
