#include "levischematic/schematic/block_actor/SchematicChestRenderer.h"

#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/model/geom/ModelPart.h"
#include "mc/client/model/models/ChestModel.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/actor/ActorTextureInfo.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/renderer/RenderMaterialInfo.h"
#include "mc/deps/core_graphics/TextureSetLayerType.h"
#include "mc/deps/core_graphics/enums/BlendTarget.h"
#include "mc/deps/minecraft_renderer/framebuilder/CSSGameplayFlags.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/RenderMaterial.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureData.h"
#include "mc/deps/renderer/Camera.h"
#include "mc/external/render_dragon/resources/ServerResourcePointer.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/CopperBehavior.h"
#include "mc/world/level/block/CopperType.h"
#include "mc/world/level/block/VanillaBlockTypeIds.h"
#include "mc/world/level/block/actor/ChestBlockActor.h"
#include "mc/world/level/block/states/BuiltInBlockStates.h"

#include <glm/ext/matrix_transform.hpp>


namespace dragon {
class ResolvedImageResource {};
}; // namespace dragon
namespace mce::framebuilder {
struct CustomSurfaceShaderMetadata {
    uint             mHash;
    CSSGameplayFlags mGameplayFlags;
};
} // namespace mce::framebuilder

ResourceLocation& ResourceLocation::operator=(ResourceLocation const& rhs) {
    if (this == &rhs) {
        return *this;
    }

    mFileSystem  = rhs.mFileSystem;
    mPath->value = rhs.mPath->value;
    mPathHash    = rhs.mPathHash;
    mFullHash    = rhs.mFullHash;

    return *this;
}

OffscreenCaptureData::OffscreenCaptureData() {
    mUnk59fd72.as<uint>() = 0;
    mUnkde1fa8.as<uint>() = 0;
    mUnk8285b9.as<mce::ServerResourcePointer<dragon::ResolvedImageResource>>() =
        mce::ServerResourcePointer<dragon::ResolvedImageResource>();
}

OffscreenCaptureData::OffscreenCaptureData(OffscreenCaptureData const& rhs) {
    mUnk59fd72.as<uint>() = rhs.mUnk59fd72.as<uint>();
    mUnkde1fa8.as<uint>() = rhs.mUnkde1fa8.as<uint>();
    mUnk8285b9.as<mce::ServerResourcePointer<dragon::ResolvedImageResource>>() =
        rhs.mUnk8285b9.as<mce::ServerResourcePointer<dragon::ResolvedImageResource>>();
}

namespace dragon {
struct RenderMetadata {
    const uint64                                         mID;
    const mce::framebuilder::CustomSurfaceShaderMetadata mCSSMetadata;
    const bool                                           mIsItem;
    std::variant<
        std::monostate,
        UIActorOffscreenCaptureDescription,
        UIThumbnailMeshOffscreenCaptureDescription,
        UIMeshOffscreenCaptureDescription,
        UIStructureVolumeOffscreenCaptureDescription>
        mOffscreenCaptureDescription;
};
} // namespace dragon

namespace levischematic::schematic::block_actor {

SchematicChestRenderer::SchematicChestRenderer(std::shared_ptr<::mce::TextureGroup> textureGroup, float transparency)
: ChestRenderer(std::move(textureGroup)) {
    mTransparency       = transparency;
    auto newmaterialptr = mce::MaterialPtr(mce::RenderMaterialGroup::common(), HashedString("chest.skinning_blend"));

    auto newmater_ptr =
        std::make_unique<mce::RenderMaterial>(*mChestModel->mDefaultMaterial->mRenderMaterialInfoPtr->mPtr);
    newmaterialptr.mRenderMaterialInfoPtr = std::make_shared<mce::RenderMaterialInfo>();
    // newmaterialptr.mRenderMaterialInfoPtr->mHashedName = HashedString("chest.skinning_blend");
    newmaterialptr.mRenderMaterialInfoPtr->mPtr = std::move(newmater_ptr);


    newmaterialptr.mRenderMaterialInfoPtr->mPtr->mStateMask                         = mce::RenderState::Blending;
    newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->enableBlend = true;
    newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->enableAlphaToCoverage   = false;
    newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->blendSource             = (mce::BlendTarget)0x6;
    newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->blendDestination        = (mce::BlendTarget)0x8;
    newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->alphaSource             = (mce::BlendTarget)0x6;
    newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->alphaDestination        = (mce::BlendTarget)0x8;
    newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->colorWriteMask          = 0xF;
    newmaterialptr.mRenderMaterialInfoPtr->mPtr->depthStencilStateDescription->depthTestEnabled = true;
    newmaterialptr.mRenderMaterialInfoPtr->mPtr->depthStencilStateDescription->depthWriteMask =
        (mce::DepthWriteMask)0x1;

    mChestModel->mDefaultMaterial                             = newmaterialptr;
    mChestModel->mMaterialVariants->mSkinningMaterialPtr      = newmaterialptr;
    mChestModel->mMaterialVariants->mSkinningColorMaterialPtr = newmaterialptr;

    mLargeChestModel->mDefaultMaterial                             = newmaterialptr;
    mLargeChestModel->mMaterialVariants->mSkinningMaterialPtr      = newmaterialptr;
    mLargeChestModel->mMaterialVariants->mSkinningColorMaterialPtr = newmaterialptr;

    mChestModel->mLid->mRot->x = 0;

    textureMap[0]                                 = ResourceLocationMap(normalTex->mResourceLocations->mColorLocation);
    normalTex->mResourceLocations->mColorLocation = textureMap[0].blendRes;

    textureMap[1]                                = ResourceLocationMap(largeTex->mResourceLocations->mColorLocation);
    largeTex->mResourceLocations->mColorLocation = textureMap[1].blendRes;

    textureMap[2] = ResourceLocationMap(trappedTex->mResourceLocations->mColorLocation);
    trappedTex->mResourceLocations->mColorLocation = textureMap[2].blendRes;

    textureMap[3] = ResourceLocationMap(trappedLargeTex->mResourceLocations->mColorLocation);
    trappedLargeTex->mResourceLocations->mColorLocation = textureMap[3].blendRes;

    textureMap[4]                                = ResourceLocationMap(enderTex->mResourceLocations->mColorLocation);
    enderTex->mResourceLocations->mColorLocation = textureMap[4].blendRes;

    auto assignTexture = [&](CopperType type) {
        textureMap[(uchar)type + 5] =
            ResourceLocationMap(mCopperTextures.get()[type].mResourceLocations->mColorLocation);
        mCopperTextures.get()[type].mResourceLocations->mColorLocation = textureMap[(uchar)type + 5].blendRes;


        textureMap[(uchar)type + 9] =
            ResourceLocationMap(mLargeCopperTextures.get()[type].mResourceLocations->mColorLocation);
        mLargeCopperTextures.get()[type].mResourceLocations->mColorLocation = textureMap[(uchar)type + 9].blendRes;
    };
    assignTexture(CopperType::Default);
    assignTexture(CopperType::Exposed);
    assignTexture(CopperType::Weathered);
    assignTexture(CopperType::Oxidized);
}

void SchematicChestRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    ScreenContext& screenContext  = renderContext.mScreenContext;
    const Vec3     renderPosition = blockEntityRenderData.renderPosition;

    const Block& block     = blockEntityRenderData.block;
    int          facingDir = 0;
    if (!blockEntityRenderData.entity) return;

    BlockType* blockType   = block.mBlockType;
    bool       foundInTree = false;

    auto& stateTree = blockType->mStates;
    auto  it        = stateTree->find(BuiltInBlockStates::CardinalDirection().mID);
    if (it != stateTree->end() && it->first == BuiltInBlockStates::CardinalDirection().mID) {
        foundInTree = true;
    }

    if (!foundInTree) {
        for (auto& alteredState : blockType->mAlteredStateCollections.get()) {
            if (alteredState->mBlockState.get().get().mID == BuiltInBlockStates::CardinalDirection().mID) {
                foundInTree = true;
                break;
            }
        }
    }

    if (foundInTree) {
        uint64_t stateId = BuiltInBlockStates::CardinalDirection().mID;
        facingDir        = *block.getState<int>(stateId);
    }

    MatrixStack::MatrixStackRef worldMatRef = renderContext.mScreenContext.camera.worldMatrixStack->push(false);
    auto*                       mat         = worldMatRef.mat;

    glm::vec3 negTarget         = {renderPosition.x, renderPosition.y + 1.0f, renderPosition.z + 1.0f};
    worldMatRef.stack->_isDirty = true;

    mat->_m = glm::translate(mat->_m.get(), negTarget);

    mat->_m = glm::scale(mat->_m.get(), glm::vec3(1.0f, -1.0f, -1.0f));

    mat->_m = glm::translate(mat->_m.get(), glm::vec3(0.5f, 0.5f, 0.5f));


    float rotationY = 0.0f;
    switch (facingDir) {
    case 1:
        rotationY = 90.0f;
        break;
    case 2:
        rotationY = 180.0f;
        break;
    case 3:
        rotationY = -90.0f;
        break;
    case 0:
    default:
        rotationY = 0.0f;
        break;
    }

    mat->_m = glm::rotate(mat->_m.get(), rotationY * 0.017453292f, glm::vec3(0.0f, 1.0f, 0.0f));

    mat->_m = glm::translate(mat->_m.get(), glm::vec3(-0.5f, -0.5f, -0.5f));

    auto* chestTexture    = &normalTex.get();
    auto* chestModel      = &mChestModel.get();
    auto  chestBlockActor = (ChestBlockActor*)blockEntityRenderData.entity;
    if (chestBlockActor->mLargeChestPaired) {
        chestModel = &mLargeChestModel.get();
        mat->translate(glm::vec3(0.5f, 0.0f, 0.0f));
    }
    if (blockEntityRenderData.block.getBlockType().mNameInfo->mFullName.get() == VanillaBlockTypeIds::TrappedChest()) {
        chestTexture = &trappedLargeTex.get();
        if (!chestBlockActor->mLargeChestPaired) {
            chestTexture = &trappedTex.get();
        }
    } else {
        auto copperbehavior = blockEntityRenderData.block.getBlockType().tryGetCopperBehavior();
        if (copperbehavior) {
            chestTexture = &mCopperTextures.get()[copperbehavior->mType];
            if (chestBlockActor->mLargeChestPaired) {
                chestTexture = &mLargeCopperTextures.get()[copperbehavior->mType];
            }
        } else if (chestBlockActor->mIsGlobalChest) {
            chestTexture = &enderTex.get();
        } else {
            if (chestBlockActor->mLargeChestPaired) {
                chestTexture = &largeTex.get();
            }
        }
    }

    {
        dragon::RenderMetadata renderMetadata{
            blockEntityRenderData.pos.hash(),
            {(uint)blockEntityRenderData.block.getBlockType().mNameInfo->mFullName->getHash(),
                         (mce::framebuilder::CSSGameplayFlags)1},
            false,
            renderContext.mOffscreenCaptureDescription
        };

        _renderModel(
            screenContext,
            renderMetadata,
            *chestModel,
            *chestTexture,
            chestModel->mDefaultMaterial,
            chestTexture->mTexturePtrs->mColorTexture->getClientTexture()
        );
    }
}

const BaseRenderSchematic::ResourceLocationMap* SchematicChestRenderer::isThisRender(HashedString const& hash) {
    for (const auto& item : textureMap) {
        if (item.vanilla.getHashedPath() == hash) return &item;
    }
    return nullptr;
}

bool SchematicChestRenderer::isAllUpload() {
    for (const auto& item : textureMap) {
        if (!item.mIsUpload) return false;
    }
    return true;
}

void SchematicChestRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    normalTex->mTexturePtrs->mColorTexture =
        textureGroup
            ->getTexture(normalTex->mResourceLocations->mColorLocation, false, 0, cg::TextureSetLayerType::Color);

    largeTex->mTexturePtrs->mColorTexture =
        textureGroup
            ->getTexture(largeTex->mResourceLocations->mColorLocation, false, 0, cg::TextureSetLayerType::Color);

    trappedTex->mTexturePtrs->mColorTexture =
        textureGroup
            ->getTexture(trappedTex->mResourceLocations->mColorLocation, false, 0, cg::TextureSetLayerType::Color);

    trappedLargeTex->mTexturePtrs->mColorTexture =
        textureGroup
            ->getTexture(trappedLargeTex->mResourceLocations->mColorLocation, false, 0, cg::TextureSetLayerType::Color);

    enderTex->mTexturePtrs->mColorTexture =
        textureGroup
            ->getTexture(enderTex->mResourceLocations->mColorLocation, false, 0, cg::TextureSetLayerType::Color);

    auto assignTexture = [&](CopperType type) {
        mCopperTextures.get()[type].mTexturePtrs->mColorTexture = textureGroup->getTexture(
            mCopperTextures.get()[type].mResourceLocations->mColorLocation,
            false,
            0,
            cg::TextureSetLayerType::Color
        );

        mLargeCopperTextures.get()[type].mTexturePtrs->mColorTexture = textureGroup->getTexture(
            mLargeCopperTextures.get()[type].mResourceLocations->mColorLocation,
            false,
            0,
            cg::TextureSetLayerType::Color
        );
    };
    assignTexture(CopperType::Default);
    assignTexture(CopperType::Exposed);
    assignTexture(CopperType::Weathered);
    assignTexture(CopperType::Oxidized);
}

} // namespace levischematic::schematic::block_actor
