#include "levischematic/schematic/block_actor/SchematicChalkboardRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicChalkboardRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicChalkboardRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicChalkboardRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicChalkboardRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
