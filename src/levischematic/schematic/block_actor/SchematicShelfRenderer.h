#pragma once

#include "levischematic/schematic/block_actor/BaseRenderSchematic.h"

#include "mc/client/renderer/actor/ShelfRenderer.h"

namespace levischematic::schematic::block_actor {

class SchematicShelfRenderer : public BaseRenderSchematic, public ShelfRenderer {
public:
    void
    renderSchematic(BaseActorRenderContext& renderContext, BlockActorRenderDataForSchematic& blockEntityRenderData)
        override;

    const ResourceLocationMap* isThisRender(HashedString const& hash) override;

    void replaceTexture(mce::TextureGroup* textureGroup) override;

    bool isAllUpload() override;
};

} // namespace levischematic::schematic::block_actor
