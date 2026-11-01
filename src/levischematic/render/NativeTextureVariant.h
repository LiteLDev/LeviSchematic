#pragma once

#include "ll/api/memory/Memory.h"
#include "mc/deps/core/checked_resource_service/ClientResourcePointer.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"

#include <cstddef>
#include <cstdint>

namespace dragon {
class ResolvedTextureResource;
}

namespace levischematic::render::detail {

using ResolvedTextureClientPointer = mce::ClientResourcePointer<dragon::ResolvedTextureResource>;


inline constexpr std::size_t kNativeTextureVariantTagOffset = 0x68;


struct NativeTextureVariantView {
    std::uint8_t                        tag      = 0xff;
    ResolvedTextureClientPointer const* resolved = nullptr;
};


inline NativeTextureVariantView inspectNativeTextureVariant(void const* storage) {
    if (storage == nullptr) {
        return {};
    }
    auto const tag = ll::memory::dAccess<std::uint8_t const>(storage, kNativeTextureVariantTagOffset);
    return {tag, tag == 2 ? static_cast<ResolvedTextureClientPointer const*>(storage) : nullptr};
}


inline mce::ServerResourcePointer<dragon::ResolvedTextureResource>
unwrapServerTexture(mce::ServerTexture::ServerResource const& serverTexture) {
    mce::ServerResourcePointer<dragon::ResolvedTextureResource> result;
    auto* variantBlock = serverTexture.mResourcePointerBlock.get();
    if (variantBlock == nullptr) return result;
    auto const view = inspectNativeTextureVariant(variantBlock->mResource.get());
    if (view.resolved == nullptr) return result;
    auto const& clientTexture = *view.resolved;
    auto*       textureBlock  = clientTexture.mResourcePointerBlock.get();
    if (textureBlock == nullptr) return result;
    ++textureBlock->mTrackingBlock.mCheckCount;
    if (textureBlock->mTrackingBlock.mIsValid) {
        result.mResourcePointerBlock = clientTexture.mResourcePointerBlock;
    }
    return result;
}

using ResolvedTextureBlock = mce::ResourceBlockTemplate<dragon::ResolvedTextureResource>;

} 
