#include "levischematic/schematic/block_actor/SchematicEndGatewayRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicEndGatewayRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicEndGatewayRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicEndGatewayRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicEndGatewayRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
