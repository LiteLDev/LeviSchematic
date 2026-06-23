#include "levischematic/schematic/block_actor/SchematicCopperGolemStatueRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicCopperGolemStatueRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap*
SchematicCopperGolemStatueRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicCopperGolemStatueRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicCopperGolemStatueRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
