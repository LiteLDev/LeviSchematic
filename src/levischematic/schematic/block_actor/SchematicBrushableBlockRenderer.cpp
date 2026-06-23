#include "levischematic/schematic/block_actor/SchematicBrushableBlockRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicBrushableBlockRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicBrushableBlockRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicBrushableBlockRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicBrushableBlockRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
