#pragma once

#include "levischematic/app/Config.h"
#include "levischematic/render/ChunkCpuTask.h"
#include "levischematic/render/BlockEntityHighlight.h"
#include "levischematic/render/RenderVisibility.h"
#include "levischematic/render/SchematicChunkLayerSubmit.h"
#include "levischematic/render/ValidationHullRenderer.h"
#include "levischematic/schematic/placement/PlacementModel.h"
#include "levischematic/util/WorkBudget.h"

#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"

#include <cstddef>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ll::thread { class ThreadPoolExecutor; }

class AABB;
class BaseActorRenderContext;
class BlockSource;
class RenderChunkBuilder;
class RenderChunkCoordinator;
class Tessellator;
class MinecraftGameplayGraphicsResources;
class Vec3;
class ScreenContext;
class Level;
class LevelRenderer;
class LevelRendererPlayer;
class Dimension;

namespace levischematic::app {
class PlacementService;
}

template <typename T>
class WeakRef;

namespace ClientBlockPipeline {
class TessellatorContext;
}

namespace levischematic::render {

struct FakeWorld;












class SchematicChunkRenderer {
public:
    bool prepareEntityComposite();
    
    bool withEntityBlockSource(placement::PlacementId, uint64_t contentRevision,
        std::function<void(BlockSource&, BlockPos const&)> const&);
    



    struct DebugStats {
        placement::PlacementId placementId             = 0;
        bool                   hasWorld                = false;
        bool                   hasChunkBuilder         = false;
        bool                   hasChunkSorter          = false;
        bool                   instanceCacheReady      = false;
        std::size_t            chunkInstanceCount      = 0;
        std::size_t            lastSortedInstanceCount = 0;
        
        std::size_t lastSubmittedLayerCount = 0;
        BlockEntityHighlightStats blockEntities;
        uint64_t    revision                = 0;
        uint64_t    renderTick              = 0;
        int         dimensionId             = -1;
        std::size_t pendingBuildCount       = 0;
        bool buildFailed = false;
        std::vector<std::array<int, 3>> expectedOrigins, submittedOrigins;
    };

    


    SchematicChunkRenderer();
    


    ~SchematicChunkRenderer();

    



    SchematicChunkRenderer(SchematicChunkRenderer const&)            = delete;
    



    SchematicChunkRenderer& operator=(SchematicChunkRenderer const&) = delete;

    
    




    void renderFrame(ScreenContext& screenContext, app::PlacementService const& placements);

    
    void renderValidationHull(
        ScreenContext&,
        Vec3 const&,
        app::PlacementService const&,
        int dimension,
        selection::SelectionOverlayView const&
    );
    
    bool hasValidationHull(app::PlacementService const&, BlockPos const&, int dimension) const;
    
    void applyConfig(app::Config const& config);

    


    void clear();
    



    void suspend(bool releaseMaterials = false);
    


    void resume();
    


    void resetMaterialResources();

    


    [[nodiscard]] std::size_t             entryCount() const;
    


    [[nodiscard]] std::vector<DebugStats> debugStats() const;

private:
    struct SchematicRendererInstanceData;

    


    void clearUnlocked();
    





    void render(ScreenContext& screenContext, placement::PlacementInstance const& placement, int currentDimensionId,
                app::PlacementService const& placements, util::WorkBudget const& budget);

    




    void logDiagnostic(placement::PlacementId id, std::string const& reason);

    
    
    


    bool _ensureOverlayTexture();

    




    void _pollChunkTask(SchematicRendererInstanceData& entry, ScreenContext& screenContext, bool publish = true);
    


    [[nodiscard]] std::size_t _pendingChunkBuildCount() const;

    
    
    








    bool _initializeChunkBuilder(
        SchematicRendererInstanceData&           entry,
        ClientBlockPipeline::TessellatorContext& pipelineContext,
        MinecraftGameplayGraphicsResources&      graphicsResources,
        BlockSource&                             region,
        WeakRef<RenderChunkCoordinator>          coordinator,
        AABB const&                              buildBoundingBox
    );

    
    
    
    
    
    
    
    
    






    bool _renderBlocks(
        BaseActorRenderContext& renderContext,
        BlockSource&            region,
        AABB const&             area,
        Vec3 const&             relativeOrigin
    );

    
    







    std::size_t _renderBlockEntities(
        BaseActorRenderContext& renderContext,
        FakeWorld const&        world,
        AABB const&             area,
        Vec3 const&             cameraTargetPosition,
        bool                    renderAlphaLayer
    );



    



    [[nodiscard]] SchematicRendererInstanceData* _findEntryByRegion(BlockSource const& region) noexcept;

    
    std::unique_ptr<ll::thread::ThreadPoolExecutor> mChunkExecutor;
    std::unordered_map<placement::PlacementId, std::unique_ptr<SchematicRendererInstanceData>> mEntries;
    
    std::vector<std::unique_ptr<SchematicRendererInstanceData>> mRetiredEntries;
    std::unordered_map<placement::PlacementId, std::string>                                    mDiagnosticReasons;

    ValidationHullRenderer mValidationHullRenderer;
    app::Config mConfig;
    size_t mPrepareStart = 0;
    size_t mFrameBuilds = 0, mFrameResorts = 0;

    std::vector<placement::PlacementId> mVisiblePlacementIds;
    uint64_t                            mPlacementsRevision     = 0;
    bool                                mVisiblePlacementsValid = false;

    mce::TexturePtr                                             overlayTexturePtr;
    mce::ServerResourcePointer<dragon::ResolvedTextureResource> overlayTexture;
    
    Level const*                            mLevel               = nullptr;
    LevelRenderer const*                    mLevelRenderer       = nullptr;
    LevelRendererPlayer const*              mLevelRendererPlayer = nullptr;
    Dimension const*                        mDimension           = nullptr;
    bool                                    mSuspended           = true;
    TerrainMaterialVariationManagerResource mTerrainMaterials;
    
    
    
    mutable std::mutex mMutex;
};

} 
