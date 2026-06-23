#include "levischematic/schematic/block_actor/BlockActorRenderSchematic.h"

#include "mc/client/renderer/TextureGroup.h"

namespace levischematic::schematic::block_actor {

bool BlockActorRenderSchematic::renderersIsEmpty() const { return mRenderers.empty(); }

bool BlockActorRenderSchematic::hasRenderer(BlockActorRendererId id) const { return mRenderers.contains(id); }

BaseRenderSchematic* BlockActorRenderSchematic::getRenderer(BlockActorRendererId id) {
    auto it = mRenderers.find(id);
    return it == mRenderers.end() ? nullptr : it->second.get();
}

void BlockActorRenderSchematic::registerRenderer(
    BlockActorRendererId                 id,
    std::unique_ptr<BaseRenderSchematic> renderer
) {
    mRenderers[id] = std::move(renderer);
}

void BlockActorRenderSchematic::renderSchematic(
    BlockActorRendererId              id,
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    if (auto* renderer = getRenderer(id)) {
        if(!renderer) return;
        renderer->renderSchematic(renderContext, blockEntityRenderData);
    }
}

void BlockActorRenderSchematic::initAllTexturePtrToRenderer(mce::TextureGroup* textureGroup) {
    for (auto& [id, renderer] : mRenderers) {
        (void)id;
        renderer->replaceTexture(textureGroup);
    }
}

BlockActorRenderSchematic::TextureUploadTarget
BlockActorRenderSchematic::findTextureUploadTarget(HashedString const& hash) {
    for (auto& [id, renderer] : mRenderers) {
        auto* resource = const_cast<BaseRenderSchematic::ResourceLocationMap*>(renderer->isThisRender(hash));
        if (resource) {
            return TextureUploadTarget{resource, renderer.get(), id};
        }
    }
    return {};
}

void BlockActorRenderSchematic::onTextureUploaded(TextureUploadTarget const& target, mce::TextureGroup* textureGroup) {
    if (!target.resource || !target.renderer) {
        return;
    }

    target.resource->mIsUpload = true;
    if (target.renderer->isAllUpload()) {
        target.renderer->replaceTexture(textureGroup);
    }
}

BlockActorRenderSchematic& BlockActorRenderSchematic::getInstance() {
    static BlockActorRenderSchematic blockActorRenderSchematic;
    return blockActorRenderSchematic;
}

} // namespace levischematic::schematic::block_actor
