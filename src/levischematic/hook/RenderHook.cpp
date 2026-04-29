#include "RenderHook.h"

#include "levischematic/app/AppKernel.h"
#include "levischematic/render/ProjectionRenderer.h"
#include "levischematic/util/PositionUtils.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/chunks/RenderChunkBuilder.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"

struct BlockQueueEntry {
    BlockPos     pos;
    const Block* blockInfo;
};

class RenderChunkGeometry {
public:
    char     unk[0x34];
    BlockPos mPosition;
    BlockPos mCenter;
    char     unk2[0x1fc];
};

namespace levischematic::hook {

using namespace levischematic::render;
using namespace levischematic::util;

namespace {

bool gRenderHooksRegistered = false;

} // namespace


LL_TYPE_INSTANCE_HOOK(
    ProjectionSortBlocksHook,
    ll::memory::HookPriority::Normal,
    RenderChunkBuilder,
    &RenderChunkBuilder::_sortBlocks,
    bool,
    BlockSource&                                                 region,
    RenderChunkGeometry&                                         renderChunkGeometry,
    bool                                                         transparentLeaves,
    AirAndSimpleBlockBits&                                       airAndSimpleBlocks,
    RenderChunkPerformanceTrackingData::RenderChunkBuildDetails& renderChunkBuildDetails
) {
    bool result = origin(region, renderChunkGeometry, transparentLeaves, airAndSimpleBlocks, renderChunkBuildDetails);

    tl_hasProjection = false;
    tl_currentScene.reset();

    if (!app::hasAppKernel()) {
        return result;
    }

    auto& projection = app::getAppKernel().projection();
    (void)projection.flushRefresh(nullptr);

    tl_currentScene = projection.sceneForDimension(static_cast<int>(region.getDimensionId()));
    if (!tl_currentScene || tl_currentScene->empty()) {
        return result;
    }

    auto subChunkKey = encodeSubChunkKey(renderChunkGeometry.mPosition);
    auto it                = tl_currentScene->bySubChunk.find(subChunkKey);
    bool hasProjectionMesh = it != tl_currentScene->bySubChunk.end() && !it->second.empty();
    bool hasColorOverride =
        tl_currentScene->subChunksWithColorOverrides.find(subChunkKey) != tl_currentScene->subChunksWithColorOverrides.end();
    if (!hasProjectionMesh && !hasColorOverride) {
        return result;
    }

    tl_hasProjection = true;
    if (hasProjectionMesh) {
        for (auto const& entry : it->second) {
            BlockQueueEntry queueEntry;
            queueEntry.pos       = entry.pos;
            queueEntry.blockInfo = entry.block;
            this->mQueues[RENDERLAYER_BLEND].push_back(queueEntry);
        }
    }

    return result;
}

LL_TYPE_INSTANCE_HOOK(
    ProjectionTessellateHook,
    ll::memory::HookPriority::Normal,
    BlockTessellator,
    &BlockTessellator::tessellateInWorld,
    bool,
    Tessellator&    tessellator,
    Block const&    block,
    BlockPos const& pos,
    bool            useCalcWithCache
) {
    if (!tl_hasProjection) {
        return origin(tessellator, block, pos, useCalcWithCache);
    }

    auto it = tl_currentScene->posColorMap.find(encodePosKey(pos));
    if (it == tl_currentScene->posColorMap.end()) {
        return origin(tessellator, block, pos, useCalcWithCache);
    }

    this->mColorOverride = it->second;
    bool result          = origin(tessellator, block, pos, useCalcWithCache);
    this->mColorOverride->reset();
    return result;
}
using RenderHook = ll::memory::HookRegistrar<
    ProjectionSortBlocksHook,
    ProjectionTessellateHook>;

void registerRenderHooks() {
    if (gRenderHooksRegistered) {
        return;
    }
    RenderHook::hook();
    gRenderHooksRegistered = true;
}

void unregisterRenderHooks() {
    if (!gRenderHooksRegistered) {
        return;
    }
    RenderHook::unhook();
    gRenderHooksRegistered = false;
}

} // namespace levischematic::hook
