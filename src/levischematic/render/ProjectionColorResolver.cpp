#include "ProjectionColorResolver.h"

#include <algorithm>

namespace levischematic::render {

ProjectionColorResolver::ProjectionColorResolver()
    : mTintDefaults{} {
    mTintDefaults.fill(kDefaultProjectionColor);
    mTintDefaults[tintIndex(TintMethod::DefaultFoliage)]   = mce::Color(0.49f, 0.78f, 0.43f, 0.85f);
    mTintDefaults[tintIndex(TintMethod::BirchFoliage)]     = mce::Color(0.60f, 0.80f, 0.36f, 0.85f);
    mTintDefaults[tintIndex(TintMethod::EvergreenFoliage)] = mce::Color(0.28f, 0.62f, 0.35f, 0.85f);
    mTintDefaults[tintIndex(TintMethod::DryFoliage)]       = mce::Color(0.76f, 0.71f, 0.38f, 0.85f);
    mTintDefaults[tintIndex(TintMethod::Grass)]            = mce::Color(0.52f, 0.84f, 0.34f, 0.85f);
    mTintDefaults[tintIndex(TintMethod::Water)]            = mce::Color(0.37f, 0.66f, 0.95f, 0.85f);
}

mce::Color ProjectionColorResolver::resolveBaseColor(Block const& block) const {
    auto tintMethod = block.getBlockType().mTintMethod;
    return isBiomeTintMethod(tintMethod) ? tintDefault(tintMethod) : kDefaultProjectionColor;
}

mce::Color ProjectionColorResolver::resolveColor(Block const& block, verifier::VerificationStatus status) const {
    auto baseColor = resolveBaseColor(block);
    return lerp(baseColor, overlayColor(status), overlayStrength(status));
}

mce::Color ProjectionColorResolver::lerp(mce::Color from, mce::Color to, float t) const {
    auto factor = std::clamp(t, 0.0f, 1.0f);
    return mce::Color(
        from.r + (to.r - from.r) * factor,
        from.g + (to.g - from.g) * factor,
        from.b + (to.b - from.b) * factor,
        from.a + (to.a - from.a) * factor
    );
}

mce::Color ProjectionColorResolver::tintDefault(TintMethod method) const {
    return isBiomeTintMethod(method) ? mTintDefaults[tintIndex(method)] : kDefaultProjectionColor;
}

mce::Color ProjectionColorResolver::overlayColor(verifier::VerificationStatus status) const {
    switch (status) {
    case verifier::VerificationStatus::PropertyMismatch:
        return kPropertyMismatchProjectionColor;
    case verifier::VerificationStatus::BlockMismatch:
        return kBlockMismatchProjectionColor;
    case verifier::VerificationStatus::Matched:
    case verifier::VerificationStatus::MissingBlock:
    case verifier::VerificationStatus::Unknown:
    default:
        return kDefaultProjectionColor;
    }
}

float ProjectionColorResolver::overlayStrength(verifier::VerificationStatus status) const {
    switch (status) {
    case verifier::VerificationStatus::PropertyMismatch:
        return 0.55f;
    case verifier::VerificationStatus::BlockMismatch:
        return 0.75f;
    case verifier::VerificationStatus::Matched:
    case verifier::VerificationStatus::MissingBlock:
    case verifier::VerificationStatus::Unknown:
    default:
        return 0.0f;
    }
}

bool ProjectionColorResolver::isBiomeTintMethod(TintMethod method) {
    switch (method) {
    case TintMethod::DefaultFoliage:
    case TintMethod::BirchFoliage:
    case TintMethod::EvergreenFoliage:
    case TintMethod::DryFoliage:
    case TintMethod::Grass:
    case TintMethod::Water:
        return true;
    case TintMethod::None:
    case TintMethod::Stem:
    case TintMethod::RedStoneWire:
    case TintMethod::Size:
    default:
        return false;
    }
}

size_t ProjectionColorResolver::tintIndex(TintMethod method) {
    return static_cast<size_t>(method);
}

} // namespace levischematic::render
