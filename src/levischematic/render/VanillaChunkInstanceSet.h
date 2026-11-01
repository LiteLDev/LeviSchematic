#pragma once

#include "SchematicRenderChunkInstanced.h"
#include "SchematicRenderChunkSorter.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/minecraft_renderer/framebuilder/FrameLightingModelCapabilities.h"
#include "mc/world/level/BlockPos.h"

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <unordered_set>

class RenderChunkBuilder;

namespace mce {
struct BufferResourceService;
}

namespace levischematic::render {















class VanillaChunkInstanceSet {
public:
    


    VanillaChunkInstanceSet() = default;
    


    ~VanillaChunkInstanceSet() { clear(); }

    



    VanillaChunkInstanceSet(VanillaChunkInstanceSet const&)            = delete;
    



    VanillaChunkInstanceSet& operator=(VanillaChunkInstanceSet const&) = delete;

    
    


    void reset();

    
    


    void clear();
    
    void invalidate(BlockPos const& origin);
    [[nodiscard]] bool needsRebuild(BlockPos const& origin) const;

    
    


    [[nodiscard]] bool ready() const noexcept;

    
    



    [[nodiscard]] SchematicRenderChunkInstanced* obtain(::BlockPos const& chunkOrigin);

    
    
    







    [[nodiscard]] bool prepareRebuild(SchematicRenderChunkInstanced& instanced, RenderChunkBuilder& builder);
    




    static void buildPrepared(SchematicRenderChunkInstanced& instanced, RenderChunkBuilder& builder);
    





    void finishRebuild(
        SchematicRenderChunkInstanced& instanced,
        RenderChunkBuilder& builder,
        ::mce::BufferResourceService& bufferResourceService
    );

    




    [[nodiscard]] bool prepareSort(SchematicRenderChunkInstanced& instanced, ::Vec3 const& sortCamera);
    



    void sortPrepared(SchematicRenderChunkInstanced& instanced);
    







    void finishSort(
        SchematicRenderChunkInstanced& instanced,
        ::mce::BufferResourceService&  bufferResourceService
    );
    





    [[nodiscard]] bool sort(
        SchematicRenderChunkInstanced& instanced,
        ::Vec3 const& sortCamera,
        ::mce::BufferResourceService& bufferResourceService
    );

    
    
    




    [[nodiscard]] bool cameraDeltaChanged(
        SchematicRenderChunkInstanced const& instanced,
        ::Vec3 const&                         sortCamera
    ) const noexcept;

    


    [[nodiscard]] std::size_t instanceCount() const noexcept { return mInstances.size(); }
    


    [[nodiscard]] bool        hasSorter() const noexcept { return static_cast<bool>(mSorter); }

private:
    


    [[nodiscard]] bool createSorter();

    std::unordered_map<::BlockPos, SchematicRenderChunkInstancedPtr> mInstances;
    std::unordered_set<::BlockPos> mDirty;
    SchematicRenderChunkSorterPtr                                    mSorter;
};

} 
