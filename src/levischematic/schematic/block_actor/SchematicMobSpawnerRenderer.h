#pragma once

#include "levischematic/schematic/block_actor/BaseRenderSchematic.h"

#include "mc/client/renderer/blockactor/MobSpawnerRenderer.h"

namespace levischematic::schematic::block_actor {

class SchematicMobSpawnerRenderer : public BaseRenderSchematic, public MobSpawnerRenderer {
public:
    void
    renderSchematic(BaseActorRenderContext& renderContext, BlockActorRenderDataForSchematic& blockEntityRenderData)
        override;

    const ResourceLocationMap* isThisRender(HashedString const& hash) override;

    void replaceTexture(mce::TextureGroup* textureGroup) override;

    bool isAllUpload() override;
};

} // namespace levischematic::schematic::block_actor
