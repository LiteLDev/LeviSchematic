#include "levischematic/schematic/block_actor/SchematicBeaconRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicBeaconRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicBeaconRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicBeaconRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicBeaconRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
