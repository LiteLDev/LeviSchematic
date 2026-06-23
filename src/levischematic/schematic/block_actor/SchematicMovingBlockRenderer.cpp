#include "levischematic/schematic/block_actor/SchematicMovingBlockRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicMovingBlockRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicMovingBlockRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicMovingBlockRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicMovingBlockRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
