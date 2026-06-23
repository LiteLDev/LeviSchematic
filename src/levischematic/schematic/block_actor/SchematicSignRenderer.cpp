#include "levischematic/schematic/block_actor/SchematicSignRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicSignRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicSignRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicSignRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicSignRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
