#pragma once

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace levishematic::verifier {

struct BlockStateSnapshot {
    uint64_t    stateId = 0;
    int         value   = 0;
    uint64_t    nameHash = 0;
    std::string name;
};

struct ContainerSlotSnapshot {
    int      slot          = 0;
    uint64_t itemNameHash  = 0;
    int      count         = 0;
};

struct ContainerSnapshot {
    std::vector<ContainerSlotSnapshot> slots;
};

struct BlockEntitySnapshot {
    std::optional<ContainerSnapshot> container;
};

struct BlockCompareSpec {
    uint64_t                        nameHash          = 0;
    std::vector<BlockStateSnapshot> exactStates;
    bool                            compareContainer = false;
};

struct ExpectedBlockSnapshot {
    BlockPos                         pos;
    const Block*                     renderBlock = nullptr;
    BlockCompareSpec                 compareSpec;
    std::optional<BlockEntitySnapshot> blockEntity;
    uint32_t                         placementId = 0;
};

enum class VerificationStatus : uint8_t {
    Unknown,
    Matched,
    PropertyMismatch,
    BlockMismatch,
};

[[nodiscard]] BlockCompareSpec buildCompareSpecFromBlock(Block const& block);

struct VerifierState {
    std::unordered_map<uint64_t, VerificationStatus> statusByPos;
    uint64_t                                         revision = 0;
};

inline constexpr bool isHiddenStatus(VerificationStatus status) {
    return status == VerificationStatus::Matched;
}

} // namespace levishematic::verifier
