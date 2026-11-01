#include "SchematicChunkLayerSubmit.h"

#include "levischematic/LeviSchematic.h"
#include "levischematic/render/NativeTextureVariant.h"
#include "levischematic/render/SchematicRenderChunkShared.h"

#include "ll/api/memory/Memory.h"

#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/chunks/RenderChunkGeometry.h"
#include "mc/client/renderer/chunks/RenderChunkSorter.h"
#include "mc/deps/core/utility/ServiceLocator.h"
#include "mc/deps/core/utility/buffer_span.h"
#include "mc/deps/minecraft_renderer/constantbuffers/GlobalConstantBuffers.h"
#include "mc/deps/minecraft_renderer/constantbuffers/PerFrameConstants.h"
#include "mc/deps/minecraft_renderer/constantbuffers/RenderChunkConstants.h"
#include "mc/deps/minecraft_renderer/framebuilder/FrameBuilder.h"
#include "mc/deps/minecraft_renderer/framebuilder/RenderChunkGeometryDescription.h"
#include "mc/deps/minecraft_renderer/game/RenderChunkDirectIndexData.h"
#include "mc/deps/minecraft_renderer/game/RenderChunkDirectVertexData.h"
#include "mc/deps/minecraft_renderer/game/TerrainLayers.h"

#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"

#include <array>
#include <cstddef>
#include <functional>
#include <glm/glm.hpp>
#include <gsl/span>
#include <string_view>

namespace levischematic::render {
namespace {

using ResolvedTexturePointer = mce::ServerResourcePointer<dragon::ResolvedTextureResource>;


struct NativePerChunkData {
    RenderChunkDirectIndexData const*  mDirectIndexData{};
    RenderChunkDirectVertexData const* mDirectVertexData{};
    bool                               mUnsorted{};
    std::uint32_t                      mIndicesOffset{};
    std::uint32_t                      mIndicesCount{};
    glm::vec4                          mChunkOriginAndScale{};
    float                              mRenderChunkFogAlpha{};
    std::uint32_t                      mVisibleCameraMask{};
};


struct NativeChunkTextures {
    std::array<ResolvedTexturePointer, 3> mArray{};
    std::size_t                           mSize = 3;
};


struct NativeChunkFog {
    glm::vec4 mColor{};
    glm::vec2 mControl{};
    float     mRenderDistance{};
};


struct NativeChunkGeometryDescription {
    NativeChunkTextures                 mTextures;
    gsl::span<NativePerChunkData const> mChunks;
    TerrainLayer const*                 mTerrainLayer{};
    TerrainLayerLOD                     mTerrainLayerLOD = TerrainLayerLOD::High;
    NativeChunkFog                      mFog;
    float                               mFarChunkDistance{};
    float                               mTime{};
    glm::mat4                           mWorldAdjustmentMatrix{1.0f};
    bool                                mIsUI = false;
    OffscreenCaptureDescription         mOffscreenCaptureDescription{};
    bool                                mIsHighlighted    = true;
    bool                                mDitheringEnabled = true;
    std::uint16_t                       mViewId           = 0;
    glm::vec3                           mCameraPosition{};
};


constexpr std::ptrdiff_t kShaderConstantDataOffset   = 0x40;
constexpr std::ptrdiff_t kShaderConstantDirtyOffset  = 0x31;
constexpr std::size_t    kRenderLayerIndexRangeCount = 22;
constexpr std::size_t    kGeometryFlagRangeQueryIndex =
    static_cast<std::size_t>(::RenderChunkGeometry::Bit::BlendCanRenderAsOpaque);


struct TerrainLayerEntry {
    ::TerrainLayer const& (*get)();
    std::string_view name;
};

TerrainLayerEntry const kTerrainLayerSequence[] = {
    {  +


  []() -> ::TerrainLayer const& { return ::TerrainLayers::OpaqueSeasons(); },
     "OpaqueSeasons"  },
    {         +


         []() -> ::TerrainLayer const& { return ::TerrainLayers::Opaque(); },
     "Opaque"         },
    {      +


      []() -> ::TerrainLayer const& { return ::TerrainLayers::EndPortal(); },
     "EndPortal"      },
    {        +


        []() -> ::TerrainLayer const& { return ::TerrainLayers::Barrier(); },
     "Barrier"        },
    {  +


  []() -> ::TerrainLayer const& { return ::TerrainLayers::StructureVoid(); },
     "StructureVoid"  },
    {     +


     []() -> ::TerrainLayer const& { return ::TerrainLayers::DoubleSide(); },
     "DoubleSide"     },
    {          +


          []() -> ::TerrainLayer const& { return ::TerrainLayers::Alpha(); },
     "Alpha"          },
    {+


[]() -> ::TerrainLayer const& { return ::TerrainLayers::AlphaSingleSide(); },
     "AlphaSingleSide"},
    {   +


   []() -> ::TerrainLayer const& { return ::TerrainLayers::AlphaSeasons(); },
     "AlphaSeasons"   },
    {          +


          []() -> ::TerrainLayer const& { return ::TerrainLayers::Blend(); },
     "Blend"          },
};


[[nodiscard]] SchematicRangeIndices const*
selectRanges(SchematicRenderChunkInstanced const& instanced, RenderChunkGeometry const& geometry) noexcept {
    if (instanced.mFallbackToUnsorted) {
        return geometry.mRenderLayerIndexRanges->data();
    }
    return instanced.mCurrentIndexRanges ? instanced.mCurrentIndexRanges->data() : nullptr;
}


void logUnavailable(char const* reason) {
    static std::string_view last;
    if (last == reason) return;
    last = reason;
    LeviSchematic::getInstance().getSelf().getLogger().warn("SchematicChunkLayerSubmit unavailable: {}", reason);
}


void logOriginWriteSkipped(char const* reason) {
    static std::string_view last;
    if (last == reason) return;
    last = reason;
    LeviSchematic::getInstance().getSelf().getLogger().warn(
        "SchematicChunkLayerSubmit: CHUNK_ORIGIN_AND_SCALE was not written ({})",
        reason
    );
}


template <typename T>
bool readShaderConstant(void const* constant, T& value) {
    if (constant == nullptr) return false;
    auto* data = ll::memory::dAccess<T const*>(constant, kShaderConstantDataOffset);
    if (data == nullptr) return false;
    value = *data;
    return true;
}


bool submitChunkLayer(
    SchematicRenderChunkInstanced const&   instanced,
    SchematicChunkRenderData const&        renderData,
    TerrainLayer const&                    layer,
    TerrainLayerLOD                        lod,
    RangeIndices                           suppliedRange,
    std::vector<mce::ServerTexture> const& textures,
    CommandListTaskContext const&          context,
    TerrainMaterialVariationManager const& materials
) {
    (void)materials.getMaterial(layer, lod);

    bool const  fallback = instanced.mFallbackToUnsorted && layer.transparency != TerrainLayer::Transparency::Sorted;
    auto const* indexVariant =
        std::get_if<std::shared_ptr<RenderChunkDirectIndexData>>(&renderData.mChunkIndices.get());
    auto const* vertexVariant = std::get_if<std::shared_ptr<RenderChunkDirectVertexData>>(&renderData.mChunkMesh.get());
    if (indexVariant == nullptr || vertexVariant == nullptr || !*vertexVariant) {
        logUnavailable("chunk data is not DirectIndexData/DirectVertexData");
        return false;
    }

    auto const* indexData    = indexVariant->get();
    bool const  hasIndexData = indexData != nullptr && indexData->mIndexData->has_value();
    if (!fallback && !hasIndexData) return false;

    auto frameBuilder = ServiceLocator<mce::framebuilder::FrameBuilder>::get();
    if (!frameBuilder || !frameBuilder.get()->enabled()) return false;


    NativePerChunkData chunk;
    chunk.mDirectIndexData  = indexData;
    chunk.mDirectVertexData = vertexVariant->get();
    chunk.mUnsorted         = fallback;
    chunk.mIndicesOffset    = suppliedRange.mStart;
    chunk.mIndicesCount     = suppliedRange.mCount;

    NativeChunkGeometryDescription description;
    description.mChunks          = {&chunk, 1};
    description.mTerrainLayer    = &layer;
    description.mTerrainLayerLOD = lod;

    auto const& chunkConstants = context.mRenderChunkConstantBuffer;
    auto const& frameConstants = context.mThreadedPerFrameConstants;
    if (!readShaderConstant(chunkConstants.CHUNK_ORIGIN_AND_SCALE, chunk.mChunkOriginAndScale)
        || !readShaderConstant(chunkConstants.RENDER_CHUNK_FOG_ALPHA, chunk.mRenderChunkFogAlpha)
        || !readShaderConstant(frameConstants.FOG_COLOR, description.mFog.mColor)
        || !readShaderConstant(frameConstants.FOG_CONTROL, description.mFog.mControl)
        || !readShaderConstant(frameConstants.RENDER_DISTANCE, description.mFog.mRenderDistance)
        || !readShaderConstant(frameConstants.FAR_CHUNKS_DISTANCE, description.mFarChunkDistance)
        || !readShaderConstant(frameConstants.TIME, description.mTime)
        || !readShaderConstant(frameConstants.VIEW_POS, description.mCameraPosition)) {
        logUnavailable("shader constants are unavailable");
        return false;
    }


    bool const endPortal = layer.id == TerrainLayers::EndPortal().id;
    if (textures.size() < 5) return false;
    description.mTextures.mArray = {
        detail::unwrapServerTexture(textures[endPortal ? 2 : 4]),
        detail::unwrapServerTexture(textures[endPortal ? 1 : 0]),
        detail::unwrapServerTexture(textures[3]),
    };

    auto const& sdkDescription =
        reinterpret_cast<mce::framebuilder::RenderChunkGeometryDescription const&>(description);
    frameBuilder.get()->_insert(std::cref(sdkDescription));
    return true;
}

} 

char const* chunkLayerName(std::size_t index) noexcept {
    return index < std::size(kTerrainLayerSequence) ? kTerrainLayerSequence[index].name.data() : "?";
}

void writeChunkOriginAndScale(
    ::BaseActorRenderContext& renderContext,
    ::BlockPos const&         chunkOrigin,
    ::Vec3 const&             relativeOrigin
) {
    auto* constant = renderContext.mScreenContext.constantBuffers.renderChunkConstantBuffer->CHUNK_ORIGIN_AND_SCALE;
    if (constant == nullptr) {
        logOriginWriteSkipped("render chunk constant is null");
        return;
    }

    auto* data = ll::memory::dAccess<float*>(constant, kShaderConstantDataOffset);
    if (data == nullptr) {
        logOriginWriteSkipped("constant data is null");
        return;
    }

    auto const* impl = renderContext.mImpl.get();
    if (impl == nullptr) {
        logOriginWriteSkipped("render context has no implementation");
        return;
    }
    auto const& camera = impl->mCameraTargetPosition.get();
    data[0]            = static_cast<float>(chunkOrigin.x) + relativeOrigin.x - camera.x;
    data[1]            = static_cast<float>(chunkOrigin.y) + relativeOrigin.y - camera.y;
    data[2]            = static_cast<float>(chunkOrigin.z) + relativeOrigin.z - camera.z;
    data[3]            = 1.0f;
    ll::memory::dAccess<uchar>(constant, kShaderConstantDirtyOffset) = 1;
}

bool TerrainMaterialVariationManagerResource::ensure() {
    if (mInstance) return true;
    std::string                requestedVariation{"lit"};
    ::buffer_span<std::string> variations;
    variations.mBegin = &requestedVariation;
    variations.mEnd   = &requestedVariation + 1;
    mInstance = std::make_unique<::TerrainMaterialVariationManager>(mce::RenderMaterialGroup::switchable(), variations);
    return true;
}

void TerrainMaterialVariationManagerResource::reset() noexcept { mInstance.reset(); }

bool makeChunkRenderData(SchematicRenderChunkInstanced const& instanced, SchematicChunkRenderData& out) {
    auto const& shared = instanced.mRenderChunkShared;
    if (!shared) return false;
    auto const* geometry = shared->mCurrentRenderChunkGeometry.get();
    if (geometry == nullptr || geometry->mMeshData.get().index() == 0) return false;

    auto const& position = shared->mPosition.get();
    out.mPosition.get()  = {position.x, position.y, position.z};
    out.mReadyTimeDiff   = static_cast<double>(geometry->mReadyTime);
    out.mChunkIndices    = instanced.mCurrentIndices.get();
    out.mChunkMesh       = geometry->mMeshData.get();
    return true;
}

ChunkLayerSubmitStats submitChunkLayers(
    SchematicRenderChunkInstanced&                 instanced,
    SchematicChunkRenderData const&                chunkRenderData,
    std::vector<::mce::ServerTexture> const&       textures,
    ::CommandListQueue const&                      commandListQueue,
    TerrainMaterialVariationManagerResource const& materials
) {
    ChunkLayerSubmitStats stats;
    auto const*           geometry =
        instanced.mRenderChunkShared ? instanced.mRenderChunkShared->mCurrentRenderChunkGeometry.get() : nullptr;
    auto* terrainMaterials = materials.get();
    if (terrainMaterials == nullptr) {
        stats.unavailable = kChunkLayerCount;
        logUnavailable("terrain material manager is unavailable");
        return stats;
    }
    if (geometry == nullptr) {
        stats.unavailable = kChunkLayerCount;
        logUnavailable("render chunk geometry is unavailable");
        return stats;
    }

    auto const* ranges = selectRanges(instanced, *geometry);
    if (ranges == nullptr) {
        stats.unavailable = kChunkLayerCount;
        logUnavailable("index ranges are unavailable");
        return stats;
    }
    buffer_span<SchematicRangeIndices> rangeSpan;
    rangeSpan.mBegin          = ranges;
    rangeSpan.mEnd            = ranges + kRenderLayerIndexRangeCount;
    bool const rangeQueryFlag = geometry->mFlags.get().test(kGeometryFlagRangeQueryIndex);
    auto const lod            = TerrainLayerLOD::High;

    for (std::size_t i = 0; i < kChunkLayerCount; ++i) {
        auto const& layer = kTerrainLayerSequence[i].get();
        stats.ranges[i]   = RenderChunkSorter::getRangeIndices(rangeSpan, layer, lod, rangeQueryFlag);
        if (stats.ranges[i].mCount == 0) {
            ++stats.empty;
            continue;
        }
        if (submitChunkLayer(
                instanced,
                chunkRenderData,
                layer,
                lod,
                stats.ranges[i],
                textures,
                commandListQueue.mGlobalContext.get(),
                *terrainMaterials
            )) {
            ++stats.submitted;
        } else {
            ++stats.unavailable;
        }
    }
    return stats;
}

} 
