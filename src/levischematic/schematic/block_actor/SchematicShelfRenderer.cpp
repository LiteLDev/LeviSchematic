#include "levischematic/schematic/block_actor/SchematicShelfRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicShelfRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicShelfRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicShelfRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicShelfRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
