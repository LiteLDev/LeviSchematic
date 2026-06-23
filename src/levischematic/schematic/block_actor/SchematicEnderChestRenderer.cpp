#include "levischematic/schematic/block_actor/SchematicEnderChestRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicEnderChestRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicEnderChestRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicEnderChestRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicEnderChestRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
