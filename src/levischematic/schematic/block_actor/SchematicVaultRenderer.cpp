#include "levischematic/schematic/block_actor/SchematicVaultRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicVaultRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicVaultRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicVaultRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicVaultRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
