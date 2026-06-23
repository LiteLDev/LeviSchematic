#include "levischematic/schematic/block_actor/SchematicTrialSpawnerRenderer.h"

namespace levischematic::schematic::block_actor {

void SchematicTrialSpawnerRenderer::renderSchematic(
    BaseActorRenderContext&           renderContext,
    BlockActorRenderDataForSchematic& blockEntityRenderData
) {
    (void)renderContext;
    (void)blockEntityRenderData;
}

const BaseRenderSchematic::ResourceLocationMap* SchematicTrialSpawnerRenderer::isThisRender(HashedString const& hash) {
    (void)hash;
    return nullptr;
}

void SchematicTrialSpawnerRenderer::replaceTexture(mce::TextureGroup* textureGroup) {
    (void)textureGroup;
}

bool SchematicTrialSpawnerRenderer::isAllUpload() {
    return true;
}

} // namespace levischematic::schematic::block_actor
