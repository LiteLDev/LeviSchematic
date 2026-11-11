#include "BlockEntityHighlight.h"
#include "BlockEntityHighlightPolicy.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/utils/SystemUtils.h"
#include "mc/deps/core/utility/ServiceLocator.h"
#include "mc/deps/minecraft_renderer/framebuilder/FrameBuilder.h"
#include "mc/deps/minecraft_renderer/renderer/RenderMaterial.h"
#include "mc/platform/threading/TlsSlot.h"

#include <Windows.h>
#include <array>
#include <cstring>
#include <vector>

namespace levischematic::render {
bool registerProjectionCutoutPass();
void unregisterProjectionCutoutPass();
namespace {
std::uintptr_t            materialEntry    = 0;
std::uintptr_t            fallbackMaterialEntry = 0;
std::uintptr_t            itemInsertEntry = 0;
bool                      installed        = false;
std::string               unavailable      = "material hook not installed";
thread_local std::size_t* activeVariations = nullptr;
thread_local bool activeDeferred = false;




constexpr std::size_t recordStride = 200;
struct RegistryCursor {
    void*          frame   = nullptr;
    void*          item    = nullptr;
    std::uintptr_t records = 0;
    std::size_t    count   = 0;
    bool           valid   = false;
};

template <class T>
T readField(void const* object, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<std::byte const*>(object) + offset, sizeof(T));
    return value;
}

RegistryCursor cursor(void const* context) {
    RegistryCursor result;
    result.frame = readField<void*>(context, 16);
    if (!result.frame) return result;
    auto const* key =
        reinterpret_cast<Bedrock::Threading::TlsSlot const*>(static_cast<std::byte const*>(result.frame) + 2512 + 160);
    result.item  = key->getValue();
    result.valid = true;
    if (!result.item) return result; 
    auto const vector = readField<std::array<std::uintptr_t, 3>>(result.item, 16);
    result.records    = vector[0];
    result.valid      = vector[0] <= vector[1] && vector[1] <= vector[2] && (vector[1] - vector[0]) % recordStride == 0
                     && (vector[2] - vector[0]) % recordStride == 0 && (vector[0] || (!vector[1] && !vector[2]));
    if (result.valid) result.count = (vector[1] - vector[0]) / recordStride;
    return result;
}

bool supportedExecutable() {
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr, executable, 32768)) return false;
    DWORD      ignored = 0;
    auto const size    = GetFileVersionInfoSizeW(executable, &ignored);
    if (!size) return false;
    std::vector<std::byte> data(size);
    if (!GetFileVersionInfoW(executable, 0, size, data.data())) return false;
    VS_FIXEDFILEINFO* version = nullptr;
    UINT              bytes   = 0;
    return VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&version), &bytes) && version
        && bytes >= sizeof(*version) && version->dwFileVersionMS == MAKELONG(26, 1)
        && version->dwFileVersionLS == MAKELONG(5, 60);
}




LL_TYPE_INSTANCE_HOOK(
    ProjectionMaterialVariationHook,
    ll::memory::HookPriority::Normal,
    mce::RenderMaterial,
    materialEntry,
    void*,
    void*                   result,
    mce::MaterialRenderType type,
    bool                    pointLighting
) {
    if (activeVariations && activeDeferred && static_cast<unsigned char>(type) != 2) {
        type = static_cast<mce::MaterialRenderType>(2);
        ++*activeVariations;
    }
    return origin(result, type, pointLighting);
}





LL_STATIC_HOOK(
    ProjectionFallbackMaterialHook,
    ll::memory::HookPriority::Normal,
    fallbackMaterialEntry,
    void*,
    void* result,
    void const* context,
    void const* description,
    mce::MaterialRenderType type
) {
    if (activeVariations && activeDeferred && readField<bool>(description, 223)) {
        
        
        type = static_cast<mce::MaterialRenderType>(0);
        ++*activeVariations;
    } else if (activeVariations && activeDeferred && static_cast<unsigned char>(type) != 2) {
        type = static_cast<mce::MaterialRenderType>(2);
        ++*activeVariations;
    }
    return origin(result, context, description, type);
}







LL_STATIC_HOOK(
    ProjectionWorldItemInsertHook,
    ll::memory::HookPriority::Normal,
    itemInsertEntry,
    void,
    void* self,
    void const* context,
    void const* description
) {
    if (!activeVariations || !activeDeferred || readField<bool>(description, 121)) {
        origin(self, context, description);
        return;
    }
    alignas(8) std::array<std::byte, 384> item;
    std::memcpy(item.data(), description, item.size());
    item[122] = std::byte{1}; 
    item[125] = std::byte{0};
    ++*activeVariations;
    origin(self, context, item.data());
}

class MaterialScope {
    std::size_t* previous;
    bool previousDeferred;

public:
    explicit MaterialScope(std::size_t& count) : previous(activeVariations), previousDeferred(activeDeferred) {
        activeVariations = &count;
        auto builder = ServiceLocator<mce::framebuilder::FrameBuilder>::get();
        
        
        activeDeferred = builder && builder.get()->isDeferredEnabled();
    }
    ~MaterialScope() { activeVariations = previous; activeDeferred = previousDeferred; }
    MaterialScope(MaterialScope const&)            = delete;
    MaterialScope& operator=(MaterialScope const&) = delete;
};
} 

std::uintptr_t resolveProjectionEntry(char const* symbol, char const* signatureText) {
    if (!supportedExecutable()) return 0;
    if (symbol && *symbol)
        if (auto* entry = ll::memory::SymbolView{symbol}.resolve(true)) return reinterpret_cast<std::uintptr_t>(entry);
    auto signature = ll::memory::Signature::parse(signatureText);
    auto image = ll::sys_utils::getImageRange();
    ll::memory::SignatureView view{signature};
    auto* entry = static_cast<std::byte const*>(view.resolve(image, true));
    if (!entry || entry < image.data() || entry >= image.data()+image.size()
        || view.resolve(image.subspan(static_cast<size_t>(entry-image.data())+1), true)) return 0;
    return reinterpret_cast<std::uintptr_t>(entry);
}

void registerBlockEntityHighlight() {
    if (installed) return;
    if (!supportedExecutable()) {
        unavailable = "unverified executable (requires 1.26.60.5)";
        return;
    }
    materialEntry = reinterpret_cast<std::uintptr_t>(ll::memory::SymbolView{
        "?getActorVariation@RenderMaterial@mce@@QEAA?AUActorMaterialVariation@2@W4MaterialRenderType@2@_N@Z"
    }
                                                         .resolve(true));
    if (!materialEntry) {
        
        auto signature = ll::memory::Signature::parse(
            "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 68 01 00 00 48 8D AC 24 ? ? ? ? "
            "48 C7 85 ? ? ? ? ? ? ? ? 45 89 CF"
        );
        auto const                image = ll::sys_utils::getImageRange();
        ll::memory::SignatureView view{signature};
        auto const*               match = static_cast<std::byte const*>(view.resolve(image, true));
        if (match && match >= image.data() && match < image.data() + image.size()
            && !view.resolve(image.subspan(static_cast<std::size_t>(match - image.data()) + 1), true)) {
            materialEntry = reinterpret_cast<std::uintptr_t>(match);
        }
    }
    fallbackMaterialEntry = resolveProjectionEntry(
        "?makeMaterialFilter@bgfxbridge@framebuilder@mce@@YA?AV?$optional@UMaterialFilter@components@frameobject@dragon@@@std@@AEBUEntityCreationContext@123@AEBURenderMeshFallbackDescription@23@W4MaterialRenderType@3@@Z",
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 98 04 00 00 48 8D AC 24 ? ? ? ? "
        "0F 29 BD ? ? ? ? 0F 29 B5 ? ? ? ? 48 C7 85 ? ? ? ? ? ? ? ? 44 89 CB"
    );
    bool const actorHooked = materialEntry && ProjectionMaterialVariationHook::hook() == 0;
    bool const fallbackHooked = fallbackMaterialEntry && ProjectionFallbackMaterialHook::hook() == 0;
    itemInsertEntry = resolveProjectionEntry("", "55 41 57 41 56 41 55 41 54 56 57 53 B8 58 21 00 00");
    bool const itemHooked = itemInsertEntry && ProjectionWorldItemInsertHook::hook() == 0;
    installed = actorHooked && fallbackHooked && itemHooked;
    if (!installed) {
        if (actorHooked) ProjectionMaterialVariationHook::unhook();
        if (fallbackHooked) ProjectionFallbackMaterialHook::unhook();
        if (itemHooked) ProjectionWorldItemInsertHook::unhook();
    }
    if (installed && !registerProjectionCutoutPass()) {
        ProjectionMaterialVariationHook::unhook();
        ProjectionFallbackMaterialHook::unhook();
        ProjectionWorldItemInsertHook::unhook();
        installed = false;
    }
    unavailable = installed ? "" : "material entry unavailable or hook installation failed";
}

void unregisterBlockEntityHighlight() {
    unregisterProjectionCutoutPass();
    if (installed) ProjectionMaterialVariationHook::unhook();
    if (installed) ProjectionFallbackMaterialHook::unhook();
    if (installed) ProjectionWorldItemInsertHook::unhook();
    installed     = false;
    materialEntry = 0;
    fallbackMaterialEntry = 0;
    itemInsertEntry = 0;
    unavailable   = "material hook not installed";
}

BlockEntityHighlightStats submitBlockEntityHighlight(std::function<std::size_t()> const& dispatch) {
    BlockEntityHighlightStats stats;
    if (!installed) {
        stats.skipped = unavailable;
        return stats;
    }
    auto service = ServiceLocator<mce::framebuilder::FrameBuilder>::get();
    if (!service || !service.get()->enabled()) {
        stats.skipped = "FrameBuilder unavailable";
        return stats;
    }
    auto&          builder = *service.get();
    RegistryCursor before;
    builder.custom([&](auto const& context) { before = cursor(&context); });
    if (!before.valid) {
        stats.skipped = "invalid frame registry";
        return stats;
    }
    
    
    MaterialScope scope(stats.forwardVariations);
    stats.submitted = dispatch();
    builder.custom([&](auto const& context) {
        auto const after = cursor(&context);
        if (!after.valid || after.frame != before.frame || (before.item && after.item != before.item)
            || after.count < before.count) {
            stats.skipped = "frame registry changed during dispatch";
            return;
        }
        for (auto i = before.count; i < after.count; ++i) {
            auto* record = reinterpret_cast<void*>(after.records + i * recordStride);
            auto  mask   = readField<detail::HighlightMask>(record, 0);
            if (detail::routeBlockEntityHighlight(mask)) ++stats.highlighted;
            std::memcpy(record, mask.data(), sizeof(mask));
        }
    });
    return stats;
}
} 
