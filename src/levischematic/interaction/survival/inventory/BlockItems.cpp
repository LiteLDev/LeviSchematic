#include "levischematic/interaction/survival/inventory/BlockItems.h"
#include "levischematic/shared/i18n/I18n.h"
#include "mc/deps/nbt/ByteTag.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/IntTag.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/registry/ItemRegistryManager.h"
#include "mc/world/item/registry/ItemRegistryRef.h"
#include "mc/world/level/levelgen/structure/StructureBlockPalette.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/level/levelgen/structure/StructureTemplateData.h"

namespace levischematic::survival {
ItemKey itemKey(ItemStackBase const& item) {
    return {item.getTypeName(), item.mItem && item.mItem->isStackedByData() ? item.getAuxValue() : 0};
}
namespace {
std::map<std::string, int> blockStates(Block const& block) {
    std::map<std::string, int> states;
    for (auto const& state : verifier::buildCompareSpecFromBlock(block).exactStates) states[state.name] = state.value;
    return states;
}
bool makeItem(ItemStack& stack, std::string const& name) {
    auto item = ItemRegistryManager::getItemRegistry().lookupByNameNoAlias(std::string_view(name));
    if (!item) return false;
    stack.reinit(*item, 1, 0);
    return static_cast<bool>(stack);
}
} // namespace
std::optional<ItemRequirement> secondLayerItem(Block const& block) {
    if (block.isAir()) return {};
    auto name   = block.getBlockType().mNameInfo->mFullName->getString();
    auto states = blockStates(block);
    if (name != "minecraft:water" && name != "minecraft:flowing_water" && name != "minecraft:lava"
        && name != "minecraft:flowing_lava" && name != "minecraft:bubble_column")
        return {};
    auto      substitute = substituteItem(name, states);
    ItemStack stack;
    if (!substitute || substitute->empty() || !makeItem(stack, *substitute)) return {};
    return ItemRequirement{itemKey(stack), stack.getDescriptionName(), 1};
}
BlockItems resolveBlockItems(placement::PlacementInstance const& p, placement::ExpectedPosition const& expected) {
    BlockItems result;
    if (!expected.block) {
        result.unsupported = tr("msg.survival.unresolved_palette");
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
    if (auto substitute = substituteItem(name, states)) {
        if (substitute->empty()) return result;
        if (!makeItem(result.pick, *substitute)) {
            result.unsupported = tr("msg.survival.no_build_item") + *substitute;
            return result;
        }
        result.materials.push_back({itemKey(result.pick), result.pick.getDescriptionName(), 1});
        return result;
    }
    auto item = block.getBlockType().asItemInstance(block, nullptr);
    if (!item || !item.mItem) {
        result.unsupported = tr("msg.survival.no_build_item") + name;
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
            result.unsupported = tr("msg.survival.bed_color_nbt_unavailable");
            return result;
        }
    }
    result.pick.reinit(*item.mItem, 1, aux);
    if (!result.pick) {
        result.unsupported = tr("msg.survival.item_conversion_failed");
        return result;
    }
    result.materials.push_back({itemKey(result.pick), result.pick.getDescriptionName(), result.rule.count});
    if (name.find("flower_pot") != std::string::npos || name.find("cauldron") != std::string::npos) {
        result.unsupported = tr("msg.survival.container_contents_require_manual");
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
} // namespace levischematic::survival
