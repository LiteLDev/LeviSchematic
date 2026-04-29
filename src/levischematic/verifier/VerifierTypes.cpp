#include "VerifierTypes.h"

#include "levischematic/LeviSchematic.h"

#include "mc/world/level/block/states/BlockState.h"

#include <algorithm>

namespace levischematic::verifier {

    auto& getLogger() {
    return levischematic::LeviSchematic::getInstance().getSelf().getLogger();
}

BlockCompareSpec buildCompareSpecFromBlock(Block const& block) {
    BlockCompareSpec spec;
    spec.nameHash = block.getBlockType().mNameInfo->mFullName->getHash();
    block.forEachState([&](BlockState const& state, int value) {
        spec.exactStates.push_back(BlockStateSnapshot{
            .stateId  = state.mID,
            .value    = *block.getState<int>(state.mID),
            .nameHash = state.mName->getHash(),
            .name     = state.mName->getString(),
        });
        return true;
    });
    std::sort(
        spec.exactStates.begin(),
        spec.exactStates.end(),
        [](BlockStateSnapshot const& lhs, BlockStateSnapshot const& rhs) {
            return lhs.stateId < rhs.stateId;
        }
    );

    spec.compareContainer = block.getBlockType().isContainerBlock();
    return spec;
}

} // namespace levischematic::verifier
