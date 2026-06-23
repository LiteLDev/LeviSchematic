#include "levischematic/schematic/block_actor/SchematicItemFrameRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicItemFrameRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicItemFrameRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicItemFrameRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicItemFrameRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
