#include "levischematic/schematic/block_actor/SchematicDecoratedPotRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicDecoratedPotRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicDecoratedPotRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicDecoratedPotRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicDecoratedPotRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
