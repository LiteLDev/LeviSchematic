#pragma once
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

namespace levischematic::render {
struct SchematicChunkRenderer::SchematicRendererInstanceData {


    ~SchematicRendererInstanceData() {


        buildTask.wait();

        chunkBuilder.reset();
        chunkInstances.clear();
        world.reset();
    }

    placement::PlacementId              placementId = 0;
    std::unique_ptr<FakeWorld>          world;
    std::unique_ptr<RenderChunkBuilder> chunkBuilder;


    VanillaChunkInstanceSet chunkInstances;

    std::size_t lastSortedInstanceCount = 0;

    std::size_t                    lastSubmittedLayerCount = 0;
    std::set<std::array<int, 3>>   submittedOrigins;
    uint64_t                       revision              = 0;
    uint64_t                       contentRevision       = 0;
    uint64_t                       renderTick            = 0;
    uint64_t                       entityDiagnosticFrame = 0;
    BlockEntityHighlightStats      blockEntities;
    int                            dimensionId = -1;
    detail::ChunkCpuTask           buildTask;
    SchematicRenderChunkInstanced* pendingBuildChunk = nullptr;
    bool                           buildFailed       = false;
    bool                           buildReady        = false;
    uint64_t                       boundsRevision    = UINT64_MAX;
    AABB                           worldBounds;
    BlockPos                       placementOrigin;
    std::size_t                    completedBuildsThisFrame = 0;
};


}
