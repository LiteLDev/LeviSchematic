#include "SchematicChunkLayerSubmit.h"
#include "BlockEntityHighlight.h"

#include "levischematic/LeviSchematic.h"
#include "levischematic/render/NativeChunkGeometryDescription.h"
#include "levischematic/render/NativeTextureVariant.h"
#include "levischematic/render/SchematicRenderChunkShared.h"

#include "ll/api/memory/Memory.h"
#include "ll/api/memory/Symbol.h"

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
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <glm/glm.hpp>
#include <gsl/span>
#include <limits>
#include <string_view>

namespace levischematic::render {
namespace {

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
    {+
    


    []() -> ::TerrainLayer const& { return ::TerrainLayers::OpaqueSeasons(); },   "OpaqueSeasons"  },
    {+
    


    []() -> ::TerrainLayer const& { return ::TerrainLayers::Opaque(); },          "Opaque"         },
    {+
    


    []() -> ::TerrainLayer const& { return ::TerrainLayers::EndPortal(); },       "EndPortal"      },
    {+
    


    []() -> ::TerrainLayer const& { return ::TerrainLayers::Barrier(); },         "Barrier"        },
    {+
    


    []() -> ::TerrainLayer const& { return ::TerrainLayers::StructureVoid(); },   "StructureVoid"  },
    {+
    


    []() -> ::TerrainLayer const& { return ::TerrainLayers::DoubleSide(); },      "DoubleSide"     },
    {+
    


    []() -> ::TerrainLayer const& { return ::TerrainLayers::Alpha(); },           "Alpha"          },
    {+
    


    []() -> ::TerrainLayer const& { return ::TerrainLayers::AlphaSingleSide(); }, "AlphaSingleSide"},
    {+
    


    []() -> ::TerrainLayer const& { return ::TerrainLayers::AlphaSeasons(); },    "AlphaSeasons"   },
    {+
    


    []() -> ::TerrainLayer const& { return ::TerrainLayers::Blend(); },           "Blend"          },
};



struct ExtraLayer { char const* symbol; char const* name; };
constexpr ExtraLayer kExtraLayers[] = {
    {"?Light@TerrainLayers@@3UTerrainLayer@@B", "Light"},
    {"?LightSource@TerrainLayers@@3UTerrainLayer@@B", "LightSource"},
    {"?OpaqueShifts@TerrainLayers@@3UTerrainLayer@@B", "OpaqueShifts"},
    {"?AlphaShifts@TerrainLayers@@3UTerrainLayer@@B", "AlphaShifts"},
    {"?AlphaSingleSideShifts@TerrainLayers@@3UTerrainLayer@@B", "AlphaSingleSideShifts"},
    {"?DeferredWater@TerrainLayers@@3UTerrainLayer@@B", "DeferredWater"},
    {"?RayTracedWater@TerrainLayers@@3UTerrainLayer@@B", "RayTracedWater"}
};
static_assert(std::size(kTerrainLayerSequence) + std::size(kExtraLayers) == kChunkLayerCount);




auto nativeExtraLayers() {
    std::array<TerrainLayer const*, std::size(kExtraLayers)> result{};
    auto const instruction = resolveProjectionEntry("",
        "48 8D 05 ? ? ? ? 49 8B 3C 04");
    if (!instruction) return result;
    std::int32_t displacement;
    std::memcpy(&displacement, reinterpret_cast<void const*>(instruction + 3), sizeof(displacement));
    auto const* layers = reinterpret_cast<std::array<TerrainLayer const*, 17> const*>(
        instruction + 7 + displacement);
    
    if (std::find(layers->begin(), layers->end(), &TerrainLayers::Opaque()) == layers->end()) return result;
    constexpr std::string_view names[]{"light", "light_source", "terrain_opaque_shifts", "terrain_alpha_shifts",
        "terrain_alpha_single_side_shifts", "terrain_deferred_water", "terrain_ray_traced_water"};
    for (size_t i = 0; i < result.size(); ++i) {
        for (auto* layer : *layers)
            if (layer && layer->name.get() == names[i]) result[i] = layer;
    }
    return result;
}





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

    
    
    auto const& vertexData = **vertexVariant;
    if (vertexData.mVertexCount > std::numeric_limits<std::uint32_t>::max()) {
        logUnavailable("vertex count exceeds the native mesh filter limit");
        return false;
    }
    auto const& metadata = vertexData.mRenderMetadata.get();
    NativePerChunkData chunk;
    chunk.mBufferData = NativeDirectBuffers{
        &vertexData,
        indexData,
        metadata ? &*metadata : nullptr,
        vertexData.mVertexCount,
    };
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

    
    
    
    description.mDitheringEnabled = true;
    if (frameBuilder.get()->isDeferredEnabled()) {
        bool const cutout = layer.name.get().find("alpha") != std::string_view::npos
            || layer.id == TerrainLayers::DoubleSide().id || layer.name.get() == "light"
            || layer.name.get() == "light_source" || layer.id == TerrainLayers::Barrier().id;
        if (!cutout) chunk.mRenderChunkFogAlpha = 1.0f;
        description.mDitheringEnabled = false;
        
        
        
        
        
        if (layer.id == TerrainLayers::Barrier().id || layer.name.get() == "light_source"
            || layer.name.get() == "light")
            description.mTerrainLayer = &TerrainLayers::Alpha();
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
    if (index < std::size(kTerrainLayerSequence)) return kTerrainLayerSequence[index].name.data();
    index -= std::size(kTerrainLayerSequence);
    return index < std::size(kExtraLayers) ? kExtraLayers[index].name : "?";
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
    static auto const extraLayers = [] {
        auto result = nativeExtraLayers();
        for (size_t i = 0; i < result.size(); ++i)
            if (!result[i])
                result[i] = static_cast<TerrainLayer const*>(ll::memory::SymbolView{kExtraLayers[i].symbol}.resolve(true));
        return result;
    }();

    for (std::size_t i = 0; i < kChunkLayerCount; ++i) {
        auto layerPtr = i < std::size(kTerrainLayerSequence) ? &kTerrainLayerSequence[i].get()
            : extraLayers[i - std::size(kTerrainLayerSequence)];
        if (!layerPtr) { ++stats.unavailable; continue; }
        auto const& layer = *layerPtr;
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
