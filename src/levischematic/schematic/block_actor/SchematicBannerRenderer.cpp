#include "levischematic/schematic/block_actor/SchematicBannerRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicBannerRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicBannerRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicBannerRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicBannerRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
