#include "VerifierTypes.h"

#include "mc/world/level/block/states/BlockState.h"

#include <algorithm>

namespace levishematic::verifier {

BlockCompareSpec buildCompareSpecFromBlock(Block const& block) {
    BlockCompareSpec spec;
    spec.nameHash = block.getBlockType().mNameInfo->mFullName->getHash();
    block.forEachState([&](BlockState const& state, int value) {
        spec.exactStates.push_back(BlockStateSnapshot{
            .stateId  = state.mID,
            .value    = value,
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

} // namespace levishematic::verifier
