#include "levischematic/schematic/block_actor/SchematicLecternRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicLecternRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicLecternRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicLecternRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicLecternRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
