#include "VerifierTypes.h"


#include "mc/world/level/block/VanillaStates.h"
#include "mc/world/level/block/states/BlockState.h"
#include "mc/world/level/block/states/BuiltInBlockStates.h"

#include <algorithm>

namespace levischematic::verifier {

std::optional<Direction::Type> readBedDirection(Block const& block) {
    auto direction = block.getState<int>(VanillaStates::Direction());
    if (!direction) direction = block.getState<int>(BuiltInBlockStates::CardinalDirection());
    if (!direction || *direction < 0 || *direction >= static_cast<int>(Direction::Type::Count)) return {};
    return static_cast<Direction::Type>(*direction);
}

BlockCompareSpec buildCompareSpecFromBlock(Block const& block) {
    BlockCompareSpec spec;
    spec.nameHash = block.getBlockType().mNameInfo->mFullName->getHash();
    bool bed      = block.getBlockType().getBlockEntityType() == BlockActorType::Bed;
    if (bed) {
        spec.bedDirection = readBedDirection(block);
        spec.complete     = spec.bedDirection.has_value();
    }
    block.mBlockType->forEachBlockStateInstance(


        [&](BlockStateInstance const& stateInstance) {
            auto state = stateInstance.mState;
            if (bed
                && (state->mID == VanillaStates::Direction().mID
                    || state->mID == BuiltInBlockStates::CardinalDirection().mID
                    || state->mID == VanillaStates::OccupiedBit().mID))
                return true;
            auto value = block.getState<int>(state->mID);
            if (!value) {
                spec.complete = false;
                return true;
            }
            spec.exactStates.push_back(
                BlockStateSnapshot{
                    .stateId  = state->mID,
                    .value    = *value,
                    .nameHash = state->mName->getHash(),
                    .name     = state->mName->getString(),
                }
            );
            return true;
        }
    );
    std::sort(
        spec.exactStates.begin(),
        spec.exactStates.end(),


        [](BlockStateSnapshot const& lhs, BlockStateSnapshot const& rhs) { return lhs.stateId < rhs.stateId; }
    );

    return spec;
}

} 
