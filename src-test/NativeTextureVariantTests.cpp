

#include "levischematic/render/NativeTextureVariant.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <variant>

namespace {


struct alignas(8) NativeTextureStorage {
    std::array<std::byte, 0x68> bytes{};
};
using NativeVariant =
    std::variant<std::monostate, NativeTextureStorage, levischematic::render::detail::ResolvedTextureClientPointer>;
static_assert(sizeof(NativeVariant) == 0x70);
static_assert(sizeof(mce::Texture) == 0x20, "Re-audit the workaround if the SDK Texture layout changes");
static_assert(
    sizeof(std::variant<std::monostate, mce::Texture, levischematic::render::detail::ResolvedTextureClientPointer>)
    == 0x28
);
} 

int main() {
    using namespace levischematic::render::detail;
    assert(inspectNativeTextureVariant(nullptr).resolved == nullptr);

    NativeVariant value;
    assert(inspectNativeTextureVariant(&value).tag == 0);
    assert(inspectNativeTextureVariant(&value).resolved == nullptr);

    auto& legacy = value.emplace<1>();


    legacy.bytes[0x20] = std::byte{2};
    assert(inspectNativeTextureVariant(&value).tag == 1);
    assert(inspectNativeTextureVariant(&value).resolved == nullptr);

    value.emplace<2>();
    auto const resolved = inspectNativeTextureVariant(&value);
    assert(resolved.tag == 2);
    assert(resolved.resolved == &std::get<2>(value));
    assert(resolved.resolved->mResourcePointerBlock == nullptr);


    alignas(8) std::array<unsigned char, 0x70> unknown{};
    unknown[0x68] = 0xff;
    assert(inspectNativeTextureVariant(unknown.data()).resolved == nullptr);
    std::puts("NativeTextureVariant ABI regression checks passed");
}
