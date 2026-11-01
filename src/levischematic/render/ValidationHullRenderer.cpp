#include "ValidationHullRenderer.h"
#include "BoxFaces.h"
#include "RenderVisibility.h"

#include "levischematic/app/PlacementService.h"
#include "levischematic/render/SchematicRenderAlgorithms.h"
#include "levischematic/selection/SelectionState.h"
#include "levischematic/schematic/FakeWorld.h"

#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/common/client/renderer/helpers/MeshHelpers.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include "mc/deps/renderer/ShaderColor.h"
#include "mc/world/level/BlockPos.h"

#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/InGamePlayScreen.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/game/ItemInHandRenderer.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/renderer/RenderMaterialInfo.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/core_graphics/TextureSetLayerType.h"
#include "mc/deps/minecraft_renderer/objects/FrameRenderObject.h"
#include "mc/deps/minecraft_renderer/objects/ViewRenderObject.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/IsMissingTexture.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/RenderMaterial.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/world/level/BlockSource.h"

#include <optional>


#include "levischematic/util/ScopeExit.h"
#include <limits>
#include <optional>

namespace levischematic::render {
void ValidationHullRenderer::clear() noexcept {
    mBatches.clear();
    mSelectionMesh.reset();
    mSelectionSize.reset();
    mMaterial.reset();
    mTexture         = {};
    mTextureResolved = false;
    mDrawable        = false;
    mCache           = {};
    mDeferred.reset();
    mBuildStart = 0;
}

void ValidationHullRenderer::appendCube(std::vector<Vertex>& vertices, BlockPos const& position, mce::Color color) {
    float const expansion = mCache.style.expansion;
    appendBox(
        vertices,
        {static_cast<float>(position.x) - expansion,
         static_cast<float>(position.y) - expansion,
         static_cast<float>(position.z) - expansion},
        {static_cast<float>(position.x + 1) + expansion,
         static_cast<float>(position.y + 1) + expansion,
         static_cast<float>(position.z + 1) + expansion},
        color
    );
}

void ValidationHullRenderer::appendBox(
    std::vector<Vertex>& vertices,
    Vec3 const&          minimum,
    Vec3 const&          maximum,
    mce::Color           color
) {
    for (auto const& face :
         detail::makeBoxFaces({minimum.x, minimum.y, minimum.z}, {maximum.x, maximum.y, maximum.z})) {
        glm::vec4 const normal{face.normal[0], face.normal[1], face.normal[2], 0.0f};
        for (auto const& position : face.vertices) {
            vertices.push_back({position[0], position[1], position[2], normal, color});
        }
    }
}

void ValidationHullRenderer::drawSelection(
    ScreenContext&                         screenContext,
    Tessellator&                           tessellator,
    mce::MaterialPtr const&                material,
    TextureVariant const&                  texture,
    Vec3 const&                            cameraPosition,
    selection::SelectionOverlayView const& selection,
    app::Config::Selection const&          style
) {
    if (!selection.corner1 || !selection.corner2 || style.faceColor[3] == 0.0f) {
        mSelectionMesh.reset();
        mSelectionSize.reset();
        return;
    }
    selection::Box const box{*selection.corner1, *selection.corner2};
    auto const           origin = box.getMin();
    auto const           size   = box.getSize();
    if (!mSelectionMesh || mSelectionSize != size || mSelectionColor != style.faceColor) {
        std::vector<Vertex> vertices;
        vertices.reserve(24);
        constexpr float expansion = 0.002f;
        auto const&     color     = style.faceColor;
        appendBox(
            vertices,
            {-expansion, -expansion, -expansion},
            {static_cast<float>(size.x) + expansion,
             static_cast<float>(size.y) + expansion,
             static_cast<float>(size.z) + expansion},
            mce::Color{color[0], color[1], color[2], color[3]}
        );
        mSelectionMesh  = buildMesh(screenContext, tessellator, material, texture, vertices);
        mSelectionSize  = size;
        mSelectionColor = color;
    }
    if (!mSelectionMesh) return;
    auto matrix            = screenContext.camera.worldMatrixStack->push(false);
    matrix.stack->_isDirty = true;
    matrix.mat->translate(
        glm::vec3{origin.x - cameraPosition.x, origin.y - cameraPosition.y, origin.z - cameraPosition.z}
    );
    OffscreenCaptureDescription captureDescription{};
    mSelectionMesh->renderMesh(screenContext, material, texture, 0, 0, captureDescription, nullptr);
}

std::unique_ptr<mce::Mesh> ValidationHullRenderer::buildMesh(
    ScreenContext&             screenContext,
    Tessellator&               tessellator,
    mce::MaterialPtr const&    material,
    TextureVariant const&      texture,
    std::vector<Vertex> const& vertices
) {
    if (vertices.empty() || vertices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    tessellator.begin({}, mce::PrimitiveMode::QuadList, static_cast<int>(vertices.size()), false);
    
    
    
    
    tessellator.mNextNormal        = std::nullopt;
    tessellator.mNextPBRTextureIdx = std::nullopt;
    tessellator.mNextMERS          = std::nullopt;
    tessellator.mNextGeoType       = std::nullopt;
    tessellator.mQuadTwoSided      = true;
    for (auto const& vertex : vertices) {
        tessellator.color(vertex.color);
        tessellator.tex2({0.5f, 0.5f});
        tessellator.vertex(vertex.x, vertex.y, vertex.z);
    }

    std::unique_ptr<mce::Mesh> result;
    MeshHelpers::renderMeshImmediately(
        screenContext,
        tessellator,
        material,
        texture,
        [&](ScreenContext const&, mce::Mesh const& mesh, mce::MaterialPtr const&, TextureVariant const&) {
            if (mesh.isValid()) result = std::make_unique<mce::Mesh>(mesh);
        }
    );
    return result;
}

void ValidationHullRenderer::draw(
    ScreenContext&                         screenContext,
    Tessellator&                           tessellator,
    mce::MaterialPtr const&                material,
    TextureVariant const&                  texture,
    Vec3 const&                            cameraPosition,
    app::PlacementService const&           placements,
    int                                    currentDimensionId,
    app::Config const&                     config,
    selection::SelectionOverlayView const& selection
) {
    auto const& state = placements.verification();
    auto const& view  = placements.viewState();
    if (!mCache.matches(state.epoch, view.revision, config.hull)) {
        mBatches.clear();
        mCache.epoch        = state.epoch;
        mCache.viewRevision = view.revision;
        mCache.style        = config.hull;
    }
    util::WorkBudget budget(config.performance.frameBudgetMs);
    auto             client        = ll::service::getClientInstance();
    auto*            levelRenderer = client ? client->getLevelRenderer() : nullptr;
    auto*            camera        = levelRenderer ? levelRenderer->mLevelRendererPlayer.get() : nullptr;
    struct Candidate {
        util::WorldBlockKey                key;
        verifier::VerificationChunk const* chunk;
        BlockPos                           origin;
        double                             priority;
    };
    std::vector<Candidate> candidates;
    for (auto const& [key, chunk] : state.chunks) {
        if (state.clearing) break;
        if (key.dimensionId != currentDimensionId) continue;
        auto     index = util::decodePosKey(key.posKey);
        BlockPos origin{index.x * 16, index.y * 16, index.z * 16};
        auto     bounds = subChunkBounds(
            Vec3{static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(origin.z)},
            config.hull.expansion + 0.01f
        );
        auto result = testVisibility(cameraPosition, bounds, config.visibility, camera);
        if (result != VisibilityResult::Visible) continue;
        auto cached = mBatches.find(key);
        if (cached != mBatches.end() && cached->second.matches(chunk.revision)) {
            continue;
        }
        double distance = detail::distanceSquaredToBox(
            {cameraPosition.x, cameraPosition.y, cameraPosition.z},
            {bounds.min.x, bounds.min.y, bounds.min.z},
            {bounds.max.x, bounds.max.y, bounds.max.z}
        );
        candidates.push_back({key, &chunk, origin, std::floor(std::sqrt(distance) / 16.0)});
    }
    std::erase_if(mBatches, [&](auto const& item) { return !state.chunks.contains(item.first); });
    detail::prioritizeVisibleBuilds(candidates, mBuildStart);
    ++mBuildStart;

    bool const       oldDrawingUi            = screenContext.isDrawingUI;
    bool const       oldDrawingEditor        = screenContext.isDrawingEditorSelectionHighlight;
    bool const       oldDrawingMovingBlock   = screenContext.isDrawingMovingBlock;
    bool const       oldDithering            = screenContext.isDitheringEnabled;
    bool const       oldAlphaMaskedTint      = screenContext.isAlphaMaskedTintEnabled;
    mce::Color const oldShaderColor          = screenContext.currentShaderColor.color;
    bool const       oldShaderColorDirty     = screenContext.currentShaderColor.dirty;
    mce::Color const oldShaderDarkColor      = screenContext.currentShaderDarkColor.color;
    bool const       oldShaderDarkColorDirty = screenContext.currentShaderDarkColor.dirty;
    Vec3 const       oldOffset               = *tessellator.mPostTransformOffset;
    auto const       oldScale                = tessellator.mPostTransformScale.get();
    auto const       oldTransform            = tessellator.mTransformMatrix.get();
    bool const       oldApplyTransform       = tessellator.mApplyTransform;
    auto const       oldNextColor            = tessellator.mNextColor;
    auto const       oldNextBone             = tessellator.mNextBoneId;
    std::array oldNextUv{tessellator.mNextUV[0].get(), tessellator.mNextUV[1].get(), tessellator.mNextUV[2].get()};
    auto const       oldNextNormal           = tessellator.mNextNormal;
    auto const       oldNextPbrTextureIdx    = tessellator.mNextPBRTextureIdx;
    auto const       oldNextMers             = tessellator.mNextMERS;
    auto const       oldNextGeoType          = tessellator.mNextGeoType;
    uchar const      oldQuadFacing           = tessellator.mQuadFacing;
    bool const       oldQuadTwoSided         = tessellator.mQuadTwoSided;
    bool const       oldNoColor              = tessellator.mNoColor;
    util::ScopeExit  restoreState([&] {
        *tessellator.mPostTransformOffset               = oldOffset;
        tessellator.mPostTransformScale                 = oldScale;
        tessellator.mTransformMatrix                    = oldTransform;
        tessellator.mApplyTransform                     = oldApplyTransform;
        tessellator.mNextColor                          = oldNextColor;
        tessellator.mNextBoneId                         = oldNextBone;
        for (std::size_t index = 0; index < oldNextUv.size(); ++index) tessellator.mNextUV[index] = oldNextUv[index];
        tessellator.mNextNormal                         = oldNextNormal;
        tessellator.mNextPBRTextureIdx                  = oldNextPbrTextureIdx;
        tessellator.mNextMERS                           = oldNextMers;
        tessellator.mNextGeoType                        = oldNextGeoType;
        tessellator.mQuadFacing                         = oldQuadFacing;
        tessellator.mQuadTwoSided                       = oldQuadTwoSided;
        tessellator.mNoColor                            = oldNoColor;
        screenContext.currentShaderColor.color          = oldShaderColor;
        screenContext.currentShaderColor.dirty          = oldShaderColorDirty;
        screenContext.currentShaderDarkColor.color      = oldShaderDarkColor;
        screenContext.currentShaderDarkColor.dirty      = oldShaderDarkColorDirty;
        screenContext.isDrawingUI                       = oldDrawingUi;
        screenContext.isDrawingEditorSelectionHighlight = oldDrawingEditor;
        screenContext.isDrawingMovingBlock              = oldDrawingMovingBlock;
        screenContext.isDitheringEnabled                = oldDithering;
        screenContext.isAlphaMaskedTintEnabled          = oldAlphaMaskedTint;
    });
    screenContext.isDrawingUI = false;
    
    
    
    screenContext.isDrawingEditorSelectionHighlight = false;
    screenContext.isDrawingMovingBlock              = false;
    screenContext.isDitheringEnabled                = false;
    screenContext.isAlphaMaskedTintEnabled          = false;
    screenContext.currentShaderColor.color          = mce::Color::WHITE();
    screenContext.currentShaderColor.dirty          = true;
    screenContext.currentShaderDarkColor.color      = mce::Color::WHITE();
    screenContext.currentShaderDarkColor.dirty      = true;
    tessellator.mPostTransformOffset                = Vec3::ZERO();
    tessellator.mPostTransformScale                 = Vec3{1.0f, 1.0f, 1.0f};
    tessellator.mApplyTransform                     = false;
    drawSelection(screenContext, tessellator, material, texture, cameraPosition, selection, config.selection);
    if (state.clearing) return;
    std::size_t builds = 0;
    for (auto const& candidate : candidates) {
        if (!budget.available()) break;
        auto& batch = mBatches[candidate.key];
        if (batch.revision != candidate.chunk->revision) {
            batch          = Batch{};
            batch.revision = candidate.chunk->revision;
            batch.cursor   = candidate.chunk->cells.begin();
        }
        while (batch.cursor != candidate.chunk->cells.end() && budget.available()) {
            auto const& [posKey, cell] = *batch.cursor++;
            auto pos                   = util::decodePosKey(posKey);
            if (!view.layerRange.contains(pos.y)) continue;
            auto const& color = cell.status == verifier::VerificationStatus::BlockMismatch ? config.hull.blockColor
                                                                                           : config.hull.propertyColor;
            if (color[3] == 0.0f) continue;
            BlockPos local{pos.x - candidate.origin.x, pos.y - candidate.origin.y, pos.z - candidate.origin.z};
            if (cell.status == verifier::VerificationStatus::BlockMismatch)
                appendCube(batch.block, local, {color[0], color[1], color[2], color[3]});
            else if (cell.status == verifier::VerificationStatus::PropertyMismatch)
                appendCube(batch.property, local, {color[0], color[1], color[2], color[3]});
        }
        if (batch.cursor != candidate.chunk->cells.end()) continue;
        if (batch.block.empty() && batch.property.empty()) {
            batch.complete = true;
            continue;
        }
        if (!budget.available() || builds >= config.performance.geometryBuildsPerFrame) continue;
        ++builds;
        auto build = [&](auto& pending, auto& mesh) {
            if (pending.empty() || mesh || !budget.available()) return;
            mesh      = buildMesh(screenContext, tessellator, material, texture, pending);
            if (mesh) std::vector<Vertex>{}.swap(pending);
        };
        build(batch.block, batch.blockMesh);
        build(batch.property, batch.propertyMesh);
        batch.complete = batch.block.empty() && batch.property.empty();
    }
    OffscreenCaptureDescription captureDescription{};
    for (auto const& [key, batch] : mBatches) {
        auto current = state.chunks.find(key);
        if (key.dimensionId != currentDimensionId || current == state.chunks.end()
            || batch.revision != current->second.revision || !batch.complete)
            continue;
        auto index = util::decodePosKey(key.posKey);
        Vec3 origin{
            static_cast<float>(index.x * 16),
            static_cast<float>(index.y * 16),
            static_cast<float>(index.z * 16)
        };
        if (testVisibility(
                cameraPosition,
                subChunkBounds(origin, config.hull.expansion + 0.01f),
                config.visibility,
                camera
            )
            != VisibilityResult::Visible)
            continue;
        auto matrix            = screenContext.camera.worldMatrixStack->push(false);
        matrix.stack->_isDirty = true;
        matrix.mat->translate(
            glm::vec3{origin.x - cameraPosition.x, origin.y - cameraPosition.y, origin.z - cameraPosition.z}
        );
        for (auto const* mesh : {batch.blockMesh.get(), batch.propertyMesh.get()}) {
            if (!mesh) continue;
            mesh->renderMesh(screenContext, material, texture, 0, 0, captureDescription, nullptr);
        }
    }
}

ValidationHullRenderer::TextureVariant ValidationHullRenderer::resolveTexture(LevelRenderer* levelRenderer) {
    if (!mTextureResolved && levelRenderer) {
        auto const& textureGroup = levelRenderer->mTextureGroup.get();
        if (textureGroup) {
            auto texture = textureGroup->getTexture(
                ResourceLocation{Core::PathView{"textures/ui/white_background"}},
                false,
                std::nullopt,
                cg::TextureSetLayerType::Color
            );
            auto const& clientTexture = texture.mClientTexture;
            if (clientTexture && clientTexture->mIsMissingTexture != IsMissingTexture::Yes) {
                mTexture         = std::move(texture);
                mTextureResolved = true;
            }
        }
    }
    if (mTextureResolved) {
        return TextureVariant{mTexture};
    }
    return {};
}


mce::MaterialPtr const* ValidationHullRenderer::resolveMaterial() {
    if (mMaterial) return mMaterial.get();
    for (auto* group : {&mce::RenderMaterialGroup::common(), &mce::RenderMaterialGroup::switchable()}) {
        for (auto const& [name, info] : group->mMaterials.get()) {
            if (name.getString() == "glow_sign_text" && info && info->mPtr) {
                mMaterial = std::make_unique<mce::MaterialPtr>(*group, name);
                return mMaterial.get();
            }
        }
    }
    return nullptr;
}
bool ValidationHullRenderer::contains(
    app::PlacementService const& placements,
    BlockPos const&              pos,
    int                          dimension,
    app::Config const&           config
) const {
    auto const& state = placements.verification();
    if (!mDrawable || state.clearing || !mCache.matches(state.epoch, placements.viewState().revision, config.hull)
        || !placements.viewState().layerRange.contains(pos.y))
        return false;
    auto  client   = ll::service::getClientInstance();
    auto* renderer = client ? client->getLevelRenderer() : nullptr;
    auto* camera   = renderer ? renderer->mLevelRendererPlayer.get() : nullptr;
    if (!camera || mCache.style != config.hull) return false;
    auto origin = util::subChunkOrigin(pos.x, pos.y, pos.z);
    if (testVisibility(
            camera->mCameraPos.get(),
            subChunkBounds(
                Vec3{static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(origin.z)},
                config.hull.expansion + 0.01f
            ),
            config.visibility,
            camera
        )
        != VisibilityResult::Visible)
        return false;
    auto key     = util::WorldBlockKey{dimension, util::subChunkKeyFromWorldPos(pos.x, pos.y, pos.z)};
    auto current = state.chunks.find(key);
    auto batch   = mBatches.find(key);
    if (current == state.chunks.end() || batch == mBatches.end() || current->second.revision != batch->second.revision
        || !batch->second.complete)
        return false;
    auto* cell = state.find(dimension, pos);
    return cell
        && ((cell->status == verifier::VerificationStatus::BlockMismatch && batch->second.blockMesh
             && mCache.style.blockColor[3] > 0)
            || (cell->status == verifier::VerificationStatus::PropertyMismatch && batch->second.propertyMesh
                && mCache.style.propertyColor[3] > 0));
}
void ValidationHullRenderer::renderPass(
    ScreenContext&                         screenContext,
    Vec3 const&                            cameraPosition,
    app::PlacementService const&           placements,
    int                                    dimensionId,
    app::Config const&                     config,
    selection::SelectionOverlayView const& selection
) {
    mDrawable           = false;
    auto& tessellator   = screenContext.tessellator;
    auto  client        = ll::service::getClientInstance();
    auto* levelRenderer = client ? client->getLevelRenderer() : nullptr;
    auto* rendererPlayer =
        levelRenderer && levelRenderer->mLevelRendererPlayer ? levelRenderer->mLevelRendererPlayer.get() : nullptr;
    auto* itemRenderer = client ? client->getItemInHandRenderer() : nullptr;
    if (!client || !levelRenderer || !rendererPlayer || !itemRenderer) {
        return;
    }

    auto const*             glowMaterial      = resolveMaterial();
    auto                    texture           = glowMaterial ? resolveTexture(levelRenderer) : TextureVariant{};
    bool const              deferred          = static_cast<bool>(itemRenderer->mIsDeferredEnabled);
    if (mDeferred && *mDeferred != deferred) {
        mBatches.clear();
        mSelectionMesh.reset();
    }
    mDeferred                                 = deferred;
    auto const&             outlineMaterial   = rendererPlayer->mOutlineSelectionMaterial.get();
    auto const&             selectionMaterial = rendererPlayer->selectionTwoSidedMaterial.get();
    mce::MaterialPtr const* materialPtr       = nullptr;
    
    
    
    
    if (glowMaterial) {
        materialPtr = glowMaterial;
    } else if (deferred && outlineMaterial.mRenderMaterialInfoPtr) {
        materialPtr = &outlineMaterial;
    } else if (selectionMaterial.mRenderMaterialInfoPtr) {
        materialPtr = &selectionMaterial;
    }
    if (!materialPtr) {
        return;
    }
    auto const& material = *materialPtr;

    auto* renderMaterial = material.mRenderMaterialInfoPtr ? material.mRenderMaterialInfoPtr->mPtr.get() : nullptr;
    auto const& blendMaterialPtr = itemRenderer->mMatBlendBlock.get();
    auto*       blendMaterial =
        blendMaterialPtr.mRenderMaterialInfoPtr ? blendMaterialPtr.mRenderMaterialInfoPtr->mPtr.get() : nullptr;
    



    struct MaterialStateRestore {
        mce::RenderMaterial*                      material{};
        std::optional<mce::BlendStateDescription> blend;
        mce::PrimitiveMode                        primitive{};
        float                                     depthBias{};
        float                                     slopeBias{};
        bool                                      restorePrimitive = false;
        


        ~MaterialStateRestore() {
            if (!material || !blend) return;
            material->blendStateDescription.get() = *blend;
            material->mDepthBias                  = depthBias;
            material->mSlopeScaledDepthBias       = slopeBias;
            if (restorePrimitive) material->mPrimitiveMode = primitive;
        }
    } restore;
    if (deferred && renderMaterial && blendMaterial) {
        restore.material                            = renderMaterial;
        restore.blend                               = renderMaterial->blendStateDescription.get();
        restore.primitive                           = renderMaterial->mPrimitiveMode;
        restore.depthBias                           = renderMaterial->mDepthBias;
        restore.slopeBias                           = renderMaterial->mSlopeScaledDepthBias;
        restore.restorePrimitive                    = true;
        renderMaterial->mPrimitiveMode              = mce::PrimitiveMode::QuadList;
        renderMaterial->blendStateDescription.get() = blendMaterial->blendStateDescription.get();
        renderMaterial->mDepthBias                  = config.hull.depthBias;
        renderMaterial->mSlopeScaledDepthBias       = config.hull.slopeBias;
    }

    draw(screenContext, tessellator, material, texture, cameraPosition, placements, dimensionId, config, selection);
    mDrawable = true;
}

} 
