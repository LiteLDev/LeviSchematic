#include "levischematic/schematic/block_actor/SchematicEnchanterRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicEnchanterRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicEnchanterRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicEnchanterRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicEnchanterRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
