#include "RenderHook.h"

#include "levischematic/app/AppKernel.h"
#include "levischematic/render/BlockActorProjectionRenderer.h"
#include "levischematic/render/ProjectionRenderer.h"
#include "levischematic/schematic/block_actor/BlockActorRenderSchematic.h"
#include "levischematic/schematic/block_actor/SchematicChestRenderer.h"
#include "levischematic/util/PositionUtils.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/memory/Memory.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/model/GeometryGroup.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/actor/ActorResourceDefinitionGroup.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/blockactor/BlockActorRenderDispatcher.h"
#include "mc/client/renderer/chunks/RenderChunkBuilder.h"
#include "mc/client/renderer/chunks/RenderChunkGeometry.h"
#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/deps/core/resource/ResourceLocationPair.h"
#include "mc/deps/core_graphics/ImageBuffer.h"
#include "mc/deps/core_graphics/ImageResource.h"
#include "mc/deps/core_graphics/TextureSetDefinition.h"
#include "mc/deps/core_graphics/TextureSetImageContainer.h"
#include "mc/deps/core_graphics/TextureSetLayerImageMipList.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTexture.h"
#include "mc/util/texture_set_helpers/TextureSetDefinitionLoader.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/dimension/DimensionType.h"


struct BlockQueueEntry {
    BlockPos     pos;
    Block const& blockInfo;
};

namespace levischematic::hook {

using namespace levischematic::render;
using namespace levischematic::util;
namespace block_actor = ::levischematic::schematic::block_actor;

namespace {

bool gRenderHooksRegistered = false;

int floorDiv16(int value) noexcept { return value / 16 - (value % 16 != 0 && value < 0 ? 1 : 0); }

uint64_t renderColumnKeyFromWorldPos(int x, int z) noexcept {
    return (static_cast<uint64_t>(static_cast<uint32_t>(floorDiv16(x))) << 21)
         | static_cast<uint64_t>(static_cast<uint32_t>(floorDiv16(z)) & 0x1FFFFFu);
}

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

    tl_currentScene = projection.sceneForDimension(region.getDimensionId());
    if (!tl_currentScene || tl_currentScene->empty()) {
        return result;
    }

    auto const& renderPosition    = renderChunkGeometry.mPosition.get();
    auto        columnKey         = renderColumnKeyFromWorldPos(renderPosition.x, renderPosition.z);
    auto        it                = tl_currentScene->byRenderColumn.find(columnKey);
    bool        hasProjectionMesh = it != tl_currentScene->byRenderColumn.end() && !it->second.empty();
    bool        hasColorOverride  = tl_currentScene->columnsWithColorOverrides.contains(columnKey);
    if (!hasProjectionMesh && !hasColorOverride) {
        return result;
    }

    tl_hasProjection = true;
    if (hasProjectionMesh) {
        auto const& entries = it->second;
        for (auto const& entry : entries) {
            BlockQueueEntry queueEntry{entry.pos, *entry.block};
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

LL_TYPE_INSTANCE_HOOK(
    ProjectionBlockEntityHook,
    ll::memory::HookPriority::Normal,
    LevelRendererCamera,
    &LevelRendererCamera::$renderBlockEntities,
    void,
    BaseActorRenderContext& renderContext,
    bool                    renderAlphaLayer
) {
    origin(renderContext, renderAlphaLayer);

    if (!renderAlphaLayer || !app::hasAppKernel()) {
        return;
    }

    auto& manager = block_actor::BlockActorRenderSchematic::getInstance();
    if (manager.renderersIsEmpty()) {
        return;
    }

    auto& projection = app::getAppKernel().blockActorProjection();
    (void)projection.flushRefresh(nullptr);

    auto scene = projection.sceneForDimension(mViewRegion->get()->getDimensionId());
    if (!scene || scene->empty()) {
        return;
    }

    Vec3 cameraTargetPos = *renderContext.mCameraTargetPosition;
    AABB renderBounds(cameraTargetPos, cameraTargetPos);
    renderBounds.min.x -= 72.0f;
    renderBounds.min.y -= 72.0f;
    renderBounds.min.z -= 72.0f;
    renderBounds.max.x += 72.0f;
    renderBounds.max.y += 72.0f;
    renderBounds.max.z += 72.0f;

    for (auto const* entry : collectBlockActorsInAabb(*scene, renderBounds)) {
        if (!entry || !entry->block || !entry->blockActor) {
            continue;
        }

        Vec3 renderPosition;
        renderPosition.x = entry->pos.x - renderContext.mCameraTargetPosition->x;
        renderPosition.y = entry->pos.y - renderContext.mCameraTargetPosition->y;
        renderPosition.z = entry->pos.z - renderContext.mCameraTargetPosition->z;

        block_actor::BlockActorRenderDataForSchematic data{
            renderPosition,
            entry->pos,
            *entry->block,
            entry->blockActor.get(),
        };
        manager.renderSchematic(entry->rendererId, renderContext, data);
    }
}

LL_TYPE_INSTANCE_HOOK(
    ProjectionTextureUploadHook,
    ll::memory::HookPriority::Normal,
    mce::TextureGroup,
    &mce::TextureGroup::uploadTexture,
    BedrockTexture&,
    ResourceLocation const&                                      resourceLocation,
    gsl::not_null<::std::shared_ptr<::cg::TextureSetDefinition>> textureSetDefinition
) {
    auto& manager = block_actor::BlockActorRenderSchematic::getInstance();
    if (manager.renderersIsEmpty()) {
        return origin(resourceLocation, textureSetDefinition);
    }

    auto target = manager.findTextureUploadTarget(resourceLocation.getHashedPath());
    if (target.resource && !textureSetDefinition->_getImageContainer()->mLayerImageList->empty()) {
        int  transparency = static_cast<int>(target.renderer->mTransparency * 255);
        auto old = textureSetDefinition->_getImageContainer()->mLayerImageList.get()[0].mImageList->getImage(0);
        if (old) {
            cg::ImageBuffer      newBuf(*old);
            auto textureSetDefinitionNew = TextureSetHelpers::TextureSetDefinitionLoader::makeDefinitionFromImageBuffer(
                ResourceLocationPair(target.resource->blendRes, PackIdVersion::EMPTY(), 0),
                &newBuf,
                false,
                false
            );
            for (auto& item : textureSetDefinitionNew->_getImageContainer()->mLayerImageList.get()) {
                if (item.mImageList->isValid() && item.mLayerType == cg::TextureSetLayerType::Color) {
                    auto imageBuffer = item.mImageList->getImage(0);
                    if (imageBuffer) {
                        auto width  = imageBuffer->mImageDescription->mWidth;
                        auto height = imageBuffer->mImageDescription->mHeight;
                        for (uint32 i = 0; i < width * height * 4; i += 4) {
                            const_cast<cg::ImageBuffer*>(imageBuffer)->mStorage->get()[i + 3] =
                                static_cast<mce::Blob::value_type>(transparency);
                        }
                    }
                }
            }

            ProjectionTextureUploadHook::unhook();
            uploadTexture(target.resource->blendRes, textureSetDefinitionNew);
            manager.onTextureUploaded(target, this);
            ProjectionTextureUploadHook::hook();
        }
    }

    return origin(resourceLocation, textureSetDefinition);
}

LL_TYPE_INSTANCE_HOOK(
    ProjectionBlockActorRendererInitHook,
    ll::memory::HookPriority::Normal,
    BlockActorRenderDispatcher,
    &BlockActorRenderDispatcher::initializeBlockEntityRenderers,
    void,
    Bedrock::NotNullNonOwnerPtr<::GeometryGroup> const&                        geometryGroup,
    ::std::shared_ptr<::mce::TextureGroup>                                     textureGroup,
    ::BlockTessellator&                                                        blockTessellator,
    ::Bedrock::NotNullNonOwnerPtr<::ActorResourceDefinitionGroup const> const& actorResourceDefinitionGroup,
    ::ResourcePackManager&                                                     resourcePackManager,
    ::Bedrock::NotNullNonOwnerPtr<::ResourceLoadManager>                       resourceLoadManager,
    ::BaseGameVersion const&                                                   baseGameVersion,
    ::Experiments const&                                                       experiments
) {
    origin(
        geometryGroup,
        std::move(textureGroup),
        blockTessellator,
        actorResourceDefinitionGroup,
        resourcePackManager,
        resourceLoadManager,
        baseGameVersion,
        experiments
    );

    auto& manager = block_actor::BlockActorRenderSchematic::getInstance();
    if (!manager.hasRenderer(BlockActorRendererId::Chest)) {
        auto textureGroupFromClient = ll::service::getClientInstance()
                                        ? ll::service::getClientInstance()->getTextureGroup()
                                        : std::shared_ptr<::mce::TextureGroup>{};
        if (textureGroupFromClient) {
            manager.registerRenderer(
                BlockActorRendererId::Chest,
                std::make_unique<block_actor::SchematicChestRenderer>(std::move(textureGroupFromClient), 0.5f)
            );
        }
    }
}
using RenderHook = ll::memory::HookRegistrar<
    ProjectionSortBlocksHook,
    ProjectionTessellateHook,
    ProjectionBlockEntityHook,
    ProjectionTextureUploadHook,
    ProjectionBlockActorRendererInitHook>;

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
