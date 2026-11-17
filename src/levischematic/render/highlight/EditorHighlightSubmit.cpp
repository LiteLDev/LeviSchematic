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

#include "EditorHighlightSubmit.h"
namespace levischematic::render {
namespace {
auto& getLogger() { return levischematic::LeviSchematic::getInstance().getSelf().getLogger(); }
}
constexpr float kOverlayOutlineWidth       = 0.0f;
constexpr float kOverlayBlockEdgeThickness = 0.0f;


constexpr float kOverlayHiddenBlocksAlpha = 0.0f;
constexpr float kOverlayTextureStretch    = 1.0f;

constexpr auto kOverlayDrawMode = static_cast<mce::framebuilder::gamecomponents::HighlightDrawMode>(1);

constexpr mce::Color kOverlayEdgeColor{0.0f, 0.0f, 0.0f, 0.0f};


void logOverlayTextureUnavailable(char const* what, std::string const& texture) {
    static std::string_view reported;
    if (reported == what) {
        return;
    }
    reported = what;
    getLogger().warn(
        "SchematicChunkRenderer: overlay texture '{}' unavailable ({}); EditorHighlightConfiguration is not "
        "submitted, so the projection cannot reach the screen through the highlight pass",
        texture,
        what
    );
}


void logEditorHighlightSubmitSkipped(char const* what) {
    static std::string_view reported;
    if (reported == what) {
        return;
    }
    reported = what;
    getLogger().warn(
        "SchematicChunkRenderer: EditorHighlightConfiguration not submitted ({}); the highlight pass has no overlay "
        "configuration this frame and the projection stays invisible",
        what
    );
}


bool submitEditorHighlight(
    mce::ServerResourcePointer<dragon::ResolvedTextureResource> const& overlayTexture,
    app::Config const&                                                 config
) {
    auto frameBuilderService = ServiceLocator<mce::framebuilder::FrameBuilder>::get();
    if (!frameBuilderService || !frameBuilderService.get()->enabled()) {
        logEditorHighlightSubmitSkipped(
            !frameBuilderService ? "FrameBuilder service unavailable" : "FrameBuilder service disabled"
        );
        return false;
    }


    mce::framebuilder::gamecomponents::EditorHighlightConfiguration::Style const style{
        mce::Color{
                   config.projection.color[0],
                   config.projection.color[1],
                   config.projection.color[2],
                   config.projection.color[3]
        },
        kOverlayEdgeColor,
        kOverlayEdgeColor,
        config.projection.alpha,
        kOverlayOutlineWidth,
        kOverlayBlockEdgeThickness,
        kOverlayHiddenBlocksAlpha,
        kOverlayTextureStretch,
    };

    mce::framebuilder::gamecomponents::EditorHighlightConfiguration description{
        overlayTexture,
        kOverlayDrawMode,
        style
    };


    frameBuilderService.get()->_insert(description);


    static bool reportedSubmit = false;
    if (!reportedSubmit) {
        reportedSubmit = true;
        getLogger().info(
            "SchematicChunkRenderer: EditorHighlightConfiguration submitted (drawMode={}, color={:.2f}/{:.2f}/"
            "{:.2f}/{:.2f}, outline={:.2f}/{:.2f}/{:.2f}/{:.2f}, blockEdge={:.2f}/{:.2f}/{:.2f}/{:.2f}, alpha={:.3f}, "
            "outlineWidth={:.2f}, blockEdgeThickness={:.2f}, hiddenBlocksAlpha={:.3f}, textureStretch={:.2f}, "
            "overlayTex={:p})",
            static_cast<std::int32_t>(kOverlayDrawMode),
            style.mColor->r,
            style.mColor->g,
            style.mColor->b,
            style.mColor->a,
            style.mOutlineColor->r,
            style.mOutlineColor->g,
            style.mOutlineColor->b,
            style.mOutlineColor->a,
            style.mBlockEdgeColor->r,
            style.mBlockEdgeColor->g,
            style.mBlockEdgeColor->b,
            style.mBlockEdgeColor->a,
            style.mAlpha,
            style.mOutlineWidth,
            style.mBlockEdgeThickness,
            style.mHiddenBlocksAlpha,
            style.mOverlayTextureStretch,
            static_cast<void*>(overlayTexture.mResourcePointerBlock.get())
        );
    }
    return true;
}


}
