#include "levischematic/render/blockentity/BlockEntityRenderDiagnostics.h"
#include "levischematic/render/chunk/SchematicChunkRenderer.h"
#include "levischematic/schematic/placement/model/PlacementGeometry.h"

#include "levischematic/render/chunk/model/SchematicRenderAlgorithms.h"
#include "levischematic/render/chunk/submission/SchematicChunkLayerSubmit.h"
#include "levischematic/render/chunk/visibility/VanillaChunkInstanceSet.h"
#include "levischematic/render/highlight/EditorHighlightConfiguration.h"
#include "levischematic/render/resources/NativeTextureVariant.h"

#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/schematic/placement/service/PlacementService.h"
#include "levischematic/schematic/world/FakeWorld.h"

#include "ll/api/memory/Memory.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/thread/ThreadPoolExecutor.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/MinecraftGraphics.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/block/tessellation_pipeline/client_block_pipeline/SchematicsRepository.h"
#include "mc/client/renderer/block/tessellation_pipeline/client_block_pipeline/TessellatorContext.h"
#include "mc/client/renderer/blockactor/BlockActorRenderDispatcher.h"
#include "mc/client/renderer/chunks/RenderChunkBuilder.h"
#include "mc/client/renderer/chunks/RenderChunkCoordinator.h"
#include "mc/client/renderer/chunks/RenderChunkGeometry.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/common/SubClientId.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/utility/ServiceLocator.h"
#include "mc/deps/core_graphics/TextureSetLayerType.h"
#include "mc/deps/minecraft_renderer/framebuilder/FrameBuilder.h"
#include "mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialVariants.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/actor/ChestBlockActor.h"
#include "mc/world/level/block/actor/component/IVanillaRenderBlockActorComponent.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionType.h"
#include "mc/world/phys/AABB.h"

#include <algorithm>
#include <fmt/format.h>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "levischematic/render/chunk/build/RendererInstance.h"
namespace levischematic::render {
namespace {
auto& getLogger() { return levischematic::LeviSchematic::getInstance().getSelf().getLogger(); }
} // namespace
std::size_t SchematicChunkRenderer::_renderBlockEntities(
    BaseActorRenderContext& renderContext,
    FakeWorld const&        world,
    AABB const&             area,
    Vec3 const&             cameraTargetPosition,
    bool                    renderAlphaLayer
) {
    if (!world.chunkSource || !world.blockSource || !renderContext.mImpl) return 0;


    MaterialVariants defaultMaterials;


    auto        entityMatrix = renderContext.mScreenContext.camera.worldMatrixStack->pushIdentity(false);
    auto&       region       = *world.blockSource;
    auto&       dispatcher   = renderContext.mBlockEntityRenderDispatcher;
    std::size_t dispatched   = 0;
    auto*       entry        = _findEntryByRegion(region);
    auto const  frame        = entry ? entry->entityDiagnosticFrame : 0;
    bool const  sample       = mConfig.diagnostics && entry && (frame == 1 || frame == 60 || frame == 180);
    std::size_t components = 0, alphaSkipped = 0, boundsSkipped = 0, missingRenderer = 0;
    std::size_t chests = 0, secondaryChests = 0, orphanChests = 0, chestSamples = 0, otherSamples = 0;
    diagnostics::BlockEntityTrace total;


    auto const chunks = world.chunkSource->mStructureLevelChunks;
    for (auto const& chunk : chunks) {
        if (!chunk) continue;
        for (auto component : chunk->mBlockEntities->mVanillaRenderBlockActorComponents.get()) {
            ++components;
            if (renderAlphaLayer && !component->hasAlphaLayer()) {
                ++alphaSkipped;
                continue;
            }
            auto const& pos            = component->getBlockActorPosition();
            auto        entityOrigin   = util::subChunkOrigin(pos.x, pos.y, pos.z);
            auto        distanceBounds = subChunkBounds(
                Vec3{
                    static_cast<float>(entityOrigin.x),
                    static_cast<float>(entityOrigin.y),
                    static_cast<float>(entityOrigin.z)
                }
            );
            auto entityVisibility =
                testVisibility(cameraTargetPosition, distanceBounds, mConfig.visibility, nullptr, false);
            if (entityVisibility != VisibilityResult::Visible) continue;
            if (!region.projectionVisible(pos) || region.getBlock(pos).isAir()) continue;
            detail::Point const local{static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z)};
            if (!detail::withinBlockEntityBounds(
                    local,
                    {area.min.x, area.min.y, area.min.z},
                    {area.max.x, area.max.y, area.max.z}
                )) {
                ++boundsSkipped;
                continue;
            }

            Vec3 const renderPos{
                local[0] - cameraTargetPosition.x,
                local[1] - cameraTargetPosition.y,
                local[2] - cameraTargetPosition.z
            };
            diagnostics::BlockEntityTrace trace;
            auto const                    rendererId = component->getRendererId();
            bool const rendererPresent = sample && static_cast<std::size_t>(rendererId) < dispatcher.mRenderers->size()
                                      && dispatcher.mRenderers->at(rendererId) != nullptr;
            if (sample && !rendererPresent) ++missingRenderer;
            auto* chest = sample && component->getBlockActor().isTypeOrDerived(BlockActorType::Chest)
                            ? static_cast<ChestBlockActor*>(&component->getBlockActor())
                            : nullptr;
            if (chest) {
                ++chests;
                if (!chest->mPairLead) ++secondaryChests;
                if (!chest->mPairLead && !chest->mLargeChestPaired) ++orphanChests;
            }
            {
                diagnostics::BlockEntityTraceScope traceScope(sample ? &trace : nullptr);
                dispatcher.render(
                    renderContext,
                    region,
                    *component,
                    region.getBlock(pos),
                    renderPos,
                    pos,
                    renderAlphaLayer,
                    defaultMaterials.mSkinningMaterialPtr,
                    nullptr,
                    0,
                    std::nullopt
                );
            }
            ++dispatched;
            if (sample) {
                total.chestCalls         += trace.chestCalls;
                total.modelCalls         += trace.modelCalls;
                total.modelGeometries    += trace.modelGeometries;
                total.groupCalls         += trace.groupCalls;
                total.hiddenGroups       += trace.hiddenGroups;
                total.meshCalls          += trace.meshCalls;
                total.hiddenMeshes       += trace.hiddenMeshes;
                total.indexCountOverride += trace.indexCountOverride;
                total.validMeshes        += trace.validMeshes;
                total.meshVertices       += trace.meshVertices;
                total.cpuIndices         += trace.cpuIndices;
                bool const detailSample   = chest ? chestSamples++ < 4 : otherSamples++ < 2;
                if (detailSample) {
                    auto const& parent =
                        renderContext.mScreenContext.camera.worldMatrixStack->stack->back()._m.get()[3];
                    getLogger().info(
                        "SchematicBlockEntities [placement #{} revision={} frame={} pass={}]: sample block={} "
                        "local=({},{},{}) render=({:.3f},{:.3f},{:.3f}) rendererId={} rendererPresent={} "
                        "chest={} lead={} paired={} deferred={} chestCalls={} actorTextureModelCalls={} "
                        "modelGeometries={} groupCalls={} hiddenGroups={} meshCalls={} hiddenMeshes={} "
                        "indexCountOverride={} validMeshes={} meshVertices={} cpuIndices={} "
                        "parentTranslation=({:.3f},{:.3f},{:.3f}) modelTranslation=({:.3f},{:.3f},{:.3f})",
                        entry->placementId,
                        entry->revision,
                        frame,
                        renderAlphaLayer ? "alpha" : "normal",
                        region.getBlock(pos).getTypeName(),
                        pos.x,
                        pos.y,
                        pos.z,
                        renderPos.x,
                        renderPos.y,
                        renderPos.z,
                        static_cast<int>(rendererId),
                        rendererPresent,
                        chest != nullptr,
                        chest && chest->mPairLead,
                        chest && chest->mLargeChestPaired != nullptr,
                        chest && chest->mDeferredPairLoad,
                        trace.chestCalls,
                        trace.modelCalls,
                        trace.modelGeometries,
                        trace.groupCalls,
                        trace.hiddenGroups,
                        trace.meshCalls,
                        trace.hiddenMeshes,
                        trace.indexCountOverride,
                        trace.validMeshes,
                        trace.meshVertices,
                        trace.cpuIndices,
                        parent.x,
                        parent.y,
                        parent.z,
                        trace.modelTranslation[0],
                        trace.modelTranslation[1],
                        trace.modelTranslation[2]
                    );
                }
            }
        }
    }
    if (sample) {
        getLogger().info(
            "SchematicBlockEntities [placement #{} revision={} frame={} pass={}]: summary components={} "
            "alphaSkipped={} boundsSkipped={} dispatched={} missingRenderer={} chests={} secondaryChests={} "
            "orphanChests={} chestCalls={} actorTextureModelCalls={} modelGeometries={} groupCalls={} "
            "hiddenGroups={} meshCalls={} hiddenMeshes={} indexCountOverride={} validMeshes={} "
            "meshVertices={} cpuIndices={} localCamera=({:.3f},{:.3f},{:.3f}) "
            "highlight={} frameAlpha={:.3f}",
            entry->placementId,
            entry->revision,
            frame,
            renderAlphaLayer ? "alpha" : "normal",
            components,
            alphaSkipped,
            boundsSkipped,
            dispatched,
            missingRenderer,
            chests,
            secondaryChests,
            orphanChests,
            total.chestCalls,
            total.modelCalls,
            total.modelGeometries,
            total.groupCalls,
            total.hiddenGroups,
            total.meshCalls,
            total.hiddenMeshes,
            total.indexCountOverride,
            total.validMeshes,
            total.meshVertices,
            total.cpuIndices,
            cameraTargetPosition.x,
            cameraTargetPosition.y,
            cameraTargetPosition.z,
            renderContext.mScreenContext.isDrawingEditorSelectionHighlight,
            renderContext.mFrameAlpha
        );
    }
    return dispatched;
}


} // namespace levischematic::render
