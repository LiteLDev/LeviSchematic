#include "levischematic/schematic/block_actor/SchematicSkullRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicSkullRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicSkullRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicSkullRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicSkullRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
