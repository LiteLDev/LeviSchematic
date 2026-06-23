#pragma once

#include "levischematic/schematic/block_actor/BlockActorRenderDataForSchematic.h"

#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/string/HashedString.h"

#include <utility>

namespace mce {
class TextureGroup;
}

namespace levischematic::schematic::block_actor {

class BaseRenderSchematic {
public:
    struct ResourceLocationMap {
        ResourceLocation vanilla;
        ResourceLocation blendRes;
        bool             mIsUpload = false;

        ResourceLocationMap();
        explicit ResourceLocationMap(ResourceLocation vanillaRes);
        ResourceLocationMap(ResourceLocation vanillaRes, ResourceLocation blendResource);
    };

    float mTransparency = 1.0f;

    virtual ~BaseRenderSchematic();

    virtual void
    renderSchematic(BaseActorRenderContext& renderContext, BlockActorRenderDataForSchematic& blockEntityRenderData) = 0;

    virtual const ResourceLocationMap* isThisRender(HashedString const& hash) = 0;

    virtual void replaceTexture(mce::TextureGroup* textureGroup) = 0;

    virtual bool isAllUpload() = 0;
};

} // namespace levischematic::schematic::block_actor
