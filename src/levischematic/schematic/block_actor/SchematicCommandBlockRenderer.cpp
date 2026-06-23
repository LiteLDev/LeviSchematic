#include "levischematic/schematic/block_actor/SchematicCommandBlockRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicCommandBlockRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicCommandBlockRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicCommandBlockRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicCommandBlockRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
