#pragma once

#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/math/Vec4.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/world/level/BlockPos.h"

#include "levischematic/render/highlight/HullCacheState.h"
#include "levischematic/shared/geometry/PositionUtils.h"
#include "levischematic/verifier/model/VerifierTypes.h"
#include "levischematic/workspace/config/Config.h"
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

class LevelRenderer;
class ScreenContext;
class Tessellator;
class Vec3;

namespace levischematic::app {
class PlacementService;
}

namespace levischematic::selection {
struct SelectionOverlayView;
}

namespace levischematic::render {

class ValidationHullRenderer {
public:
    using TextureVariant = std::variant<std::monostate, mce::TexturePtr, mce::ClientTexture, mce::ServerTexture>;

    void renderPass(
        ScreenContext&,
        Vec3 const&,
        app::PlacementService const&,
        int dimension,
        app::Config const&,
        selection::SelectionOverlayView const&
    );
    bool contains(app::PlacementService const&, BlockPos const&, int dimension, app::Config const&) const;
    void clear() noexcept;

private:
    struct Vertex {
        float      x = 0.0f;
        float      y = 0.0f;
        float      z = 0.0f;
        glm::vec4  normal{0.0f, 0.0f, 1.0f, 0.0f};
        mce::Color color;
    };

    void appendCube(
        std::vector<Vertex>& vertices,
        std::vector<Vertex>& edges,
        BlockPos const&      position,
        Block const*         block,
        mce::Color           color
    );
    static void appendBox(std::vector<Vertex>&, Vec3 const& minimum, Vec3 const& maximum, mce::Color);
    void        drawSelection(
        ScreenContext&,
        Tessellator&,
        mce::MaterialPtr const&,
        TextureVariant const&,
        Vec3 const& cameraPosition,
        selection::SelectionOverlayView const&,
        app::Config::Selection const&
    );
    void drawPlacementBoxes(
        ScreenContext&,
        Tessellator&,
        mce::MaterialPtr const&,
        TextureVariant const&,
        Vec3 const& cameraPosition,
        app::PlacementService const&,
        int dimensionId
    );
    static std::unique_ptr<mce::Mesh> buildMesh(
        ScreenContext&             screenContext,
        Tessellator&               tessellator,
        mce::MaterialPtr const&    material,
        TextureVariant const&      texture,
        std::vector<Vertex> const& vertices,
        bool                       lines = false
    );

    struct Batch : detail::RenderBatchVersion {
        std::vector<Vertex>                                                      block, property, edges;
        std::unique_ptr<mce::Mesh>                                               blockMesh, propertyMesh, edgeMesh;
        std::unordered_map<uint64_t, verifier::VerificationCell>::const_iterator cursor;
    };
    TextureVariant          resolveTexture(LevelRenderer*);
    mce::MaterialPtr const* resolveMaterial();
    void                    draw(
        ScreenContext&,
        Tessellator&,
        mce::MaterialPtr const&,
        TextureVariant const&,
        Vec3 const&,
        app::PlacementService const&,
        int dimension,
        app::Config const&,
        selection::SelectionOverlayView const&
    );
    std::unordered_map<util::WorldBlockKey, Batch, util::WorldBlockKeyHash> mBatches;
    detail::HullCacheState                                                  mCache;
    bool                                                                    mTaskOverlay = false;
    mce::TexturePtr                                                         mTexture;
    std::unique_ptr<mce::MaterialPtr>                                       mMaterial;
    bool                                                                    mTextureResolved  = false;
    bool                                                                    mDrawable         = false;
    bool                                                                    mReportedUnrouted = false;
    std::size_t                                                             mBuildStart       = 0;
    std::optional<bool>                                                     mDeferred;
    std::unique_ptr<mce::Mesh>                                              mSelectionMesh;
    struct PlacementBox {
        BlockPos                   size;
        std::unique_ptr<mce::Mesh> mesh;
    };
    std::map<uint32_t, PlacementBox> mPlacementBoxes;
    std::optional<BlockPos>          mSelectionSize;
    std::array<float, 4>             mSelectionColor{};
};
} // namespace levischematic::render
