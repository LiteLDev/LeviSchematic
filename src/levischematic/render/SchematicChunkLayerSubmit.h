#pragma once

#include "levischematic/render/SchematicRenderChunkInstanced.h"

#include "mc/client/renderer/game/TerrainMaterialVariationManager.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/minecraft_renderer/commands/CommandListQueue.h"
#include "mc/deps/minecraft_renderer/game/TerrainLayer.h"
#include "mc/deps/minecraft_renderer/game/TerrainLayerLOD.h"
#include "mc/deps/minecraft_renderer/objects/ChunkRenderData.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/world/level/BlockPos.h"

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

class BaseActorRenderContext;

namespace levischematic::render {





class TerrainMaterialVariationManagerResource {
public:
    


    TerrainMaterialVariationManagerResource()                                                          = default;
    



    TerrainMaterialVariationManagerResource(TerrainMaterialVariationManagerResource const&)            = delete;
    



    TerrainMaterialVariationManagerResource& operator=(TerrainMaterialVariationManagerResource const&) = delete;
    


    ~TerrainMaterialVariationManagerResource() { reset(); }

    


    [[nodiscard]] bool                               ensure();
    


    void                                             reset() noexcept;
    


    [[nodiscard]] ::TerrainMaterialVariationManager* get() const noexcept { return mInstance.get(); }

private:
    std::unique_ptr<::TerrainMaterialVariationManager> mInstance;
};

using SchematicRangeIndices    = ::RangeIndices;
using SchematicChunkRenderData = ::ChunkRenderData;

inline constexpr std::size_t kChunkLayerCount = 17;





[[nodiscard]] char const* chunkLayerName(std::size_t index) noexcept;







void writeChunkOriginAndScale(
    ::BaseActorRenderContext& renderContext,
    ::BlockPos const&         chunkOrigin,
    ::Vec3 const&             relativeOrigin
);





struct ChunkLayerSubmitStats {
    std::size_t                                         submitted   = 0;
    std::size_t                                         empty       = 0;
    std::size_t                                         unavailable = 0;
    std::array<SchematicRangeIndices, kChunkLayerCount> ranges{};
};









[[nodiscard]] ChunkLayerSubmitStats submitChunkLayers(
    SchematicRenderChunkInstanced&                 instanced,
    SchematicChunkRenderData const&                chunkRenderData,
    std::vector<::mce::ServerTexture> const&       textures,
    ::CommandListQueue const&                      commandListQueue,
    TerrainMaterialVariationManagerResource const& materials
);






[[nodiscard]] bool makeChunkRenderData(SchematicRenderChunkInstanced const& instanced, SchematicChunkRenderData& out);

} 
