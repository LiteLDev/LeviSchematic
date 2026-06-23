#include "levischematic/schematic/block_actor/SchematicPistonArmRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicPistonArmRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicPistonArmRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicPistonArmRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicPistonArmRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
