#include "levischematic/schematic/block_actor/SchematicConduitRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicConduitRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicConduitRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicConduitRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicConduitRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
