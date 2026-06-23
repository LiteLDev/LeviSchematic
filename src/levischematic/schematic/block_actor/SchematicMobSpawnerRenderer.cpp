#include "levischematic/schematic/block_actor/SchematicMobSpawnerRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicMobSpawnerRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicMobSpawnerRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicMobSpawnerRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicMobSpawnerRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
