#include "levischematic/schematic/block_actor/SchematicShulkerBoxRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicShulkerBoxRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicShulkerBoxRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicShulkerBoxRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicShulkerBoxRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
