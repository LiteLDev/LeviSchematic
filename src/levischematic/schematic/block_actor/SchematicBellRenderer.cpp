#include "levischematic/schematic/block_actor/SchematicBellRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicBellRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicBellRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicBellRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicBellRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
