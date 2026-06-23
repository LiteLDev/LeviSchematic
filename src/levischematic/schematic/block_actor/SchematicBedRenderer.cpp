#include "levischematic/schematic/block_actor/SchematicBedRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicBedRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicBedRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicBedRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicBedRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
