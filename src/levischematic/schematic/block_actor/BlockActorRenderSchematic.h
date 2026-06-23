#pragma once

#include "levischematic/schematic/block_actor/BaseRenderSchematic.h"

#include "mc/world/level/block/actor/BlockActorRendererId.h"

#include <map>
#include <memory>

namespace mce {
class TextureGroup;
}

namespace levischematic::schematic::block_actor {

class BlockActorRenderSchematic {
public:
    struct TextureUploadTarget {
        BaseRenderSchematic::ResourceLocationMap* resource = nullptr;
        BaseRenderSchematic*                      renderer = nullptr;
        BlockActorRendererId                      id       = BlockActorRendererId::Default;
    };

    [[nodiscard]] bool                 renderersIsEmpty() const;
    [[nodiscard]] bool                 hasRenderer(BlockActorRendererId id) const;
    [[nodiscard]] BaseRenderSchematic* getRenderer(BlockActorRendererId id);

    void registerRenderer(BlockActorRendererId id, std::unique_ptr<BaseRenderSchematic> renderer);
    void renderSchematic(
        BlockActorRendererId              id,
        BaseActorRenderContext&           renderContext,
        BlockActorRenderDataForSchematic& blockEntityRenderData
    );
    void initAllTexturePtrToRenderer(mce::TextureGroup* textureGroup);

    [[nodiscard]] TextureUploadTarget findTextureUploadTarget(HashedString const& hash);
    void onTextureUploaded(TextureUploadTarget const& target, mce::TextureGroup* textureGroup);

    static BlockActorRenderSchematic& getInstance();

private:
    std::map<BlockActorRendererId, std::unique_ptr<BaseRenderSchematic>> mRenderers;
};

} // namespace levischematic::schematic::block_actor
