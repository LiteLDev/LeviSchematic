#pragma once

#include "levischematic/util/PositionUtils.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/Direction.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/actor/BlockActorRendererId.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class BlockActor;

namespace levischematic::verifier {


struct BlockStateSnapshot {
    uint64_t    stateId  = 0;
    int         value    = 0;
    uint64_t    nameHash = 0;
    std::string name;
};


struct BlockCompareSpec {
    uint64_t                        nameHash = 0;
    std::vector<BlockStateSnapshot> exactStates;
    std::optional<Direction::Type>  bedDirection;
    bool                            complete = true;
};

enum class VerificationStatus : uint8_t {
    Unknown,
    Matched,
    MissingBlock,
    PropertyMismatch,
    BlockMismatch,
};


[[nodiscard]] BlockCompareSpec               buildCompareSpecFromBlock(Block const& block);
[[nodiscard]] std::optional<Direction::Type> readBedDirection(Block const& block);


struct VerificationCell {
    Block const*       expected    = nullptr;
    uint32_t           placementId = 0;
    VerificationStatus status      = VerificationStatus::Unknown;
    Block const*       observed    = nullptr;

    template <class Compare>
    bool observe(Block const* actual, Compare&& compare) {
        if (actual && actual == observed) return false;
        auto nextStatus = VerificationStatus::Unknown;
        if (actual && expected)
            nextStatus = actual == expected ? VerificationStatus::Matched : compare(expected, actual);
        bool changed = nextStatus != status;
        observed     = actual;
        status       = nextStatus;
        return changed;
    }
};

struct VerificationChunk {
    std::unordered_map<uint64_t, VerificationCell> cells;
    uint64_t                                       revision = 0;
};

struct VerifierState {
    std::unordered_map<util::WorldBlockKey, VerificationChunk, util::WorldBlockKeyHash> chunks;
    uint64_t                                                                            revision = 0;
    uint64_t                                                                            epoch    = 0;
    bool                                                                                clearing = false;

    [[nodiscard]] VerificationCell const* find(int dimension, BlockPos const& pos) const {
        if (clearing) return nullptr;
        auto it = chunks.find({dimension, util::subChunkKeyFromWorldPos(pos.x, pos.y, pos.z)});
        if (it == chunks.end()) return nullptr;
        auto cell = it->second.cells.find(util::encodePosKey(pos));
        return cell == it->second.cells.end() ? nullptr : &cell->second;
    }

    [[nodiscard]] bool showsProjection(int dimension, BlockPos const& pos, uint32_t placementId) const {
        auto* cell = find(dimension, pos);
        return !cell
            || (cell->placementId == placementId
                && (cell->status == VerificationStatus::Unknown || cell->status == VerificationStatus::MissingBlock));
    }
};
} 
