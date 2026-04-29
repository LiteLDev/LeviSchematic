#pragma once

#include "levischematic/verifier/VerifierTypes.h"

#include "mc/deps/core/math/Color.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/TintMethod.h"

#include <array>

namespace levischematic::render {

inline constexpr mce::Color kDefaultProjectionColor(0.75f, 0.85f, 1.0f, 0.85f);
inline constexpr mce::Color kPropertyMismatchProjectionColor(1.0f, 0.87f, 0.25f, 0.9f);
inline constexpr mce::Color kBlockMismatchProjectionColor(0.95f, 0.35f, 0.35f, 0.9f);

class ProjectionColorResolver {
public:
    ProjectionColorResolver();

    [[nodiscard]] mce::Color resolveBaseColor(Block const& block) const;
    [[nodiscard]] mce::Color resolveColor(Block const& block, verifier::VerificationStatus status) const;
    [[nodiscard]] mce::Color lerp(mce::Color from, mce::Color to, float t) const;
    [[nodiscard]] mce::Color tintDefault(TintMethod method) const;

private:
    [[nodiscard]] mce::Color overlayColor(verifier::VerificationStatus status) const;
    [[nodiscard]] float      overlayStrength(verifier::VerificationStatus status) const;
    [[nodiscard]] static bool isBiomeTintMethod(TintMethod method);
    [[nodiscard]] static size_t tintIndex(TintMethod method);

    std::array<mce::Color, static_cast<size_t>(TintMethod::Size)> mTintDefaults;
};

} // namespace levischematic::render
