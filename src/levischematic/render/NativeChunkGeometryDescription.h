#pragma once

#include "mc/deps/minecraft_renderer/game/TerrainLayerLOD.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/external/render_dragon/resources/ServerResourcePointer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <gsl/span>
#include <variant>

struct RenderChunkDirectIndexData;
struct RenderChunkDirectVertexData;
struct RenderChunkLayerGroupIndexData;
struct RenderChunkLayerGroupVertexData;
struct TerrainLayer;
namespace dragon {
struct RenderMetadata;
}

namespace levischematic::render {




struct NativeDirectBuffers {
    RenderChunkDirectVertexData const* mVertexData{};
    RenderChunkDirectIndexData const*  mIndexData{};
    dragon::RenderMetadata const*      mRenderMetadata{};
    std::uint64_t                      mVertexCount{};
};

struct NativeLayerGroupBuffers {
    RenderChunkLayerGroupVertexData const* mVertexData{};
    RenderChunkLayerGroupIndexData const*  mIndexData{};
    std::uint8_t                           mGroupIdx{};
    dragon::RenderMetadata const*          mRenderMetadata{};
    std::uint64_t                          mVertexCount{};
};






struct NativePerChunkData {
    std::variant<NativeDirectBuffers, NativeLayerGroupBuffers> mBufferData;
    bool                                                       mUnsorted{};
    std::uint32_t                                              mIndicesOffset{};
    std::uint32_t                                              mIndicesCount{};
    glm::vec4                                                  mChunkOriginAndScale{};
    float                                                      mRenderChunkFogAlpha{};
    std::uint32_t                                              mVisibleCameraMask{};
};





struct NativeChunkTextures {
    std::array<mce::ServerResourcePointer<dragon::ResolvedTextureResource>, 3> mArray{};
    std::size_t                                                                mSize = 3;
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
    bool                                mDitheringEnabled = false;
    std::uint16_t                       mViewId           = 0;
    glm::vec3                           mCameraPosition{};
};

} 
