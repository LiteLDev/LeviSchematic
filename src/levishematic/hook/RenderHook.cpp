#include "RenderHook.h"

#include "levishematic/app/AppKernel.h"
#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/util/PositionUtils.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/chunks/RenderChunkBuilder.h"
#include "mc/world/level/block/Block.h"

#include <Windows.h>

struct BlockQueueEntry {
    BlockPos     pos;
    const Block* blockInfo;
};

struct RenderChunkGeometry {
    char     unk[0x34];
    BlockPos mPosition;
    BlockPos mCenter;
    char     unk2[0x1fc];
};

namespace levishematic::hook {

using namespace levishematic::render;
using namespace levishematic::util;

namespace {

bool gRenderHooksRegistered = false;
unsigned long long _sortBlocks_va;

} // namespace

void atest() {
    HMODULE hModule = GetModuleHandle(nullptr);
    _sortBlocks_va = reinterpret_cast<unsigned long long>(hModule) + 0x201a600;
}

// 0x201a600：
LL_TYPE_INSTANCE_HOOK(
    ProjectionSortBlocksHook,
    ll::memory::HookPriority::Normal,
    RenderChunkBuilder,
    _sortBlocks_va,
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
    projection.flushRefresh(nullptr);

    tl_currentScene = projection.scene();
    if (!tl_currentScene || tl_currentScene->empty()) {
        return result;
    }

    auto subChunkKey = encodeSubChunkKey(renderChunkGeometry.mPosition);
    auto it          = tl_currentScene->bySubChunk.find(subChunkKey);
    if (it == tl_currentScene->bySubChunk.end() || it->second.empty()) {
        return result;
    }

    tl_hasProjection = true;
    for (auto const& entry : it->second) {
        BlockQueueEntry queueEntry;
        queueEntry.pos       = entry.pos;
        queueEntry.blockInfo = entry.block;
        this->mQueues[RENDERLAYER_BLEND].push_back(queueEntry);
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

} // namespace levishematic::hook
