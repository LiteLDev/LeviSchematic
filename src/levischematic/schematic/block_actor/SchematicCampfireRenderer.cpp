#include "levischematic/schematic/block_actor/SchematicCampfireRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicCampfireRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicCampfireRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicCampfireRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicCampfireRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
