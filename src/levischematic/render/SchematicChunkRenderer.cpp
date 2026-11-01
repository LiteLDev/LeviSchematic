#include "SchematicChunkRenderer.h"
#include "BlockEntityRenderDiagnostics.h"

#include "EditorHighlightConfiguration.h"
#include "NativeTextureVariant.h"
#include "SchematicChunkLayerSubmit.h"
#include "SchematicRenderAlgorithms.h"
#include "VanillaChunkInstanceSet.h"

#include "levischematic/LeviSchematic.h"
#include "levischematic/app/PlacementService.h"
#include "levischematic/schematic/FakeWorld.h"

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
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace levischematic::render {
namespace {


auto& getLogger() { return levischematic::LeviSchematic::getInstance().getSelf().getLogger(); }


constexpr float kOverlayOutlineWidth       = 0.0f;
constexpr float kOverlayBlockEdgeThickness = 0.0f;
constexpr float kOverlayHiddenBlocksAlpha  = 0.125f;
constexpr float kOverlayTextureStretch     = 1.0f;

constexpr auto       kOverlayDrawMode = static_cast<mce::framebuilder::gamecomponents::HighlightDrawMode>(1);
constexpr mce::Color kOverlayHighlightColor{1.0f, 1.0f, 1.0f, 0.0f};
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


void submitEditorHighlight(
    mce::ServerResourcePointer<dragon::ResolvedTextureResource> const& overlayTexture,
    app::Config const&                                                 config
) {
    auto frameBuilderService = ServiceLocator<mce::framebuilder::FrameBuilder>::get();
    if (!frameBuilderService || !frameBuilderService.get()->enabled()) {
        logEditorHighlightSubmitSkipped(
            !frameBuilderService ? "FrameBuilder service unavailable" : "FrameBuilder service disabled"
        );
        return;
    }


    mce::framebuilder::gamecomponents::EditorHighlightConfiguration::Style const style{
        kOverlayHighlightColor,
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
}


ScreenContext makeSchematicScreenContext(ScreenContext& screenContext) {
    return ScreenContext{
        screenContext.viewport,
        screenContext.guiData,
        screenContext.clock,
        screenContext.tessellator,
        screenContext.minecraftGraphics,
        screenContext.profanityContext,
        screenContext.commandListQueue,
        screenContext.frameAllocator,
        screenContext,
        screenContext.xm,
        screenContext.ym,
        0.0f
    };
}


} 


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
    uint64_t                       revision                = 0;
    uint64_t                       contentRevision         = 0;
    uint64_t                       renderTick              = 0;
    uint64_t                       entityDiagnosticFrame   = 0;
    int                            dimensionId             = -1;
    detail::ChunkCpuTask           buildTask;
    SchematicRenderChunkInstanced* pendingBuildChunk = nullptr;
    bool                           buildFailed       = false;
    bool                           buildReady        = false;
    uint64_t                       boundsRevision    = UINT64_MAX;
    AABB                           worldBounds;
    BlockPos                       placementOrigin;
    std::size_t                    completedBuildsThisFrame = 0;
};

SchematicChunkRenderer::SchematicChunkRenderer()
: mChunkExecutor(std::make_unique<ll::thread::ThreadPoolExecutor>("LeviSchematicChunkBuild", 2)) {}
SchematicChunkRenderer::~SchematicChunkRenderer() {
    clear();
    if (mChunkExecutor) mChunkExecutor->destroy();
}

void SchematicChunkRenderer::clear() {
    std::scoped_lock lock(mMutex);
    clearUnlocked();
}

void SchematicChunkRenderer::clearUnlocked() {
    mVisiblePlacementsValid = false;
    mVisiblePlacementIds.clear();
    mEntries.clear();
    mRetiredEntries.clear();
    mDiagnosticReasons.clear();
    mValidationHullRenderer.clear();
    overlayTexture.mResourcePointerBlock.reset();
    overlayTexturePtr    = {};
    mLevel               = nullptr;
    mLevelRenderer       = nullptr;
    mLevelRendererPlayer = nullptr;
    mDimension           = nullptr;
}

void SchematicChunkRenderer::renderValidationHull(
    ScreenContext&                         context,
    Vec3 const&                            camera,
    app::PlacementService const&           placements,
    int                                    dimension,
    selection::SelectionOverlayView const& selection
) {
    std::unique_lock lock(mMutex, std::try_to_lock);
    if (!lock.owns_lock() || mSuspended) return;
    auto hullConfig                       = mConfig;
    hullConfig.performance.frameBudgetMs *= 0.5;
    mValidationHullRenderer.renderPass(context, camera, placements, dimension, hullConfig, selection);
}
bool SchematicChunkRenderer::hasValidationHull(
    app::PlacementService const& placements,
    BlockPos const&              pos,
    int                          dimension
) const {
    std::unique_lock lock(mMutex, std::try_to_lock);
    return lock.owns_lock() && !mSuspended && mValidationHullRenderer.contains(placements, pos, dimension, mConfig);
}
void SchematicChunkRenderer::applyConfig(app::Config const& config) {
    std::scoped_lock lock(mMutex);
    if (mConfig.projection.texture != config.projection.texture) {
        overlayTexture.mResourcePointerBlock.reset();
        overlayTexturePtr = {};
    }
    if (mConfig.hull != config.hull) mValidationHullRenderer.clear();
    mConfig = config;
}

void SchematicChunkRenderer::suspend(bool releaseMaterials) {
    std::scoped_lock lock(mMutex);
    mSuspended = true;
    clearUnlocked();
    if (releaseMaterials) mTerrainMaterials.reset();
}

void SchematicChunkRenderer::resume() {
    std::scoped_lock lock(mMutex);
    if (!mSuspended) {
        return;
    }
    mSuspended = false;
}

void SchematicChunkRenderer::resetMaterialResources() {
    std::scoped_lock lock(mMutex);
    clearUnlocked();
    mTerrainMaterials.reset();
}

std::size_t SchematicChunkRenderer::entryCount() const {
    std::scoped_lock lock(mMutex);
    return mEntries.size();
}

void SchematicChunkRenderer::renderFrame(ScreenContext& screenContext, app::PlacementService const& placements) {
    std::unique_lock lock(mMutex, std::try_to_lock);
    if (!lock.owns_lock() || mSuspended) {
        return;
    }

    auto  client   = ll::service::getClientInstance();
    auto* player   = client ? client->getLocalPlayer() : nullptr;
    auto* level    = client ? client->getLevel() : nullptr;
    auto* renderer = client ? client->getLevelRenderer() : nullptr;
    if (!player || !level || !renderer || !renderer->mLevelRendererPlayer.get()) {
        clearUnlocked();
        return;
    }

    auto* dimension      = &player->getDimension();
    auto* rendererPlayer = renderer->mLevelRendererPlayer.get();
    if (mLevel != level || mLevelRenderer != renderer || mLevelRendererPlayer != rendererPlayer
        || mDimension != dimension) {
        clearUnlocked();
        mLevel               = level;
        mLevelRenderer       = renderer;
        mLevelRendererPlayer = rendererPlayer;
        mDimension           = dimension;
    }

    auto const dimensionId = static_cast<int>(player->getDimensionId());


    auto placementLock = placements.tryLockPlacements();
    if (!placementLock.owns_lock()) return;
    auto const revision = placements.revision();
    if (!mVisiblePlacementsValid || mPlacementsRevision != revision) {
        std::vector<placement::PlacementId>        visible;
        std::unordered_set<placement::PlacementId> visibleIds;
        for (auto const& item : placements.orderedPlacements()) {
            auto const& placement = item.get();
            if (placement.enabled && placement.renderEnabled && placement.dimensionId == dimensionId && placement.asset
                && !placement.asset->localBlocks.empty()) {
                visible.push_back(placement.id);
                visibleIds.insert(placement.id);
            }
        }
        for (auto it = mEntries.begin(); it != mEntries.end();) {
            if (visibleIds.contains(it->first)) {
                ++it;
            } else {
                mRetiredEntries.push_back(std::move(it->second));
                it = mEntries.erase(it);
            }
        }
        std::erase_if(
            mDiagnosticReasons,


            [&](auto const& item) { return !visibleIds.contains(item.first); }
        );
        mVisiblePlacementIds    = std::move(visible);
        mPlacementsRevision     = revision;
        mVisiblePlacementsValid = true;
    }

    std::erase_if(
        mRetiredEntries,


        [](auto const& entry) { return !entry->buildTask.pending() || entry->buildTask.ready(); }
    );
    if (mVisiblePlacementIds.empty()) {
        overlayTexture.mResourcePointerBlock.reset();
        overlayTexturePtr = {};
        return;
    }


    (void)mTerrainMaterials.ensure();

    util::WorkBudget budget(mConfig.performance.frameBudgetMs * 0.5);
    mFrameBuilds  = 0;
    mFrameResorts = 0;
    for (size_t i = 0; i < mVisiblePlacementIds.size(); ++i) {
        auto id = mVisiblePlacementIds[(i + mPrepareStart) % mVisiblePlacementIds.size()];
        if (auto const* placement = placements.findPlacement(id))
            render(screenContext, *placement, dimensionId, placements, budget);
    }
    ++mPrepareStart;
}

std::vector<SchematicChunkRenderer::DebugStats> SchematicChunkRenderer::debugStats() const {
    std::scoped_lock        lock(mMutex);
    std::vector<DebugStats> stats;
    stats.reserve(mEntries.size());

    for (auto const& [id, entry] : mEntries) {
        DebugStats item;
        item.placementId             = id;
        item.revision                = entry->revision;
        item.renderTick              = entry->renderTick;
        item.dimensionId             = entry->dimensionId;
        item.hasWorld                = static_cast<bool>(entry->world);
        item.hasChunkBuilder         = static_cast<bool>(entry->chunkBuilder);
        item.hasChunkSorter          = entry->chunkInstances.hasSorter();
        item.instanceCacheReady      = entry->chunkInstances.ready();
        item.chunkInstanceCount      = entry->chunkInstances.instanceCount();
        item.lastSortedInstanceCount = entry->lastSortedInstanceCount;
        item.lastSubmittedLayerCount = entry->lastSubmittedLayerCount;
        item.pendingBuildCount       = entry->pendingBuildChunk ? 1 : 0;
        stats.push_back(std::move(item));
    }

    std::sort(
        stats.begin(),
        stats.end(),


        [](DebugStats const& lhs, DebugStats const& rhs) { return lhs.placementId < rhs.placementId; }
    );
    return stats;
}

void SchematicChunkRenderer::_pollChunkTask(
    SchematicRendererInstanceData& entry,
    ScreenContext&                 screenContext,
    bool                           publish
) {
    if (!entry.buildTask.ready() && !entry.buildReady) return;
    try {
        if (entry.buildTask.ready()) {
            entry.buildTask.take();
            entry.buildReady = true;
        }
        if (!publish || mFrameBuilds >= mConfig.performance.geometryBuildsPerFrame) return;
        ++mFrameBuilds;
        entry.chunkInstances
            .finishRebuild(*entry.pendingBuildChunk, *entry.chunkBuilder, screenContext.bufferResourceService);
        entry.pendingBuildChunk = nullptr;
        entry.buildReady        = false;
        ++entry.completedBuildsThisFrame;
    } catch (std::exception const& error) {
        getLogger().error(
            "SchematicChunkRenderer [placement #{}]: async chunk build failed: {}",
            entry.placementId,
            error.what()
        );
        entry.buildFailed = true;
    } catch (...) {
        getLogger().error("SchematicChunkRenderer [placement #{}]: async chunk build failed", entry.placementId);
        entry.buildFailed = true;
    }
    if (entry.buildFailed) {


        entry.pendingBuildChunk = nullptr;
        entry.buildReady        = false;
        entry.chunkBuilder.reset();
        entry.chunkInstances.clear();
    }
}

std::size_t SchematicChunkRenderer::_pendingChunkBuildCount() const {
    std::size_t count = 0;
    for (auto const& [id, entry] : mEntries) count += entry->buildTask.running();
    for (auto const& entry : mRetiredEntries) count += entry->buildTask.running();
    return count;
}

void SchematicChunkRenderer::logDiagnostic(placement::PlacementId id, std::string const& reason) {
    if (!mConfig.diagnostics) return;
    auto it = mDiagnosticReasons.find(id);
    if (it != mDiagnosticReasons.end() && it->second == reason) {
        return;
    }

    getLogger().info("SchematicChunkRenderer [placement #{}]: {}", id, reason);
    mDiagnosticReasons[id] = reason;
}

bool SchematicChunkRenderer::_ensureOverlayTexture() {


    overlayTexture.reset();

    auto client = ll::service::getClientInstance();
    if (client == nullptr) {
        logOverlayTextureUnavailable("client instance unavailable", mConfig.projection.texture);
        return false;
    }

    auto textureGroup = client->getTextureGroup();
    if (!textureGroup) {
        logOverlayTextureUnavailable("texture group unavailable", mConfig.projection.texture);
        return false;
    }

    ResourceLocation const location{Core::PathView{mConfig.projection.texture}};
    overlayTexturePtr = textureGroup->getTexture(location, false, 0, cg::TextureSetLayerType::Color);


    auto const* textureData = overlayTexturePtr.mClientTexture ? overlayTexturePtr.mClientTexture.get()
                                                               : &mce::TexturePtr::mDefaultTextureData();


    mce::ServerTexture::ServerResource const serverResource{textureData->mClientTexture};

    auto const*       outer        = serverResource.mResourcePointerBlock.get();
    auto const        view         = detail::inspectNativeTextureVariant(outer ? outer->mResource.get() : nullptr);
    auto const*       inner        = view.resolved ? view.resolved->mResourcePointerBlock.get() : nullptr;
    std::string const textureState = fmt::format(
        "default={}, missing={}, loadState=0x{:02x}, outer={}, nativeVariant={}, inner={}, innerValid={}",
        !overlayTexturePtr.mClientTexture,
        textureData->mIsMissingTexture == IsMissingTexture::Yes,
        static_cast<unsigned>(textureData->mTextureLoadState),
        outer != nullptr,
        view.tag,
        inner != nullptr,
        inner && inner->mTrackingBlock.mIsValid.load()
    );
    static std::string lastTextureState;
    if (lastTextureState != textureState) {
        getLogger()
            .info("SchematicChunkRenderer [overlay texture probe]: {} | {}", mConfig.projection.texture, textureState);
        lastTextureState = textureState;
    }

    overlayTexture = detail::unwrapServerTexture(serverResource);
    if (overlayTexture.mResourcePointerBlock == nullptr) {
        logOverlayTextureUnavailable(
            "server resource handle invalid after unwrapServerTexture",
            mConfig.projection.texture
        );
        return false;
    }


    static bool reportedResolved = false;
    if (!reportedResolved) {
        getLogger().info(
            "SchematicChunkRenderer: overlay texture '{}' resolved (resourceBlock={:p})",
            mConfig.projection.texture,
            static_cast<void*>(overlayTexture.mResourcePointerBlock.get())
        );
        reportedResolved = true;
    }
    return true;
}

void SchematicChunkRenderer::render(
    ScreenContext&                      screenContext,
    placement::PlacementInstance const& placement,
    int                                 currentDimensionId,
    app::PlacementService const&        placements,
    util::WorkBudget const&             budget
) {
    if (!placement.enabled) {
        logDiagnostic(placement.id, "skipped: placement disabled");
        return;
    }
    if (!placement.renderEnabled) {
        logDiagnostic(placement.id, "skipped: render disabled");
        return;
    }
    if (placement.dimensionId != currentDimensionId) {
        logDiagnostic(
            placement.id,
            "skipped: dimension mismatch (placement=" + std::to_string(placement.dimensionId)
                + ", current=" + std::to_string(currentDimensionId) + ")"
        );
        return;
    }
    if (!placement.asset || (placement.asset->localBlocks.empty() && placement.overrides.empty())) {
        logDiagnostic(placement.id, "skipped: schematic has no local blocks");
        return;
    }

    auto client = ll::service::getClientInstance();
    if (!client) {
        logDiagnostic(placement.id, "skipped: client instance unavailable");
        return;
    }

    auto* level         = client->getLevel();
    auto* levelRenderer = client->getLevelRenderer();
    if (level == nullptr || levelRenderer == nullptr || !levelRenderer->mLevelRendererPlayer.get()) {
        logDiagnostic(placement.id, "skipped: level or level renderer unavailable");
        return;
    }

    auto& ownedEntry = mEntries[placement.id];
    if (!ownedEntry) ownedEntry = std::make_unique<SchematicRendererInstanceData>();
    auto* entry                     = ownedEntry.get();
    entry->placementId              = placement.id;
    entry->placementOrigin          = placement.origin;
    entry->completedBuildsThisFrame = 0;
    _pollChunkTask(*entry, screenContext, false);
    if (entry->boundsRevision != placement.revision) {
        auto size = transformedSizeFor(placement);
        Vec3 lower{
            static_cast<float>(placement.origin.x),
            static_cast<float>(placement.origin.y),
            static_cast<float>(placement.origin.z)
        };
        Vec3 upper{
            lower.x + static_cast<float>(size.x),
            lower.y + static_cast<float>(size.y),
            lower.z + static_cast<float>(size.z)
        };
        for (auto const& [key, value] : placement.overrides) {
            if (value.kind != placement::OverrideEntry::Kind::SetBlock || !value.block || value.block->isAir())
                continue;
            auto position = util::decodePosKey(key);
            lower.x       = std::min(lower.x, static_cast<float>(position.x));
            lower.y       = std::min(lower.y, static_cast<float>(position.y));
            lower.z       = std::min(lower.z, static_cast<float>(position.z));
            upper.x       = std::max(upper.x, static_cast<float>(position.x) + 1.0f);
            upper.y       = std::max(upper.y, static_cast<float>(position.y) + 1.0f);
            upper.z       = std::max(upper.z, static_cast<float>(position.z) + 1.0f);
        }
        entry->worldBounds = AABB{
            {lower.x - 0.01f, lower.y - 0.01f, lower.z - 0.01f},
            {upper.x + 0.01f, upper.y + 0.01f, upper.z + 0.01f}
        };
        entry->boundsRevision = placement.revision;
    }
    auto* rendererCamera = levelRenderer->mLevelRendererPlayer.get();
    auto  visibility =
        testVisibility(rendererCamera->mCameraPos.get(), entry->worldBounds, mConfig.visibility, rendererCamera);
    if (visibility != VisibilityResult::Visible) return;
    if (!entry->world && !budget.available()) return;

    auto dimension = level->getDimension(DimensionType{placement.dimensionId}).lock();
    if (!dimension) {
        logDiagnostic(placement.id, "skipped: target dimension unavailable");
        return;
    }

    ScreenContext schematicContext                     = makeSchematicScreenContext(screenContext);
    schematicContext.isDrawingUI                       = false;
    schematicContext.isDrawingEditorSelectionHighlight = true;
    schematicContext.subClientId                       = screenContext.subClientId;

    BaseActorRenderContext renderContext(schematicContext, *client, client->getMinecraftGame_DEPRECATED());

    bool const needsRebuild = !entry->world || entry->contentRevision != placement.contentRevision
                           || entry->dimensionId != placement.dimensionId;

    if (needsRebuild) {


        if (entry->buildTask.pending() && !entry->buildTask.ready()) return;
        getLogger().info(
            "SchematicChunkRenderer [placement #{}]: rebuilding fake world (revision={}, dim={})",
            placement.id,
            placement.revision,
            placement.dimensionId
        );


        if (entry->buildTask.pending()) {
            try {
                entry->buildTask.take();
            } catch (...) {}
        }
        entry->pendingBuildChunk = nullptr;
        entry->buildReady        = false;
        entry->buildFailed       = false;
        entry->chunkBuilder.reset();
        entry->chunkInstances.reset();
        entry->world.reset();
        entry->lastSortedInstanceCount = 0;
        entry->entityDiagnosticFrame   = 0;

        try {
            entry->world = std::make_unique<FakeWorld>(placement, *dimension);
        } catch (std::exception const& error) {
            logDiagnostic(placement.id, error.what());
            entry->buildFailed = true;
            return;
        }
        entry->contentRevision = placement.contentRevision;
        entry->dimensionId     = placement.dimensionId;
    }
    entry->revision = placement.revision;
    _pollChunkTask(*entry, screenContext, budget.available());
    if (entry->buildFailed) return;
    if (!entry->pendingBuildChunk && entry->world) {
        try {
            entry->world->advance(placement, placements.verification(), placements.viewState(), budget);
        } catch (std::exception const& error) {
            logDiagnostic(placement.id, error.what());
            entry->buildFailed = true;
            return;
        }
        for (auto key : entry->world->dirty) {
            auto index = util::decodePosKey(key);
            entry->chunkInstances.invalidate({index.x * 16, index.y * 16, index.z * 16});
        }
        entry->world->dirty.clear();
    }


    if (!entry->world || !entry->world->blockSource) {
        logDiagnostic(placement.id, "skipped: fake world has no block source");
        return;
    }

    auto& region = *entry->world->blockSource;
    if (!entry->world->indexed) return;
    AABB const area = region.mBounds;

    if (!entry->chunkBuilder) {
        auto coordinator = levelRenderer->getRenderChunkCoordinator(DimensionType{placement.dimensionId});
        if (!coordinator) {
            logDiagnostic(placement.id, "skipped: render chunk coordinator unavailable");
            return;
        }

        ClientBlockPipeline::TessellatorContext pipelineContext(renderContext.mMinecraftGame);
        pipelineContext.mSchematicsRepo       = client->getMinecraftGame_DEPRECATED().getSchematicsRepository();
        pipelineContext.mTextureShiftsEnabled = levelRenderer->mTextureShiftsEnabled;

        WeakRef<RenderChunkCoordinator> weakCoordinator{coordinator};
        auto                            graphics = client->getMinecraftGraphics();
        if (!_initializeChunkBuilder(
                *entry,
                pipelineContext,
                graphics->mMinecraftGameplayGraphicsResources.get(),
                region,
                weakCoordinator,
                area
            )) {
            logDiagnostic(placement.id, "skipped: chunk builder initialization failed");
            return;
        }
    }
    if (!entry->pendingBuildChunk) {
        entry->chunkBuilder->mGUIRendering     = true;
        entry->chunkBuilder->mBuildBoundingBox = area;
    }


    if (_ensureOverlayTexture()) {
        submitEditorHighlight(overlayTexture, mConfig);
    }

    renderContext.mIgnoresLighting = true;


    Vec3 const& cameraPos = levelRenderer->mLevelRendererPlayer->mCameraPos.get();
    Vec3 const  cameraTargetPosition{
        cameraPos.x - static_cast<float>(placement.origin.x),
        cameraPos.y - static_cast<float>(placement.origin.y),
        cameraPos.z - static_cast<float>(placement.origin.z),
    };
    AABB clipRegion;
    clipRegion.min = {
        std::min(0.0f, -(area.max.x - area.min.x)),
        std::min(0.0f, -(area.max.y - area.min.y)),
        std::min(0.0f, -(area.max.z - area.min.z))
    };
    clipRegion.max = {
        std::max(0.0f, area.max.x - area.min.x),
        std::max(0.0f, area.max.y - area.min.y),
        std::max(0.0f, area.max.z - area.min.z)
    };
    if (auto* impl = renderContext.mImpl.get(); impl != nullptr) {
        impl->mCameraTargetPosition.get() = cameraTargetPosition;
        impl->mWorldClipRegion.get()      = clipRegion;
    }

    auto matrixRef            = renderContext.mScreenContext.camera.worldMatrixStack->push(false);
    matrixRef.stack->_isDirty = true;

    matrixRef.mat->translate(glm::vec3{-cameraTargetPosition.x, -cameraTargetPosition.y, -cameraTargetPosition.z});


    if (auto* levelRendererPlayer = levelRenderer->mLevelRendererPlayer.get(); levelRendererPlayer != nullptr) {
        levelRendererPlayer->setupCamera(renderContext.mScreenContext.camera, renderContext.mFrameAlpha);
    }

    ++entry->renderTick;
    ++entry->entityDiagnosticFrame;
    Vec3 zeroOffset = {0.0f, 0.0f, 0.0f};
    _renderBlocks(renderContext, region, area, zeroOffset);

    if (!entry->pendingBuildChunk) {
        _renderBlockEntities(renderContext, *entry->world, area, cameraTargetPosition, false);
        _renderBlockEntities(renderContext, *entry->world, area, cameraTargetPosition, true);
    }
}

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

bool SchematicChunkRenderer::_initializeChunkBuilder(
    SchematicRendererInstanceData&           entry,
    ClientBlockPipeline::TessellatorContext& pipelineContext,
    MinecraftGameplayGraphicsResources&      graphicsResources,
    BlockSource&                             region,
    WeakRef<RenderChunkCoordinator>          coordinator,
    AABB const&                              buildBoundingBox
) {
    auto* schematicsRepo = pipelineContext.mSchematicsRepo.get().get();
    if (schematicsRepo == nullptr) {
        return false;
    }

    auto materialRepository = schematicsRepo->mMaterialRepository;
    if (!materialRepository) {
        return false;
    }

    entry.chunkBuilder =
        std::make_unique<RenderChunkBuilder>(graphicsResources, pipelineContext, region.getChunkSource(), coordinator);
    entry.chunkBuilder->mBuildBoundingBox = buildBoundingBox;
    return true;
}

SchematicChunkRenderer::SchematicRendererInstanceData*
SchematicChunkRenderer::_findEntryByRegion(BlockSource const& region) noexcept {
    for (auto& [id, entry] : mEntries) {
        if (entry && entry->world && entry->world->blockSource.get() == &region) {
            return entry.get();
        }
    }
    return nullptr;
}


bool SchematicChunkRenderer::_renderBlocks(
    BaseActorRenderContext& renderContext,
    BlockSource&            region,
    AABB const&             area,
    Vec3 const&             relativeOrigin
) {
    float const volume = (area.max.x - area.min.x) * (area.max.y - area.min.y) * (area.max.z - area.min.z);
    if (volume < 1.0f) {
        return false;
    }

    auto* entry = _findEntryByRegion(region);
    if (entry == nullptr || !entry->chunkBuilder) {
        return false;
    }

    auto textureList = RenderChunkGeometry::createTerrainTextureList(
        static_cast<SubClientId>(renderContext.mScreenContext.subClientId)
    );
    if (textureList.empty()) {
        logDiagnostic(entry->placementId, "renderBlocks aborted: terrain texture list is empty");
        return false;
    }

    if (!entry->chunkInstances.ready()) {
        logDiagnostic(entry->placementId, "renderBlocks aborted: chunk sorter unavailable");
        return false;
    }


    bool const layerMaterialsReady = mTerrainMaterials.get() != nullptr;
    if (!layerMaterialsReady) {
        logDiagnostic(
            entry->placementId,
            "renderBlocks: terrain material variation manager unavailable, layers not submitted"
        );
    }

    auto& bufferResourceService = renderContext.mScreenContext.bufferResourceService;
    auto& commandListQueue      = renderContext.mScreenContext.commandListQueue;


    Vec3 cameraTargetPosition{};
    if (auto const* impl = renderContext.mImpl.get(); impl != nullptr) {
        cameraTargetPosition = impl->mCameraTargetPosition.get();
    }


    Vec3 const sortCenter = cameraTargetPosition;

    auto const& origins      = entry->world->origins;
    std::size_t rebuiltCount = entry->completedBuildsThisFrame;
    std::size_t sortedCount  = 0;

    std::size_t pendingBuildCount = 0;


    std::size_t pendingResortCount = 0;

    std::size_t cameraResortCount = 0;


    std::size_t submittedLayerCount    = 0;
    std::size_t emptyLayerCount        = 0;
    std::size_t unavailableLayerCount  = 0;
    std::size_t submittedInstanceCount = 0;

    std::size_t noMeshDataCount = 0;

    std::string layerDetail;

    for (BlockPos const& chunkStart : origins) {
        Vec3 worldOrigin{
            static_cast<float>(entry->placementOrigin.x) + static_cast<float>(chunkStart.x),
            static_cast<float>(entry->placementOrigin.y) + static_cast<float>(chunkStart.y),
            static_cast<float>(entry->placementOrigin.z) + static_cast<float>(chunkStart.z)
        };
        Vec3 worldCamera{
            cameraTargetPosition.x + static_cast<float>(entry->placementOrigin.x),
            cameraTargetPosition.y + static_cast<float>(entry->placementOrigin.y),
            cameraTargetPosition.z + static_cast<float>(entry->placementOrigin.z)
        };
        auto chunkVisibility =
            testVisibility(worldCamera, subChunkBounds(worldOrigin), mConfig.visibility, mLevelRendererPlayer);
        if (chunkVisibility != VisibilityResult::Visible) continue;
        if (!entry->world->subChunks.at(util::encodeSubChunkKey(chunkStart)).built) continue;
        SchematicRenderChunkInstanced* instanced = entry->chunkInstances.obtain(chunkStart);
        if (instanced == nullptr) {
            continue;
        }

        if (entry->chunkInstances.needsRebuild(chunkStart) && entry->world->ready(chunkStart)) {
            if (entry->pendingBuildChunk || _pendingChunkBuildCount() >= mConfig.performance.queuedChunkBuilds
                || mFrameBuilds >= mConfig.performance.geometryBuildsPerFrame) {
                ++pendingBuildCount;
            } else {
                ++mFrameBuilds;
                if (entry->chunkInstances.prepareRebuild(*instanced, *entry->chunkBuilder)) {
                    entry->pendingBuildChunk = instanced;
                    auto* builder            = entry->chunkBuilder.get();
                    try {
                        entry->buildTask.start(
                            [this](auto work) { mChunkExecutor->execute(std::move(work)); },
                            [instanced, builder] { VanillaChunkInstanceSet::buildPrepared(*instanced, *builder); }
                        );
                    } catch (std::exception const& error) {
                        getLogger().error(
                            "SchematicChunkRenderer [placement #{}]: submitting chunk build failed: {}",
                            entry->placementId,
                            error.what()
                        );
                        entry->buildFailed = true;
                        return false;
                    }
                    ++pendingBuildCount;
                }
            }
        }
        if (entry->pendingBuildChunk != instanced && instanced->mRenderChunkShared
            && instanced->mRenderChunkShared->mCurrentRenderChunkGeometry
            && (instanced->mCurrentRenderChunkGeometry != instanced->mRenderChunkShared->mCurrentRenderChunkGeometry
                || entry->chunkInstances.cameraDeltaChanged(*instanced, sortCenter))) {


            if (mFrameResorts >= mConfig.performance.cameraResortsPerFrame) {
                ++pendingResortCount;
            } else {
                ++mFrameResorts;
                if (entry->chunkInstances.sort(*instanced, sortCenter, bufferResourceService)) {
                    ++sortedCount;
                    ++cameraResortCount;
                }
            }
        }

        if (!layerMaterialsReady) {
            continue;
        }


        SchematicChunkRenderData chunkRenderData;
        if (!makeChunkRenderData(*instanced, chunkRenderData)) {
            ++noMeshDataCount;
            continue;
        }


        writeChunkOriginAndScale(renderContext, chunkStart, relativeOrigin);

        auto const stats =
            submitChunkLayers(*instanced, chunkRenderData, textureList, commandListQueue, mTerrainMaterials);
        ++submittedInstanceCount;
        submittedLayerCount   += stats.submitted;
        emptyLayerCount       += stats.empty;
        unavailableLayerCount += stats.unavailable;

        if (layerDetail.size() < 320) {
            for (std::size_t layer = 0; layer < stats.ranges.size(); ++layer) {
                auto const& range = stats.ranges[layer];
                if (range.mCount == 0) {
                    continue;
                }
                layerDetail += std::string{" "} + chunkLayerName(layer) + "(start=" + std::to_string(range.mStart)
                             + ",count=" + std::to_string(range.mCount) + ")";
            }
        }
    }

    entry->lastSortedInstanceCount = sortedCount;
    entry->lastSubmittedLayerCount = submittedLayerCount;

    if (rebuiltCount != 0 || pendingBuildCount != 0 || cameraResortCount != 0 || pendingResortCount != 0) {
        getLogger().debug(
            "SchematicChunkRenderer: rebuilt {} chunk instance(s) this frame (pending {}), sorted {} "
            "(cameraReSort={}, pendingReSort={}); sortCamera {:.2f}/{:.2f}/{:.2f}",
            rebuiltCount,
            pendingBuildCount,
            sortedCount,
            cameraResortCount,
            pendingResortCount,
            sortCenter.x,
            sortCenter.y,
            sortCenter.z
        );
    }


    logDiagnostic(
        entry->placementId,
        "layers: submitted=" + std::to_string(submittedLayerCount) + ", empty=" + std::to_string(emptyLayerCount)
            + ", unavailable=" + std::to_string(unavailableLayerCount) + ", instances="
            + std::to_string(submittedInstanceCount) + ", noMesh=" + std::to_string(noMeshDataCount) + ", pendingBuild="
            + std::to_string(pendingBuildCount) + ", pendingReSort=" + std::to_string(pendingResortCount)
            + "; nonEmptyRanges:" + (layerDetail.empty() ? std::string{" (none)"} : layerDetail)
    );

    return !origins.empty() && entry->chunkInstances.instanceCount() >= origins.size();
}

} 
