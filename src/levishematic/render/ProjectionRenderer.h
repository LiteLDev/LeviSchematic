#pragma once

#include "levishematic/util/PositionUtils.h"
#include "levishematic/verifier/VerifierTypes.h"

#include "mc/deps/core/math/Color.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

class RenderChunkCoordinator;

namespace levishematic::placement {
class PlacementProjectionCache;
struct PlacementState;
}

namespace levishematic::render {

using PlacementProjectionId = uint32_t;

struct ProjEntry {
    BlockPos     pos;
    const Block* block = nullptr;
    mce::Color   color;
};

inline constexpr int        RENDERLAYER_BLEND = 3;
inline constexpr mce::Color kDefaultProjectionColor(0.75f, 0.85f, 1.0f, 0.85f);
inline constexpr mce::Color kPropertyMismatchProjectionColor(0.95f, 0.82f, 0.28f, 0.9f);
inline constexpr mce::Color kBlockMismatchProjectionColor(0.95f, 0.35f, 0.35f, 0.9f);

struct ProjectionScene {
    std::unordered_map<uint64_t, std::vector<ProjEntry>> bySubChunk;
    std::unordered_map<uint64_t, mce::Color>             posColorMap;

    [[nodiscard]] bool empty() const { return posColorMap.empty(); }
};

struct PatchOp {
    enum class Kind {
        Remove,
        SetBlock,
        ClearOverride,
    };

    Kind        kind  = Kind::Remove;
    const Block* block = nullptr;
    mce::Color   color = kDefaultProjectionColor;

    static PatchOp remove() { return PatchOp{Kind::Remove}; }
    static PatchOp clearOverride() { return PatchOp{Kind::ClearOverride}; }
    static PatchOp setBlock(const Block* block, mce::Color color = kDefaultProjectionColor) {
        return PatchOp{Kind::SetBlock, block, color};
    }
};

class ProjectionProjector {
public:
    ProjectionProjector();
    ~ProjectionProjector();

    [[nodiscard]] std::shared_ptr<const ProjectionScene> scene() const;
    [[nodiscard]] bool needsRefresh(uint64_t placementsRevision, uint64_t verifierRevision) const;

    void rebuild(
        placement::PlacementState const& state,
        verifier::VerifierState const&   verifierState
    );
    void rebuildAndRefresh(
        placement::PlacementState const&               state,
        verifier::VerifierState const&                 verifierState,
        std::shared_ptr<RenderChunkCoordinator> const& coordinator
    );
    void triggerRebuild(std::shared_ptr<RenderChunkCoordinator> const& coordinator) const;
    void triggerRebuildForPosition(
        BlockPos const&                                pos,
        std::shared_ptr<RenderChunkCoordinator> const& coordinator
    ) const;
    void clear();

private:
    void rebuildLocked(
        placement::PlacementState const&               state,
        verifier::VerifierState const&                 verifierState,
        std::shared_ptr<RenderChunkCoordinator> const& coordinator,
        bool                                           triggerRefresh
    );

    std::atomic<std::shared_ptr<const ProjectionScene>> mScene;
    std::unique_ptr<placement::PlacementProjectionCache> mPlacementCache;
    uint64_t                                            mProjectedRevision = 0;
    uint64_t                                            mVerifierRevision  = 0;
    mutable std::mutex                                  mMutex;
};

extern thread_local std::shared_ptr<const ProjectionScene> tl_currentScene;
extern thread_local bool                                   tl_hasProjection;

} // namespace levishematic::render
