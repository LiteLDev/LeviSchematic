
#include "ll/api/memory/Hook.h"
#include "ll/api/memory/Memory.h"
#include "ll/api/service/TargetedBedrock.h"

#include "levischematic/LeviSchematic.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/model/geom/ModelPart.h"
#include "mc/client/model/models/ChestModel.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/BaseActorRenderer.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/actor/ActorTextureInfo.h"
#include "mc/client/renderer/blockactor/BlockActorRenderDispatcher.h"
#include "mc/client/renderer/blockactor/BlockActorRenderer.h"
#include "mc/client/renderer/blockactor/ChestRenderer.h"
#include "mc/client/renderer/game/LevelRendererCamera.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/renderer/RenderMaterialInfo.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/resource/ResourceLocationPair.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/core_graphics/ImageBuffer.h"
#include "mc/deps/core_graphics/TextureSetLayerType.h"
#include "mc/deps/core_graphics/enums/BlendTarget.h"
#include "mc/deps/minecraft_renderer/framebuilder/CSSGameplayFlags.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/RenderMaterial.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/CopperType.h"
#include "mc/world/level/block/VanillaBlockTypeIds.h"
#include "mc/world/level/block/actor/BlockActorRendererId.h"
#include "mc/world/level/block/registry/BlockTypeRegistry.h"
#include "mc/world/level/block/states/BuiltInBlockStates.h"


#include "mc/deps/core/container/DenseEnumMap.h"
#include "mc/deps/core_graphics/ImageResource.h"
#include "mc/deps/core_graphics/TextureSetDefinition.h"
#include "mc/deps/core_graphics/TextureSetImageContainer.h"
#include "mc/deps/core_graphics/TextureSetLayerImageMipList.h"
#include "mc/util/texture_set_helpers/TextureSetDefinitionLoader.h"
#include "mc/world/level/block/actor/ChestBlockActor.h"
#include "mc/world/level/block/CopperBehavior.h"
#include "mc/world/level/BlockSource.h"


#include <Windows.h>
#include <cstdlib>
#include <minwinbase.h>
#include <utility>

// 头文件中暂缺的类型定义，在使用的时候放在前面即可
namespace mce::framebuilder {
struct CustomSurfaceShaderMetadata {
    uint             mHash;
    CSSGameplayFlags mGameplayFlags;
};
} // namespace mce::framebuilder

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

namespace block_actor_test {
// 调试使用的日志，可以忽略，下面用到的地方也可以去掉
auto& logger = levischematic::LeviSchematic::getInstance().getSelf().getLogger();


struct BlockActorRenderDataForSchematic {
    Vec3         renderPosition;
    BlockPos     pos;
    const Block& block;
    BlockActor*  entity;
};

// 构建一个虚类，方便利用重载渲染不同BlockActor
class BaseRenderSchematic {
public:
    struct ResourceLocationMap {
        ResourceLocation vanila;
        ResourceLocation blendRes;
        bool mIsUpload=false;
        explicit ResourceLocationMap(ResourceLocation vanilaRes) {
            vanila   = vanilaRes;
            blendRes = ResourceLocation(Core::PathView(vanilaRes.mPath->value + "_blendqiu"));
        };
        ResourceLocationMap() = default;
    };
    // 透明度
    float mTransparency = 1.0f;

    virtual void
    renderSchematic(BaseActorRenderContext& renderContext, BlockActorRenderDataForSchematic& blockEntityRenderData) = 0;

    virtual const ResourceLocationMap* isThisRender(HashedString const& hash) = 0;

    virtual void replaceTexture(mce::TextureGroup* textureGroup) = 0;

    virtual bool isAllUpload() = 0;
};

class BlockActorRenderSchematic {
public:
    // 保存全部类型的render容器
    std::map<BlockActorRendererId, std::unique_ptr<BaseRenderSchematic>> renderers;

    // // 当所有的texture上传后，把所有render内的textureptr替换新的
    void initAllTexturePtrToRenderer(mce::TextureGroup* textureGroup) {
        for (auto it = renderers.begin(); it != renderers.end(); ++it) {
            it->second->replaceTexture(textureGroup);
        }
    };

    std::pair<const BaseRenderSchematic::ResourceLocationMap*, BlockActorRendererId> isSchematicRenderer(HashedString const& hash){
        for(auto it = renderers.begin(); it!=renderers.end(); ++it) {
            if(it->second->isThisRender(hash))
                return std::make_pair(it->second->isThisRender(hash), it->first);
        }
        return std::make_pair(nullptr, BlockActorRendererId::Default);
    };
    static BlockActorRenderSchematic& getInstance() {
        static BlockActorRenderSchematic blockActorRenderSchematic{};
        return blockActorRenderSchematic;
    };
};

// 管理类实例
// static BlockActorRenderSchematic blockActorRenderSchematic;


LL_AUTO_TYPE_INSTANCE_HOOK(
    RenderChestActorTest,
    ll::memory::HookPriority::Normal,
    LevelRendererCamera,
    &LevelRendererCamera::$renderBlockEntities,
    void,
    BaseActorRenderContext& renderContext,
    bool                    renderAlphaLayer
) {
    origin(renderContext, renderAlphaLayer);
    if (renderAlphaLayer && !BlockActorRenderSchematic::getInstance().renderers.empty()) {
        BlockPos pos(2, 1, 1); // 渲染的世界位置
        BlockPos pos1(1, 1, 1);
        BlockPos pos2(2, 1, 0);
        BlockPos pos3(2, 1, -1);
        BlockPos pos4(2, 1, -2);
        BlockPos pos5(2, 1, -3);
        BlockPos pos6(2, 1, -4);

        BlockPos pose(0, 1, 1);
        BlockPos pos1e(-1, 1, 1);
        BlockPos pos2e(0, 1, 0);
        BlockPos pos3e(0, 1, -1);
        BlockPos pos4e(0, 1, -2);
        BlockPos pos5e(0, 1, -3);
        BlockPos pos6e(0, 1, -4);

        Vec3     renderpos;
        renderpos.x = pos.x - renderContext.mCameraTargetPosition->x;
        renderpos.y = pos.y - renderContext.mCameraTargetPosition->y;
        renderpos.z = pos.z - renderContext.mCameraTargetPosition->z;

        Vec3     renderpos1;
        renderpos1.x = pos1.x - renderContext.mCameraTargetPosition->x;
        renderpos1.y = pos1.y - renderContext.mCameraTargetPosition->y;
        renderpos1.z = pos1.z - renderContext.mCameraTargetPosition->z;

        Vec3     renderpos2;
        renderpos2.x = pos2.x - renderContext.mCameraTargetPosition->x;
        renderpos2.y = pos2.y - renderContext.mCameraTargetPosition->y;
        renderpos2.z = pos2.z - renderContext.mCameraTargetPosition->z;

        Vec3     renderpos3;
        renderpos3.x = pos3.x - renderContext.mCameraTargetPosition->x;
        renderpos3.y = pos3.y - renderContext.mCameraTargetPosition->y;
        renderpos3.z = pos3.z - renderContext.mCameraTargetPosition->z;

        Vec3     renderpos4;
        renderpos4.x = pos4.x - renderContext.mCameraTargetPosition->x;
        renderpos4.y = pos4.y - renderContext.mCameraTargetPosition->y;
        renderpos4.z = pos4.z - renderContext.mCameraTargetPosition->z;

        Vec3     renderpos5;
        renderpos5.x = pos5.x - renderContext.mCameraTargetPosition->x;
        renderpos5.y = pos5.y - renderContext.mCameraTargetPosition->y;
        renderpos5.z = pos5.z - renderContext.mCameraTargetPosition->z;

        Vec3     renderpos6;
        renderpos6.x = pos6.x - renderContext.mCameraTargetPosition->x;
        renderpos6.y = pos6.y - renderContext.mCameraTargetPosition->y;
        renderpos6.z = pos6.z - renderContext.mCameraTargetPosition->z;

        // 渲染使用的对应方块的Block类
        // auto& chestBlock = BlockTypeRegistry::get().getDefaultBlockState(VanillaBlockTypeIds::Chest());

        // block_actor_test::BlockActorRenderDataForSchematic data{renderpos, pos, chestBlock, mViewRegion->get()->getBlockEntity({0, 1, 1})};
        block_actor_test::BlockActorRenderDataForSchematic data{renderpos, pos, mViewRegion->get()->getBlock(pose), mViewRegion->get()->getBlockEntity(pose)};
        block_actor_test::BlockActorRenderDataForSchematic data1{renderpos1, pos1, mViewRegion->get()->getBlock(pos1e), mViewRegion->get()->getBlockEntity(pos1e)};
        block_actor_test::BlockActorRenderDataForSchematic data2{renderpos2, pos2, mViewRegion->get()->getBlock(pos2e), mViewRegion->get()->getBlockEntity(pos2e)};
        block_actor_test::BlockActorRenderDataForSchematic data3{renderpos3, pos3, mViewRegion->get()->getBlock(pos3e), mViewRegion->get()->getBlockEntity(pos3e)};
        block_actor_test::BlockActorRenderDataForSchematic data4{renderpos4, pos4, mViewRegion->get()->getBlock(pos4e), mViewRegion->get()->getBlockEntity(pos4e)};
        block_actor_test::BlockActorRenderDataForSchematic data5{renderpos5, pos5, mViewRegion->get()->getBlock(pos5e), mViewRegion->get()->getBlockEntity(pos5e)};
        block_actor_test::BlockActorRenderDataForSchematic data6{renderpos6, pos6, mViewRegion->get()->getBlock(pos6e), mViewRegion->get()->getBlockEntity(pos6e)};
        BlockActorRenderSchematic::getInstance().renderers[BlockActorRendererId::Chest]->renderSchematic(renderContext, data);
        BlockActorRenderSchematic::getInstance().renderers[BlockActorRendererId::Chest]->renderSchematic(renderContext, data1);
        BlockActorRenderSchematic::getInstance().renderers[BlockActorRendererId::Chest]->renderSchematic(renderContext, data2);
        BlockActorRenderSchematic::getInstance().renderers[BlockActorRendererId::Chest]->renderSchematic(renderContext, data3);
        BlockActorRenderSchematic::getInstance().renderers[BlockActorRendererId::Chest]->renderSchematic(renderContext, data4);
        BlockActorRenderSchematic::getInstance().renderers[BlockActorRendererId::Chest]->renderSchematic(renderContext, data5);
        BlockActorRenderSchematic::getInstance().renderers[BlockActorRendererId::Chest]->renderSchematic(renderContext, data6);
    }
    if (BlockActorRenderSchematic::getInstance().renderers.empty()) {
        logger.debug("renderer is empty!");
    }
}

// 构建新的cg::TextureSetDefinition，将会上传到GPU
std::shared_ptr<::cg::TextureSetDefinition>
makeDefinitionFromImageBuffer(ResourceLocation const& res, cg::ImageBuffer const* buffer) {
    HMODULE hModule = GetModuleHandle(nullptr);
    // ResourceLocationPair(const ResourceLocation&, const PackIdVersion&, int) rva
    void* resourceLocationPair_va  = (void*)(reinterpret_cast<BYTE*>(hModule) + 0x0);
    void* resourceLocationPair_ptr = malloc(0x0);
    ZeroMemory(resourceLocationPair_ptr, 0x0);
    void* packid = malloc(0x0);
    ZeroMemory(packid, 0x0);
    ll::memory::addressCall<void, void*, ResourceLocation const&, void*, int>(
        resourceLocationPair_va,
        resourceLocationPair_ptr,
        res,
        packid,
        0
    );

    void* func_ptr = (void*)(reinterpret_cast<BYTE*>(hModule) + 0x0);
    return ll::memory::addressCall<
        std::shared_ptr<::cg::TextureSetDefinition>,
        ResourceLocationPair*,
        cg::ImageBuffer const*,
        bool,
        bool>(func_ptr, (ResourceLocationPair*)resourceLocationPair_ptr, buffer, false, false);
};


class ModifyChestRenderer : public BaseRenderSchematic, public ChestRenderer {
public:
    // 原版纹理对应的路径，作为查找使用
    std::array<ResourceLocationMap, 13> textureMap;

    explicit ModifyChestRenderer(std::shared_ptr<::mce::TextureGroup> textureGroup, float transparency)
    : ChestRenderer(std::move(textureGroup)) {
        mTransparency       = transparency;
        auto newmaterialptr = mce::MaterialPtr();
        auto newmater_ptr =
            std::make_unique<mce::RenderMaterial>(*mChestModel->mDefaultMaterial->mRenderMaterialInfoPtr->mPtr);
        newmaterialptr.mRenderMaterialInfoPtr              = std::make_shared<mce::RenderMaterialInfo>();
        newmaterialptr.mRenderMaterialInfoPtr->mHashedName = HashedString("chest.skinning_blend");
        newmaterialptr.mRenderMaterialInfoPtr->mPtr        = std::move(newmater_ptr);


        newmaterialptr.mRenderMaterialInfoPtr->mPtr->mStateMask                         = mce::RenderState::Blending;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->enableBlend = true;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->enableAlphaToCoverage = false;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->blendSource = mce::BlendTarget::SourceAlpha;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->blendDestination =
            mce::BlendTarget::OneMinusSrcAlpha;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->alphaSource = mce::BlendTarget::SourceAlpha;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->alphaDestination =
            mce::BlendTarget::OneMinusSrcAlpha;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->blendStateDescription->colorWriteMask          = 0xF;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->depthStencilStateDescription->depthTestEnabled = true;
        newmaterialptr.mRenderMaterialInfoPtr->mPtr->depthStencilStateDescription->depthWriteMask =
            (mce::DepthWriteMask)0x1;

        mChestModel->mDefaultMaterial = newmaterialptr;

        mChestModel->setModelMaterial(mChestModel->mDefaultMaterial);
        mChestModel->mMaterialVariants->mSkinningColorMaterialPtr = newmaterialptr;

        mLargeChestModel->mDefaultMaterial = newmaterialptr;
        mLargeChestModel->setModelMaterial(newmaterialptr);
        mLargeChestModel->mMaterialVariants->mSkinningColorMaterialPtr = newmaterialptr;

        mChestModel->mLid->mRot->x                                = 0;
        // 普通箱子
        textureMap[0] = ResourceLocationMap(normalTex->mResourceLocations->mColorLocation);
        normalTex->mResourceLocations->mColorLocation = textureMap[0].blendRes;

        textureMap[1] = ResourceLocationMap(largeTex->mResourceLocations->mColorLocation);
        largeTex->mResourceLocations->mColorLocation = textureMap[1].blendRes;

        textureMap[2] = ResourceLocationMap(trappedTex->mResourceLocations->mColorLocation);
        trappedTex->mResourceLocations->mColorLocation = textureMap[2].blendRes;

        textureMap[3] = ResourceLocationMap(trappedLargeTex->mResourceLocations->mColorLocation);
        trappedLargeTex->mResourceLocations->mColorLocation = textureMap[3].blendRes;

        textureMap[4] = ResourceLocationMap(enderTex->mResourceLocations->mColorLocation);
        enderTex->mResourceLocations->mColorLocation = textureMap[4].blendRes;

        // 铜箱子
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
    };
    void renderSchematic(BaseActorRenderContext& renderContext, BlockActorRenderDataForSchematic& blockEntityRenderData)
        override {
        // ─── 提取渲染上下文基本字段 ────────────────────────────────────────
        ScreenContext& screenContext  = renderContext.mScreenContext;
        const Vec3     renderPosition = blockEntityRenderData.renderPosition;

        // ─── 获取朝向（CardinalDirection 方块状态）────────────────────────
        const Block& block     = blockEntityRenderData.block;
        int          facingDir = 0; // 默认朝向
        if(!blockEntityRenderData.entity)
            return;

        // 在 BlockType 的状态树中查找 CardinalDirection
        BlockType* blockType   = block.mBlockType;
        bool       foundInTree = false;

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
            facingDir        = *block.getState<int>(stateId);
        }

        // ─── 压入世界矩阵并设置平移 ─────────────────────────────────────────
        MatrixStack::MatrixStackRef worldMatRef = renderContext.mScreenContext.camera.worldMatrixStack->push(false);
        auto*                       mat         = worldMatRef.mat;

        // 将矩阵平移到方块中心（+1 是方块尺寸，Minecraft 坐标中心偏移）

        glm::vec3 negTarget         = {renderPosition.x, renderPosition.y + 1.0f, renderPosition.z + 1.0f};
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
        case 1:
            rotationY = 90.0f;
            break; // 朝西
        case 2:
            rotationY = 180.0f;
            break; // 朝北
        case 3:
            rotationY = -90.0f;
            break; // 朝东
        case 0:
        default:
            rotationY = 0.0f;
            break; // 朝南（默认）
        }

        // 绕 Y 轴旋转（角度转弧度：deg * PI/180）
        mat->_m = glm::rotate(mat->_m.get(), rotationY * 0.017453292f, glm::vec3(0.0f, 1.0f, 0.0f));

        // 旋转后重新计算中心，并将原点移至方块中心
        mat->_m = glm::translate(mat->_m.get(), glm::vec3(-0.5f, -0.5f, -0.5f));

        auto* chestTexture = &normalTex.get();
        auto* chestModel = &mChestModel.get();
        auto chestBlockActor = (ChestBlockActor*)blockEntityRenderData.entity;
        if(chestBlockActor->mLargeChestPaired) {
            chestModel = &mLargeChestModel.get();
            mat->translate(glm::vec3(0.5f,0.0f,0.0f));
        }
        if(blockEntityRenderData.block.getBlockType().mNameInfo->mFullName.get() == VanillaBlockTypeIds::TrappedChest()) {
            chestTexture = &trappedLargeTex.get();
            if(!chestBlockActor->mLargeChestPaired) {
                chestTexture = &trappedTex.get();
            }
        }else {
            auto copperbehavior = blockEntityRenderData.block.getBlockType().tryGetCopperBehavior();
            if(copperbehavior) {
                chestTexture = &mCopperTextures.get()[copperbehavior->mType];
                if(chestBlockActor->mLargeChestPaired) {
                    chestTexture = &mLargeCopperTextures.get()[copperbehavior->mType];
                }
            } else if (chestBlockActor->mIsGlobalChest) {
                chestTexture = &enderTex.get();
            } else {
                if(chestBlockActor->mLargeChestPaired) {
                    chestTexture = &largeTex.get();
                }
            }
        }

        // ─── 构建 RenderMetadata ──────────────────────────────────────
        {
            dragon::RenderMetadata renderMetadata{
                blockEntityRenderData.pos.hashCode(),
                {blockEntityRenderData.block.getBlockType().mNameInfo->mFullName->getHash(),
                                 (mce::framebuilder::CSSGameplayFlags)1},
                false,
                renderContext.mOffscreenCaptureDescription
            };

            // ─── 提交模型渲染 ─────────────────────────────────────────────
            _renderModel(screenContext, renderMetadata, *chestModel, *chestTexture);
        }
    }
    // 判断是不是属于该Render的纹理
    const ResourceLocationMap* isThisRender(HashedString const& hash) override {
        for (const auto& item : textureMap) {
            if (item.vanila.getHashedPath() == hash) return &item;
        }
        return nullptr;
    }
    bool isAllUpload() override {
        for (const auto& item : textureMap) {
            if (!item.mIsUpload) return false;
        }
        return true;
    }
    void replaceTexture(mce::TextureGroup* textureGroup) override {
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
            textureMap[3].mIsUpload=false;

        enderTex->mTexturePtrs->mColorTexture =
            textureGroup
                ->getTexture(enderTex->mResourceLocations->mColorLocation, false, 0, cg::TextureSetLayerType::Color);
            textureMap[4].mIsUpload=false;

        // 铜箱子
        auto assignTexture = [&](CopperType type) {
            mCopperTextures.get()[type].mTexturePtrs->mColorTexture =
            textureGroup
                ->getTexture(mCopperTextures.get()[type].mResourceLocations->mColorLocation, false, 0, cg::TextureSetLayerType::Color);

            mLargeCopperTextures.get()[type].mTexturePtrs->mColorTexture =
            textureGroup
                ->getTexture(mLargeCopperTextures.get()[type].mResourceLocations->mColorLocation, false, 0, cg::TextureSetLayerType::Color);
        };
        assignTexture(CopperType::Default);
        assignTexture(CopperType::Exposed);
        assignTexture(CopperType::Weathered);
        assignTexture(CopperType::Oxidized);
    }
};

LL_AUTO_TYPE_INSTANCE_HOOK(
    UploadImage,
    ll::memory::HookPriority::Normal,
    mce::TextureGroup,
    &mce::TextureGroup::uploadTexture,
    BedrockTexture&,
    ResourceLocation const&                                      resourceLocation,
    gsl::not_null<::std::shared_ptr<::cg::TextureSetDefinition>> textureSetDefinition
) {
    if(BlockActorRenderSchematic::getInstance().renderers.empty()) {
        return origin(resourceLocation, textureSetDefinition);
    }
    auto newRes       = BlockActorRenderSchematic::getInstance().isSchematicRenderer(resourceLocation.getHashedPath());
    if (newRes.first && !textureSetDefinition->_getImageContainer()->mLayerImageList->empty()) {
        int  transparency = BlockActorRenderSchematic::getInstance().renderers[newRes.second]->mTransparency * 255;
        auto old = textureSetDefinition->_getImageContainer()->mLayerImageList.get()[0].mImageList->getImage(0);
        cg::ImageBuffer newBuf(*old);
        auto            textureSetDefinition_new = makeDefinitionFromImageBuffer(newRes.first->blendRes, &newBuf);
        for (auto& item : textureSetDefinition_new->_getImageContainer()->mLayerImageList.get()) {
            if (item.mImageList->isValid() && item.mLayerType == cg::TextureSetLayerType::Color) {
                auto imagebuff = item.mImageList->getImage(0);
                if (imagebuff) {
                    auto width  = imagebuff->mImageDescription->mWidth;
                    auto height = imagebuff->mImageDescription->mHeight;
                    for (uint32 i = 0; i < width * height * 4; i += 4) {
                        const_cast<cg::ImageBuffer*>(imagebuff)->mStorage->get()[i + 3] = transparency;
                    };
                    logger.debug("me is target change, type->{}, text->{}", (uchar)item.mLayerType, newRes.first->blendRes.getFullPath().value);
                }
            }
        }
        UploadImage::unhook();
        uploadTexture(newRes.first->blendRes, textureSetDefinition_new);
        const_cast<BaseRenderSchematic::ResourceLocationMap*>(newRes.first)->mIsUpload = true;
        UploadImage::hook();
        if(BlockActorRenderSchematic::getInstance().renderers[newRes.second]->isAllUpload()) {
            BlockActorRenderSchematic::getInstance().renderers[newRes.second]->replaceTexture(this);
        }
    };

    return origin(resourceLocation, textureSetDefinition);
}


LL_AUTO_TYPE_INSTANCE_HOOK(
    initModels,
    ll::memory::HookPriority::Normal,
    BlockActorRenderDispatcher,
    &BlockActorRenderDispatcher::initializeBlockEntityRenderers,
    void,
    Bedrock::NotNullNonOwnerPtr<::GeometryGroup> const&                        geometryGroup,
    ::std::shared_ptr<::mce::TextureGroup>                                     textureGroup,
    ::BlockTessellator&                                                        blockTessellator,
    ::Bedrock::NotNullNonOwnerPtr<::ActorResourceDefinitionGroup const> const& actorResourceDefinitionGroup,
    ::ResourcePackManager&                                                     resourcePackManager,
    ::Bedrock::NotNullNonOwnerPtr<::ResourceLoadManager>                       resourceLoadManager,
    ::BaseGameVersion const&                                                   baseGameVersion,
    ::Experiments const&                                                       experiments
) {
    origin(
        geometryGroup,
        std::move(textureGroup),
        blockTessellator,
        actorResourceDefinitionGroup,
        resourcePackManager,
        resourceLoadManager,
        baseGameVersion,
        experiments
    );
    if (BlockActorRenderSchematic::getInstance().renderers.empty()) {
        auto chestRenderer =
            std::make_unique<ModifyChestRenderer>(ll::service::getClientInstance()->getTextureGroup(), 0.5);
        BlockActorRenderSchematic::getInstance().renderers[BlockActorRendererId::Chest] = std::move(chestRenderer);
    }
}

} // namespace block_actor_test