#include "levischematic/schematic/block_actor/SchematicHangingSignRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicHangingSignRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicHangingSignRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicHangingSignRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicHangingSignRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
