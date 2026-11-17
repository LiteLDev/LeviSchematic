#include "levischematic/render/blockentity/BlockEntityHighlight.h"
#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/render/blockentity/BlockEntityHighlightPolicy.h"
#include "levischematic/render/chunk/model/ProjectionLegacyItemPolicy.h"
#include "levischematic/render/chunk/model/ProjectionMeshDefaults.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/utils/SystemUtils.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/core/utility/ServiceLocator.h"
#include "mc/deps/minecraft_renderer/constantbuffers/GlobalConstantBuffers.h"
#include "mc/deps/minecraft_renderer/framebuilder/FrameBuilder.h"
#include "mc/deps/minecraft_renderer/framebuilder/RenderItemInHandDescription.h"
#include "mc/deps/minecraft_renderer/game/ItemContextFlags.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include "mc/deps/minecraft_renderer/renderer/MeshGenUtil.h"
#include "mc/deps/minecraft_renderer/renderer/RenderMaterial.h"
#include "mc/platform/threading/TlsSlot.h"

#include <Windows.h>
#include <array>
#include <atomic>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace levischematic::render {
bool registerProjectionHighlightPipeline();
void unregisterProjectionHighlightPipeline();
namespace {
std::uintptr_t            materialEntry         = 0;
std::uintptr_t            fallbackMaterialEntry = 0;
std::uintptr_t            itemInsertEntry       = 0;
std::uintptr_t            meshDefaultsEntry     = 0;
std::uintptr_t            fallbackInsertEntry   = 0;
bool                      installed             = false;
std::string               unavailable           = "material hook not installed";
thread_local std::size_t* activeVariations      = nullptr;
thread_local bool         activeDeferred        = false;
thread_local bool         activePrepass         = false;

struct NativeVertexFormat {
    std::uint16_t                 fields;
    std::array<std::uint16_t, 15> offsets;
    std::uint16_t                 stride;
    bool                          allowHalfFloats;
};
struct NativeVertexLayout {
    NativeVertexFormat vertices, attributes, geometry;
    bool               split;
};
static_assert(sizeof(NativeVertexFormat) == 36);
static_assert(sizeof(NativeVertexLayout) == 110);
static_assert(offsetof(NativeVertexLayout, split) == 108);

LL_STATIC_HOOK(
    ProjectionMeshDefaultsHook,
    ll::memory::HookPriority::Normal,
    meshDefaultsEntry,
    void,
    mce::MeshData const&                  data,
    mce::VertexLayout const&              layout,
    std::vector<uchar>&                   output,
    uint&                                 indexSize,
    std::optional<gsl::span<glm::mat4x4>> bones,
    std::optional<gsl::span<glm::mat4x4>> previousBones
) {
    origin(data, layout, output, indexSize, bones, previousBones);
    if (!activeVariations || !activeDeferred) return;
    NativeVertexLayout view;
    std::memcpy(&view, &layout, sizeof(view));
    auto const& format = view.split ? view.attributes : view.vertices;
    if (!(format.fields & (1u << 7))) return;
    auto const count    = data.mPositions->size();
    auto const supplied = data.mPBRTextureIndices->size();
    auto const start    = view.split ? count * view.vertices.stride : 0;
    if (detail::fillMissingTerrainMaterialIndices(output, count, supplied, start, format.stride, format.offsets[7])) {
        static std::atomic<unsigned> samples{0};
        if (samples.fetch_add(1, std::memory_order_relaxed) < 16)
            LeviSchematic::getInstance().getSelf().getLogger().info(
                "Projection mesh defaults: prepass={} vertices={} pbrIndices={} split={} missingPbr=65535",
                activePrepass,
                count,
                supplied,
                view.split
            );
    }
}


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

bool labelRecordView(RegistryCursor const& registry, void* record, std::uint16_t value);

bool labelProjectionStage(RegistryCursor const& registry, void* record, bool prepass) {
    return labelRecordView(registry, record, prepass ? detail::projectionCoverageView : detail::projectionColorView);
}

bool labelRecordView(RegistryCursor const& registry, void* record, std::uint16_t value) {
    auto const mask = readField<detail::HighlightMask>(record, 0);
    if (!detail::hasComponent(mask, 0)) return false;
    constexpr std::size_t viewVectorOffset = 16 + 24 + 91 * 24;
    auto const            values           = readField<std::array<std::uintptr_t, 3>>(registry.item, viewVectorOffset);
    auto const            index            = readField<std::uint16_t>(record, 16);
    if (!values[0] || values[0] > values[1] || values[1] > values[2]
        || (values[1] - values[0]) % sizeof(std::uint16_t) != 0
        || index >= (values[1] - values[0]) / sizeof(std::uint16_t))
        return false;
    std::memcpy(reinterpret_cast<void*>(values[0] + index * sizeof(value)), &value, sizeof(value));
    return true;
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
    auto const requested = static_cast<mce::MaterialRenderType>(activePrepass ? 1 : 2);
    if (activeVariations && activeDeferred && type != requested) {
        type = requested;
        ++*activeVariations;
    }
    return origin(result, type, pointLighting);
}


LL_STATIC_HOOK(
    ProjectionFallbackMaterialHook,
    ll::memory::HookPriority::Normal,
    fallbackMaterialEntry,
    void*,
    void*                   result,
    void const*             context,
    void const*             description,
    mce::MaterialRenderType type
) {
    auto const requested = static_cast<mce::MaterialRenderType>(activePrepass ? 1 : 2);
    if (activeVariations && activeDeferred && type != requested) {
        type = requested;
        ++*activeVariations;
    }
    auto* returned = origin(result, context, description, type);
    if (activeVariations && activeDeferred) {
        static std::atomic<unsigned> samples{0};
        if (samples.fetch_add(1, std::memory_order_relaxed) < 24) {
            auto const       color = readField<std::array<float, 4>>(description, 228);
            std::string_view sourceName;
            if (auto* source = readField<std::byte const*>(description, 264))
                sourceName = reinterpret_cast<HashedString const*>(source + 16)->getString();
            std::string targetName, flags, bindings;
            if (readField<bool>(result, 136)) {
                auto* material = readField<std::byte const*>(result, 0);
                targetName     = *reinterpret_cast<std::string const*>(material + 296);
                auto const& nativeFlags =
                    *reinterpret_cast<std::unordered_map<std::string, std::string> const*>(material + 680);
                for (auto const& [key, value] : nativeFlags) flags += key + "=" + value + ";";
                auto* textures = readField<std::byte const*>(result, 72);
                auto  count    = readField<std::uint16_t>(result, 82);
                for (unsigned i = 0; textures && i < count && i < 32; ++i) {
                    auto* entry     = textures + i * 48;
                    auto* parameter = readField<std::byte const*>(entry, 0);
                    if (!parameter) continue;
                    bindings += *reinterpret_cast<std::string const*>(parameter + 8);
                    bindings += readField<std::uint8_t>(entry, 16) == 0
                                  ? (readField<void const*>(entry, 8) ? ":bound;" : ":null;")
                                  : ":alias;";
                }
            }
            LeviSchematic::getInstance().getSelf().getLogger().info(
                "Projection fallback: prepass={} type={} textures={} moving={} ui={} color={}/{}/{}/{} source={} "
                "target={} flags={} bindings={} id={}",
                activePrepass,
                static_cast<int>(type),
                readField<std::size_t>(description, 136),
                readField<bool>(description, 223),
                readField<bool>(description, 220),
                color[0],
                color[1],
                color[2],
                color[3],
                sourceName,
                targetName,
                flags,
                bindings,
                readField<std::int64_t>(description, 368)
            );
        }
    }
    return returned;
}


LL_STATIC_HOOK(
    ProjectionWorldItemInsertHook,
    ll::memory::HookPriority::Normal,
    itemInsertEntry,
    void,
    void*       self,
    void const* context,
    void const* description
) {
    if (!activeVariations || !activeDeferred || readField<bool>(description, 121)) {
        origin(self, context, description);
        return;
    }
    if (activePrepass && readField<bool>(description, 122)) return;
    alignas(8) std::array<std::byte, 384> item;
    std::memcpy(item.data(), description, item.size());
    item[122] = activePrepass ? std::byte{0} : std::byte{1};
    item[125] = std::byte{0};
    static std::atomic<unsigned> samples{0};
    if (samples.fetch_add(1, std::memory_order_relaxed) < 16) {
        auto const overlay = readField<std::array<float, 4>>(description, 144);
        auto const tint    = readField<std::array<float, 4>>(description, 176);
        LeviSchematic::getInstance().getSelf().getLogger().info(
            "Projection item: prepass={} blending={} alphaTest={} texture={} overlay={}/{}/{}/{} tint={}/{}/{}/{} "
            "flags={} id={}",
            activePrepass,
            readField<bool>(description, 122),
            readField<bool>(description, 123),
            readField<void const*>(description, 8) != nullptr,
            overlay[0],
            overlay[1],
            overlay[2],
            overlay[3],
            tint[0],
            tint[1],
            tint[2],
            tint[3],
            readField<std::uint8_t>(description, 120),
            readField<std::int64_t>(description, 288)
        );
    }
    ++*activeVariations;
    auto const before = cursor(context);
    origin(self, context, item.data());
    if (!activePrepass && before.valid && !readField<bool>(description, 122)) {
        auto const after = cursor(context);
        if (after.valid && before.frame == after.frame && (!before.item || before.item == after.item)
            && after.count >= before.count) {
            for (auto i = before.count; i < after.count; ++i) {
                auto* record = reinterpret_cast<void*>(after.records + i * recordStride);
                auto  mask   = readField<detail::HighlightMask>(record, 0);
                if (!detail::hasComponent(mask, 12) || !detail::hasComponent(mask, 13)
                    || !detail::hasComponent(mask, 25) || !detail::hasComponent(mask, 5))
                    continue;
                detail::restoreWorldItemPass(mask, false, readField<bool>(description, 123));
                std::memcpy(record, mask.data(), sizeof(mask));
            }
        }
    }
}

template <class T>
bool copyActorConstant(std::array<std::byte, 384>& item, std::size_t offset, void const* constant) {
    if (!constant) return false;
    auto const* data = readField<T const*>(constant, 64);
    if (!data) return false;
    std::memcpy(item.data() + offset, data, sizeof(T));
    return true;
}

bool submitLegacyWorldItem(void const* description) {
    auto const* materialInfo = readField<std::byte const*>(description, 264);
    if (!materialInfo) return false;
    auto const source = reinterpret_cast<HashedString const*>(materialInfo + 16)->getString();
    if (!detail::useWorldItemForLegacyMesh(
            source,
            readField<std::size_t>(description, 136),
            readField<bool>(description, 223),
            readField<bool>(description, 220),
            readField<bool>(description, 221),
            readField<std::array<float, 4>>(description, 228)
        ))
        return false;
    auto const* material  = readField<mce::RenderMaterial const*>(materialInfo, 64);
    auto const* constants = readField<mce::GlobalConstantBuffers const*>(description, 280);
    if (!material || !constants || !readField<void const*>(description, 8)) return false;
    auto builder = ServiceLocator<mce::framebuilder::FrameBuilder>::get();
    if (!builder || !builder.get()->enabled()) return false;

    alignas(8) std::array<std::byte, 384> item{};
    auto                                  copy = [&](std::size_t to, std::size_t from, std::size_t size) {
        std::memcpy(item.data() + to, static_cast<std::byte const*>(description) + from, size);
    };
    copy(0, 0, 8);
    copy(8, 8, 16);
    copy(40, 264, 16);
    copy(56, 144, 64);
    item[120] = static_cast<std::byte>(
        static_cast<unsigned>(ItemContextFlags::WorldPass) | static_cast<unsigned>(ItemContextFlags::Item3D)
    );
    item[122]         = material->blendStateDescription->enableBlend ? std::byte{1} : std::byte{0};
    item[123]         = material->mDefines->contains("ALPHA_TEST") ? std::byte{1} : std::byte{0};
    item[124]         = readField<float>(description, 308) != 0 ? std::byte{1} : std::byte{0};
    auto const& actor = constants->entityConstantBuffer.get();
    using V4          = std::array<float, 4>;
    using V2          = std::array<float, 2>;
    if (!copyActorConstant<V4>(item, 128, actor.GLINT_COLOR) || !copyActorConstant<V4>(item, 144, actor.OVERLAY_COLOR)
        || !copyActorConstant<V4>(item, 160, actor.CHANGE_COLOR)
        || !copyActorConstant<V4>(item, 176, actor.MULTIPLICATIVE_TINT_CHANGE_COLOR)
        || !copyActorConstant<V4>(item, 192, actor.TILE_LIGHT_COLOR)
        || !copyActorConstant<V2>(item, 208, actor.TILE_LIGHT_INTENSITY)
        || !copyActorConstant<V4>(item, 216, actor.BLOCK_LIGHT_COLOR)
        || !copyActorConstant<V2>(item, 260, actor.UV_OFFSET) || !copyActorConstant<V2>(item, 268, actor.UV_ROTATION)
        || !copyActorConstant<V2>(item, 276, actor.UV_SCALE))
        return false;
    copy(232, 312, 28);
    copy(284, 360, 2);
    copy(288, 368, 56);
    copy(344, 424, 24);
    copy(368, 448, 16);
    auto const& native = *reinterpret_cast<mce::framebuilder::RenderItemInHandDescription const*>(item.data());
    builder.get()->_insert(std::cref(native));
    static std::array<std::atomic<unsigned>, 2> samples{};
    if (samples[activePrepass ? 0 : 1].fetch_add(1, std::memory_order_relaxed) < 8)
        LeviSchematic::getInstance().getSelf().getLogger().info(
            "Projection legacy item: prepass={} source={} blending={} alphaTest={} id={}",
            activePrepass,
            source,
            item[122] != std::byte{0},
            item[123] != std::byte{0},
            readField<std::int64_t>(item.data(), 288)
        );
    return true;
}

LL_STATIC_HOOK(
    ProjectionLegacyItemBridgeHook,
    ll::memory::HookPriority::Normal,
    fallbackInsertEntry,
    void,
    void*       self,
    void const* context,
    void const* descriptions
) {
    if (activeVariations && activeDeferred && readField<std::uint8_t>(descriptions, 8) == 2) {
        if (auto const* fallback = readField<void const*>(descriptions, 0); fallback && submitLegacyWorldItem(fallback))
            return;
    }
    origin(self, context, descriptions);
}

class MaterialScope {
    std::size_t* previous;
    bool         previousDeferred;
    bool         previousPrepass;

public:
    MaterialScope(std::size_t& count, bool prepass)
    : previous(activeVariations),
      previousDeferred(activeDeferred),
      previousPrepass(activePrepass) {
        activeVariations = &count;
        activePrepass    = prepass;
        auto builder     = ServiceLocator<mce::framebuilder::FrameBuilder>::get();


        activeDeferred = builder && builder.get()->isDeferredEnabled();
    }
    ~MaterialScope() {
        activeVariations = previous;
        activeDeferred   = previousDeferred;
        activePrepass    = previousPrepass;
    }
    MaterialScope(MaterialScope const&)            = delete;
    MaterialScope& operator=(MaterialScope const&) = delete;
};
}

std::uintptr_t resolveProjectionEntry(char const* symbol, char const* signatureText) {
    if (!supportedExecutable()) return 0;
    if (symbol && *symbol)
        if (auto* entry = ll::memory::SymbolView{symbol}.resolve(true)) return reinterpret_cast<std::uintptr_t>(entry);
    auto                      signature = ll::memory::Signature::parse(signatureText);
    auto                      image     = ll::sys_utils::getImageRange();
    ll::memory::SignatureView view{signature};
    auto*                     entry = static_cast<std::byte const*>(view.resolve(image, true));
    if (!entry || entry < image.data() || entry >= image.data() + image.size()
        || view.resolve(image.subspan(static_cast<size_t>(entry - image.data()) + 1), true))
        return 0;
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
        "?makeMaterialFilter@bgfxbridge@framebuilder@mce@@YA?AV?$optional@UMaterialFilter@components@frameobject@"
        "dragon@@@std@@AEBUEntityCreationContext@123@AEBURenderMeshFallbackDescription@23@W4MaterialRenderType@3@@Z",
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 98 04 00 00 48 8D AC 24 ? ? ? ? "
        "0F 29 BD ? ? ? ? 0F 29 B5 ? ? ? ? 48 C7 85 ? ? ? ? ? ? ? ? 44 89 CB"
    );
    bool const actorHooked    = materialEntry && ProjectionMaterialVariationHook::hook() == 0;
    bool const fallbackHooked = fallbackMaterialEntry && ProjectionFallbackMaterialHook::hook() == 0;
    itemInsertEntry           = resolveProjectionEntry("", "55 41 57 41 56 41 55 41 54 56 57 53 B8 58 21 00 00");
    bool const itemHooked     = itemInsertEntry && ProjectionWorldItemInsertHook::hook() == 0;
    meshDefaultsEntry         = resolveProjectionEntry(
        "?generateOldStyleVertexBufferData@MeshGenUtil@mce@@YAXAEBVMeshData@2@AEBUVertexLayout@2@AEAV?$vector@EV?$"
        "allocator@E@std@@@std@@AEAIV?$optional@V?$span@U?$mat@$03$03M$0A@@glm@@$0?0@gsl@@@6@4@Z",
        "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC 28 01 00 00 "
        "44 0F 29 A4 24 10 01 00 00 44 0F 29 9C 24 00 01 00 00 44 0F 29 94 24 F0 00 00 00"
    );
    bool const meshHooked = meshDefaultsEntry && ProjectionMeshDefaultsHook::hook() == 0;
    fallbackInsertEntry   = resolveProjectionEntry(
        "",
        "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC B8 09 00 00 "
        "48 8D AC 24 80 00 00 00 44 0F 29 AD 20 09 00 00 44 0F 29 A5 10 09 00 00"
    );
    bool const bridgeHooked = fallbackInsertEntry && ProjectionLegacyItemBridgeHook::hook() == 0;
    installed               = actorHooked && fallbackHooked && itemHooked && meshHooked && bridgeHooked;
    if (!installed) {
        if (actorHooked) ProjectionMaterialVariationHook::unhook();
        if (fallbackHooked) ProjectionFallbackMaterialHook::unhook();
        if (itemHooked) ProjectionWorldItemInsertHook::unhook();
        if (meshHooked) ProjectionMeshDefaultsHook::unhook();
        if (bridgeHooked) ProjectionLegacyItemBridgeHook::unhook();
    }
    if (installed && !registerProjectionHighlightPipeline()) {
        ProjectionMaterialVariationHook::unhook();
        ProjectionFallbackMaterialHook::unhook();
        ProjectionWorldItemInsertHook::unhook();
        ProjectionMeshDefaultsHook::unhook();
        ProjectionLegacyItemBridgeHook::unhook();
        installed = false;
    }
    unavailable = installed ? "" : "material entry unavailable or hook installation failed";
}

void unregisterBlockEntityHighlight() {
    unregisterProjectionHighlightPipeline();
    if (installed) ProjectionMaterialVariationHook::unhook();
    if (installed) ProjectionFallbackMaterialHook::unhook();
    if (installed) ProjectionWorldItemInsertHook::unhook();
    if (installed) ProjectionMeshDefaultsHook::unhook();
    if (installed) ProjectionLegacyItemBridgeHook::unhook();
    installed             = false;
    materialEntry         = 0;
    fallbackMaterialEntry = 0;
    itemInsertEntry       = 0;
    meshDefaultsEntry     = 0;
    fallbackInsertEntry   = 0;
    unavailable           = "material hook not installed";
}

static BlockEntityHighlightStats captureProjection(std::function<std::size_t()> const& dispatch, bool prepassOnly) {
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
    auto&          builder  = *service.get();
    bool const     deferred = builder.isDeferredEnabled();
    RegistryCursor before;
    builder.custom([&](auto const& context) { before = cursor(&context); });
    if (!before.valid) {
        stats.skipped = "invalid frame registry";
        return stats;
    }


    std::size_t prepassEnd = before.count;
    if (prepassOnly || deferred) {
        MaterialScope scope(stats.forwardVariations, true);
        auto          count = dispatch();
        if (prepassOnly) stats.submitted = count;
        builder.custom([&](auto const& context) {
            auto const current = cursor(&context);
            if (current.valid && current.frame == before.frame && current.count >= before.count)
                prepassEnd = current.count;
        });
    }
    if (!prepassOnly) {
        MaterialScope scope(stats.forwardVariations, false);
        stats.submitted = dispatch();
    }
    builder.custom([&](auto const& context) {
        auto const after = cursor(&context);
        if (!after.valid || after.frame != before.frame || (before.item && after.item != before.item)
            || after.count < before.count) {
            stats.skipped = "frame registry changed during dispatch";
            return;
        }
        std::size_t coverageRecords = 0, colorRecords = 0, unpartitionedRecords = 0;
        for (auto i = before.count; i < after.count; ++i) {
            auto*      record         = reinterpret_cast<void*>(after.records + i * recordStride);
            auto       mask           = readField<detail::HighlightMask>(record, 0);
            bool const stageCandidate = deferred
                                     && (detail::hasComponent(mask, 1) || detail::hasComponent(mask, 3)
                                         || (detail::hasComponent(mask, 5) && detail::hasComponent(mask, 61)))
                                     && detail::hasComponent(mask, 12) && detail::hasComponent(mask, 13)
                                     && detail::hasComponent(mask, 25);
            bool const splitStage     = stageCandidate && labelProjectionStage(after, record, i < prepassEnd);
            if (splitStage) {
                if (i < prepassEnd) ++coverageRecords;
                else ++colorRecords;
            } else if (stageCandidate) ++unpartitionedRecords;
            if (detail::routeBlockEntityHighlight(mask, splitStage)) ++stats.highlighted;
            std::memcpy(record, mask.data(), sizeof(mask));
        }
        static std::array<std::atomic<unsigned>, 2> stageSamples{};
        if (deferred && (coverageRecords || colorRecords || unpartitionedRecords)
            && stageSamples[prepassOnly ? 0 : 1].fetch_add(1, std::memory_order_relaxed) < 8)
            LeviSchematic::getInstance().getSelf().getLogger().info(
                "Projection stages: coverageOnly={} coverage={} color={} unpartitioned={}",
                prepassOnly,
                coverageRecords,
                colorRecords,
                unpartitionedRecords
            );
    });
    return stats;
}

bool isProjectionPrepass() { return activePrepass; }
bool isProjectionDeferred() { return activeDeferred; }

BlockEntityHighlightStats submitBlockEntityHighlight(std::function<std::size_t()> const& dispatch) {
    return captureProjection(dispatch, false);
}

BlockEntityHighlightStats submitBlockEntityPrepass(std::function<std::size_t()> const& dispatch) {
    return captureProjection(dispatch, true);
}

ValidationMarkerRouteStats routeValidationMarkers(std::function<void()> const& dispatch, std::uint16_t view) {
    ValidationMarkerRouteStats stats;
    auto                       service = ServiceLocator<mce::framebuilder::FrameBuilder>::get();
    if (!service || !service.get()->enabled()) return stats;
    auto&          builder = *service.get();
    RegistryCursor before;
    builder.custom([&](auto const& context) { before = cursor(&context); });
    if (!before.valid) return stats;
    dispatch();
    builder.custom([&](auto const& context) {
        auto const after = cursor(&context);
        if (!after.valid || after.frame != before.frame || (before.item && after.item != before.item)
            || after.count < before.count)
            return;
        for (auto i = before.count; i < after.count; ++i) {
            auto* record = reinterpret_cast<void*>(after.records + i * recordStride);
            auto  mask   = readField<detail::HighlightMask>(record, 0);
            if (detail::routeValidationMarker(mask) && labelRecordView(after, record, view)) {
                ++stats.routed;
            } else {
                mask = readField<detail::HighlightMask>(record, 0);
                detail::disableRecordPasses(mask);
                ++stats.unrouted;
            }
            std::memcpy(record, mask.data(), sizeof(mask));
        }
    });
    return stats;
}
}
