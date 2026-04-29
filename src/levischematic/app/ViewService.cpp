#include "ViewService.h"

#include <algorithm>
#include <limits>

namespace levischematic::app {

std::string ViewError::describe() const {
    switch (code) {
    case Code::InvalidLayerRange:
        return detail.empty() ? "Invalid layer range." : detail;
    }

    return "Unknown view error.";
}

ViewMutationStatus ViewService::setMinY(int minY) {
    auto const& current = mViewState.layerRange;
    auto        status  = validateRange(minY, current.maxY);
    if (!status) {
        return status;
    }

    auto& layerRange = mViewState.layerRange;
    bool  changed    = layerRange.minY != minY || !layerRange.enabled;
    layerRange.minY  = minY;
    layerRange.enabled = true;
    if (changed) {
        touch();
    }

    return ok<ViewError>();
}

ViewMutationStatus ViewService::setMaxY(int maxY) {
    auto const& current = mViewState.layerRange;
    auto        status  = validateRange(current.minY, maxY);
    if (!status) {
        return status;
    }

    auto& layerRange = mViewState.layerRange;
    bool  changed    = layerRange.maxY != maxY || !layerRange.enabled;
    layerRange.maxY  = maxY;
    layerRange.enabled = true;
    if (changed) {
        touch();
    }

    return ok<ViewError>();
}

ViewMutationStatus ViewService::setRange(int minY, int maxY) {
    auto status = validateRange(minY, maxY);
    if (!status) {
        return status;
    }

    auto& layerRange = mViewState.layerRange;
    bool  changed    = layerRange.minY != minY || layerRange.maxY != maxY || !layerRange.enabled;
    layerRange.minY  = minY;
    layerRange.maxY  = maxY;
    layerRange.enabled = true;
    if (changed) {
        touch();
    }

    return ok<ViewError>();
}

bool ViewService::adjustMaxYBy(int delta) {
    auto& layerRange = mViewState.layerRange;

    auto const proposedMaxY = static_cast<long long>(layerRange.maxY) + static_cast<long long>(delta);
    auto const targetMaxY   = static_cast<int>(std::clamp(
        proposedMaxY,
        static_cast<long long>(layerRange.minY),
        static_cast<long long>(std::numeric_limits<int>::max())
    ));

    bool const changed = layerRange.maxY != targetMaxY || !layerRange.enabled;
    if (!changed) {
        return false;
    }

    layerRange.maxY   = targetMaxY;
    layerRange.enabled = true;
    touch();
    return true;
}

bool ViewService::enableLayerRange() {
    if (mViewState.layerRange.enabled) {
        return false;
    }

    mViewState.layerRange.enabled = true;
    touch();
    return true;
}

bool ViewService::disableLayerRange() {
    if (!mViewState.layerRange.enabled) {
        return false;
    }

    mViewState.layerRange.enabled = false;
    touch();
    return true;
}

ViewMutationStatus ViewService::validateRange(int minY, int maxY) {
    if (minY > maxY) {
        return ViewMutationStatus::failure({
            .code   = ViewError::Code::InvalidLayerRange,
            .detail = "Invalid layer range: minY cannot be greater than maxY.",
        });
    }

    return ok<ViewError>();
}

void ViewService::touch() {
    ++mViewState.revision;
}

} // namespace levischematic::app
