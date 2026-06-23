#pragma once

#include "levischematic/schematic/block_actor/BaseRenderSchematic.h"

#include "mc/client/renderer/blockactor/ChestRenderer.h"

#include <array>

namespace mce {
class TextureGroup;
}

namespace levischematic::schematic::block_actor {

class SchematicChestRenderer : public BaseRenderSchematic, public ChestRenderer {
    std::array<ResourceLocationMap, 13> textureMap;
public:

    explicit SchematicChestRenderer(std::shared_ptr<::mce::TextureGroup> textureGroup, float transparency);

    void
    renderSchematic(BaseActorRenderContext& renderContext, BlockActorRenderDataForSchematic& blockEntityRenderData)
        override;

    const ResourceLocationMap* isThisRender(HashedString const& hash) override;

    bool isAllUpload() override;

    void replaceTexture(mce::TextureGroup* textureGroup) override;
};

} // namespace levischematic::schematic::block_actor
