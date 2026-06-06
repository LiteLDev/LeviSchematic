
#include "ll/api/memory/Hook.h"
#include "ll/api/memory/Memory.h"
#include "ll/api/service/TargetedBedrock.h"

#include "levischematic/LeviSchematic.h"

#include "mc/client/model/models/ChestModel.h"
#include "mc/client/renderer/BaseActorRenderer.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/core_graphics/ImageBuffer.h"
#include "mc/deps/core_graphics/enums/BlendTarget.h"
#include "mc/world/level/block/Block.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/world/level/block/actor/BlockActorRendererId.h"
#include "mc/world/level/block/states/BuiltInBlockStates.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/actor/ActorTextureInfo.h"
#include "mc/client/renderer/blockactor/ChestRenderer.h"
#include "mc/world/level/BlockPos.h"
#include "mc/client/renderer/blockactor/BlockActorRenderer.h"
#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/world/level/block/registry/BlockTypeRegistry.h"
#include "mc/world/level/block/VanillaBlockTypeIds.h"
#include "mc/client/renderer/blockactor/BlockActorRenderDispatcher.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/model/models/DataDrivenGeometry.h"
#include "mc/client/model/geom/ModelPart.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/deps/core/renderer/RenderMaterialInfo.h"
#include "mc/deps/minecraft_renderer/renderer/RenderMaterial.h"
#include "mc/deps/core_graphics/interface/BlendStateDescription.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core_graphics/TextureSetLayerType.h"
#include "mc/deps/core/resource/ResourceLocationPair.h"

#include "mc/deps/core_graphics/TextureSetDefinition.h"
#include "mc/deps/core_graphics/TextureSetImageContainer.h"
#include "mc/deps/core_graphics/TextureSetLayerImageMipList.h"
#include "mc/deps/core_graphics/ImageResource.h"
#include "mc/util/texture_set_helpers/TextureSetDefinitionLoader.h"

#include <Windows.h>
#include <cstdlib>
#include <minwinbase.h>

namespace mce::framebuilder {
    enum class CSSGameplayFlags : int {
        MER = 0,
        MERS = 1,
    };
struct CustomSurfaceShaderMetadata {
    uint mHash;
    CSSGameplayFlags mGameplayFlags;
};
}

namespace dragon {
struct RenderMetadata {
    const uint64 mID;
    const mce::framebuilder::CustomSurfaceShaderMetadata mCSSMetadata;
    const bool mIsItem;
    std::variant<std::monostate,
    UIActorOffscreenCaptureDescription,
    UIThumbnailMeshOffscreenCaptureDescription,
    UIMeshOffscreenCaptureDescription,
    UIStructureVolumeOffscreenCaptureDescription> mOffscreenCaptureDescription;
};
}

namespace block_actor_test{
    auto& logger = levischematic::LeviSchematic::getInstance().getSelf().getLogger();

struct BlockActorRenderDataForSchematic {
    Vec3 renderPosition;
    BlockPos pos;
    const Block& block;
};


class BaseRenderSchematic {
    public:
    virtual void renderSchematic(BaseActorRenderContext& renderContext, BlockActorRenderDataForSchematic& blockEntityRenderData) = 0;
};

static std::map<BlockActorRendererId, std::unique_ptr<BaseRenderSchematic>> renderers;


LL_AUTO_TYPE_INSTANCE_HOOK(
    RenderChestActorTest,
    ll::memory::HookPriority::Normal,
    LevelRendererCamera,
    &LevelRendererCamera::$renderBlockEntities,
    void,
    BaseActorRenderContext& renderContext,
    bool renderAlphaLayer
){
    origin(renderContext, renderAlphaLayer);
    if(renderAlphaLayer && !renderers.empty()) {
    BlockPos pos(1,1,1);//渲染的世界位置
    Vec3 renderpos;
    renderpos.x = pos.x - renderContext.mCameraTargetPosition->x;
    renderpos.y = pos.y - renderContext.mCameraTargetPosition->y;
    renderpos.z = pos.z - renderContext.mCameraTargetPosition->z;

    auto& chestBlock   = BlockTypeRegistry::get().getDefaultBlockState(VanillaBlockTypeIds::Chest());

    block_actor_test::BlockActorRenderDataForSchematic data{
        renderpos,
        pos,
        chestBlock
    };
    renderers[BlockActorRendererId::Chest]->renderSchematic(renderContext, data);
    }
    if(renderers.empty()){
        logger.debug("renderer is empty!");
    }
}

std::shared_ptr<::cg::TextureSetDefinition> makeDefinitionFromImageBuffer(ResourceLocation& res, cg::ImageBuffer const* buffer){
    HMODULE hModule = GetModuleHandle(nullptr);
    //ResourceLocationPair(const ResourceLocation&, const PackIdVersion&, int) rva
    void* resourceLocationPair_va = (void*)(reinterpret_cast<BYTE*>(hModule) + 0x0000);
    void* resourceLocationPair_ptr=malloc(0x00);
    ZeroMemory(resourceLocationPair_ptr, 0x00);
    void* packid = malloc(0x00);
    ZeroMemory(packid, 0x00);
    ll::memory::addressCall<void, void*, ResourceLocation&, void*, int>(
        resourceLocationPair_va,
        resourceLocationPair_ptr,
        res,
        packid,
        0
    );

    void* func_ptr = (void*)(reinterpret_cast<BYTE*>(hModule) + 0x0000);
    return ll::memory::addressCall<std::shared_ptr<::cg::TextureSetDefinition>, ResourceLocationPair *,cg::ImageBuffer const*,bool,bool>(
        func_ptr,
        (ResourceLocationPair*)resourceLocationPair_ptr,
        buffer,
        false,
        false
    );
};

auto hash_str = HashedString("textures/entity/chest/normal");
ResourceLocation newLoc(Core::PathView("textures/entity/chest/normal_ttthjhh"));

class ModifyChestRenderer: public BaseRenderSchematic, public ChestRenderer {
public:
    explicit ModifyChestRenderer(std::shared_ptr<::mce::TextureGroup> textureGroup) :ChestRenderer(std::move(textureGroup)){
        auto newmaterialptr = mce::MaterialPtr();
        auto newmater_ptr = std::make_unique<mce::RenderMaterial>(*mChestModel->mDefaultMaterial->mRenderMaterialInfoPtr->mPtr);
        newmaterialptr.mRenderMaterialInfoPtr = std::make_shared<mce::RenderMaterialInfo>();
        newmaterialptr.mRenderMaterialInfoPtr->mHashedName = HashedString("chest.skinning_blend");
        newmaterialptr.mRenderMaterialInfoPtr->mPtr = std::move(newmater_ptr);
        

        newmaterialptr.mRenderMaterialInfoPtr->mPtr->mStateMask = mce::RenderState::Blending;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->enableBlend=true;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->enableAlphaToCoverage=false;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->blendSource=mce::BlendTarget::SourceAlpha;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->blendDestination=mce::BlendTarget::OneMinusSrcAlpha;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->alphaSource=mce::BlendTarget::SourceAlpha;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->alphaDestination=mce::BlendTarget::OneMinusSrcAlpha;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->colorWriteMask=0xF;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->depthStencilStateDescription->depthTestEnabled = true;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->depthStencilStateDescription->depthWriteMask = mce::DepthWriteMask::DepthWriteMaskNone;

        mChestModel->mDefaultMaterial = newmaterialptr;
        
        mChestModel->setModelMaterial(mChestModel->mDefaultMaterial);
        mChestModel->mMaterialVariants->mSkinningColorMaterialPtr = newmaterialptr;
        mChestModel->mLid->mRot->x = 0;
        normalTex->mResourceLocations->mUnkf61888.as<ResourceLocation>() = newLoc;
    };
    void renderSchematic(BaseActorRenderContext& renderContext, BlockActorRenderDataForSchematic& blockEntityRenderData) override {
         // ─── 提取渲染上下文基本字段 ────────────────────────────────────────
        ScreenContext&  screenContext    = renderContext.mScreenContext;
        const Vec3     renderPosition   = blockEntityRenderData.renderPosition;

        // ─── 获取朝向（CardinalDirection 方块状态）────────────────────────
        const Block& block = blockEntityRenderData.block;
        int facingDir = 0; // 默认朝向

        // 在 BlockType 的状态树中查找 CardinalDirection
        BlockType* blockType = block.mBlockType;
        bool foundInTree = false;

        // 查找状态红黑树
        auto& stateTree = blockType->mStates;
        auto  it        = stateTree->find(BuiltInBlockStates::CardinalDirection().mID);
        if (it != stateTree->end() && it->first == BuiltInBlockStates::CardinalDirection().mID) {
            foundInTree = true;
        }

        if (!foundInTree) {
            // 回退：在 AlteredStateCollections 中查找
            for (auto& alteredState : blockType->mAlteredStateCollections.get()) {
                if (alteredState->mBlockState.get().get().mID == BuiltInBlockStates::CardinalDirection().mID) {
                    foundInTree = true;
                    break;
                }
            }
        }

        if (foundInTree) {
            uint64_t stateId = BuiltInBlockStates::CardinalDirection().mID;
            facingDir = *block.getState<int>(stateId);
        }

        // ─── 压入世界矩阵并设置平移 ─────────────────────────────────────────
        MatrixStack::MatrixStackRef worldMatRef = renderContext.mScreenContext.camera
                                                .worldMatrixStack->push(false);
        auto* mat = worldMatRef.mat;

        // 将矩阵平移到方块中心（+1 是方块尺寸，Minecraft 坐标中心偏移）

        glm::vec3 negTarget = {renderPosition.x, renderPosition.y + 1.0f, renderPosition.z + 1.0f};
        worldMatRef.stack->_isDirty = true;

        // 手动做 mat = mat * translate(tx, ty, tz)，等效于乘以平移矩阵
        mat->_m = glm::translate(mat->_m.get(), negTarget);

        // 翻转 Y 和 Z 轴（Minecraft 坐标系 → OpenGL 坐标系）
        mat->_m = glm::scale(mat->_m.get(), glm::vec3(1.0f, -1.0f, -1.0f));

        // 平移到方块几何中心 (0.5, 0.5, 0.5)
        mat->_m = glm::translate(mat->_m.get(), glm::vec3(0.5f, 0.5f, 0.5f));


        // 普通世界方块：按朝向旋转
        float rotationY = 0.0f;
        switch (facingDir) {
            case 1:  rotationY = 90.0f;   break; // 朝西
            case 2:  rotationY = 180.0f;  break; // 朝北
            case 3:  rotationY = -90.0f;  break; // 朝东
            case 0:
            default: rotationY = 0.0f;    break; // 朝南（默认）
        }

        // 绕 Y 轴旋转（角度转弧度：deg * PI/180）
        mat->_m = glm::rotate(mat->_m.get(), rotationY * 0.017453292f, glm::vec3(0.0f, 1.0f, 0.0f));

        // 旋转后重新计算中心，并将原点移至方块中心
        mat->_m = glm::translate(mat->_m.get(), glm::vec3(-0.5f, -0.5f, -0.5f));

        // ─── 构建 RenderMetadata ──────────────────────────────────────
        {
        dragon::RenderMetadata renderMetadata{
            blockEntityRenderData.pos.hashCode(),
            {blockEntityRenderData.block.getBlockType().mNameInfo->mFullName->getHash(), mce::framebuilder::CSSGameplayFlags::MERS},
            false,
            renderContext.mOffscreenCaptureDescription
        };

        // ─── 提交模型渲染 ─────────────────────────────────────────────
        _renderModel(
            screenContext,
            renderMetadata,
            mChestModel,
            normalTex
        );
    }
    }
};

LL_AUTO_TYPE_INSTANCE_HOOK(
    UploadImage,
    ll::memory::HookPriority::Normal,
    mce::TextureGroup,
    &mce::TextureGroup::uploadTexture,
    BedrockTexture&,
    ResourceLocation const& resourceLocation,
    gsl::not_null<::std::shared_ptr<::cg::TextureSetDefinition>> textureSetDefinition
) {
    if(resourceLocation.getHashedPath() == hash_str && !textureSetDefinition->_getImageContainer()->mLayerImageList->empty()) {
        auto old = textureSetDefinition->_getImageContainer()->mLayerImageList.get()[0].mImageList->getImage(0);
        cg::ImageBuffer newBuf(*old);
        auto textureSetDefinition_new = makeDefinitionFromImageBuffer(newLoc,&newBuf);
        for(auto& item:textureSetDefinition_new->_getImageContainer()->mLayerImageList.get()) {
            if(item.mImageList->isValid()){
                auto imagebuff = item.mImageList->getImage(0);
                if(imagebuff){
                    auto width = imagebuff->mImageDescription->mWidth;
                    auto height = imagebuff->mImageDescription->mHeight;
                    for(uint32 i = 0; i < width * height * 4; i += 4){
                        const_cast<cg::ImageBuffer *>(imagebuff)->mStorage->get()[i+3] = 128;
                    };
                    logger.debug("me is target change, type->{}",(uchar)item.mLayerType);
                }
            }
        }
        UploadImage::unhook();
        uploadTexture(newLoc,textureSetDefinition_new);
        static_cast<ModifyChestRenderer*>(renderers[BlockActorRendererId::Chest].get())->normalTex->mTexturePtrs->mUnkd1a95d.as<mce::TexturePtr>() = getTexture(newLoc, false, 0, cg::TextureSetLayerType::Color);
        logger.debug("is target:need->{}, ismiss->{}",textureSetDefinition->mNeedsDecompression, textureSetDefinition->mIsMissingTexture);
    };
    
    return origin(resourceLocation, textureSetDefinition);
}


LL_AUTO_TYPE_INSTANCE_HOOK(
    initModels,
    ll::memory::HookPriority::Normal, 
    BlockActorRenderDispatcher,
    &BlockActorRenderDispatcher::initializeBlockEntityRenderers,
    void,
    Bedrock::NotNullNonOwnerPtr<::GeometryGroup> const&                      geometryGroup,
    ::std::shared_ptr<::mce::TextureGroup>                                     textureGroup,
    ::BlockTessellator&                                                        blockTessellator,
    ::Bedrock::NotNullNonOwnerPtr<::ActorResourceDefinitionGroup const> const& actorResourceDefinitionGroup,
    ::ResourcePackManager&                                                     resourcePackManager,
    ::Bedrock::NotNullNonOwnerPtr<::ResourceLoadManager>                       resourceLoadManager,
    ::BaseGameVersion const&                                                   baseGameVersion,
    ::Experiments const&                                                       experiments
){
    origin(geometryGroup,
        std::move(textureGroup),
        blockTessellator,
        actorResourceDefinitionGroup,
        resourcePackManager,
        resourceLoadManager,
        baseGameVersion,
        experiments);
    if(renderers.empty()) {
        auto chestRenderer = std::make_unique<ModifyChestRenderer>(ll::service::getClientInstance()->getTextureGroup());
        renderers[BlockActorRendererId::Chest] = std::move(chestRenderer);
    }
}

}