#include "BlockItems.h"
#include "mc/deps/nbt/ByteTag.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/level/levelgen/structure/StructureBlockPalette.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/level/levelgen/structure/StructureTemplateData.h"

namespace levischematic::survival {
ItemKey itemKey(ItemStackBase const& item) {
    return {item.getTypeName(), item.mItem && item.mItem->isStackedByData() ? item.getAuxValue() : 0};
}
BlockItems resolveBlockItems(placement::PlacementInstance const& p, placement::ExpectedPosition const& expected) {
    BlockItems result;
    if (!expected.block) {
        result.unsupported = "Unresolved palette / 未解析方块";
        return result;
    }
    auto const& block = *expected.block;
    if (block.isAir()) return result;
    auto                       spec = verifier::buildCompareSpecFromBlock(block);
    std::map<std::string, int> states;
    for (auto const& state : spec.exactStates) states[state.name] = state.value;
    if (spec.bedDirection) states["direction"] = static_cast<int>(*spec.bedDirection);
    auto name   = block.getBlockType().mNameInfo->mFullName->getString();
    result.rule = buildRule(name, states);
    
    
    auto item = block.getBlockType().asItemInstance(block, nullptr);
    if (!item || !item.mItem) {
        result.unsupported = "No build item / 无对应物品: " + name;
        return result;
    }
    int aux = item.getAuxValue();
    if (block.getBlockType().getBlockEntityType() == BlockActorType::Bed) {
        
        bool found = false;
        if (p.asset && p.asset->structure && expected.sourceIndex != SIZE_MAX) {
            auto const& data     = p.asset->structure->mStructureTemplateData.get();
            auto        palette  = data.getPalette(StructureTemplateData::DEFAULT_PALETTE_NAME());
            auto        position = palette ? palette->getBlockPositionData(expected.sourceIndex) : nullptr;
            auto        tag      = position ? position->mBlockEntityData.get() : nullptr;
            if (tag && tag->contains("color")) {
                auto const& v = tag->at("color");
                if (v.hold<ByteTag>()) {
                    aux   = v.get<ByteTag>().data;
                    found = true;
                } else if (v.hold<IntTag>()) {
                    aux   = v.get<IntTag>().data;
                    found = true;
                }
            }
        }
        if (!found) {
            result.unsupported = "Bed color NBT unavailable / 床颜色数据缺失";
            return result;
        }
    }
    result.pick.reinit(*item.mItem, 1, aux);
    if (!result.pick) {
        result.unsupported = "Item conversion failed / 物品解析失败";
        return result;
    }
    result.materials.push_back({itemKey(result.pick), result.pick.getDescriptionName(), result.rule.count});
    
    if (name.find("flower_pot") != std::string::npos || name.find("cauldron") != std::string::npos) {
        result.unsupported = "Container contents require manual accounting / 容器内容需手工核对";
    }
    return result;
}
verifier::VerificationStatus compareBlocks(Block const* expected, Block const* actual) {
    using S = verifier::VerificationStatus;
    if (!expected || !actual) return S::Unknown;
    if (expected->isAir()) return actual->isAir() ? S::Matched : S::ExtraBlock;
    if (actual->isAir()) return S::MissingBlock;
    auto spec = verifier::buildCompareSpecFromBlock(*expected);
    if (spec.nameHash != actual->getBlockType().mNameInfo->mFullName->getHash()) return S::BlockMismatch;
    if (!spec.complete) return S::Unknown;
    if (spec.bedDirection && spec.bedDirection != verifier::readBedDirection(*actual)) return S::PropertyMismatch;
    for (auto const& state : spec.exactStates) {
        auto value = actual->getState<int>(state.stateId);
        if (!value) return S::Unknown;
        if (*value != state.value) return S::PropertyMismatch;
    }
    return S::Matched;
}
bool placementOrientationMatches(Block const& expected, Block const& predicted) {
    auto a = verifier::buildCompareSpecFromBlock(expected), b = verifier::buildCompareSpecFromBlock(predicted);
    if (!a.complete || !b.complete || a.nameHash != b.nameHash || a.bedDirection != b.bedDirection) return false;
    std::map<std::string, int> statesA, statesB;
    for (auto const& state : a.exactStates) statesA[state.name] = state.value;
    for (auto const& state : b.exactStates) statesB[state.name] = state.value;
    return placementOrientationMatches(statesA, statesB);
}
} 
