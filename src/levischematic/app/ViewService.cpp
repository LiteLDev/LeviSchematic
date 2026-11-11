#include "ViewService.h"

#include <algorithm>
#include <limits>

namespace levischematic::app {

ViewMutationStatus ViewService::configure(editor::LayerRange range) {
    auto status = validateRange(range.minY, range.maxY);
    if (!status) return status;
    if (static_cast<unsigned>(range.axis) > 2 || static_cast<unsigned>(range.mode) > 4)
        return ViewMutationStatus::failure({ViewError::Code::InvalidLayerRange, "Invalid layer axis or mode."});
    range.enabled = range.mode != editor::LayerMode::All;
    if (range != mViewState.layerRange) {
        mViewState.layerRange = range;
        touch();
    }
    return ok<ViewError>();
}
bool ViewService::cycleMode(int direction) {
    auto range = mViewState.layerRange;
    range.mode = static_cast<editor::LayerMode>((static_cast<int>(range.mode) + (direction < 0 ? 4 : 1)) % 5);
    (void)configure(range);
    return true;
}
bool ViewService::step(int delta, double playerCoordinate) {
    if (!editor::stepLayer(mViewState.layerRange, delta, playerCoordinate)) return false;
    touch();
    return true;
}
bool ViewService::setAt(BlockPos const& position) {
    auto& range      = mViewState.layerRange;
    int   coordinate = range.axis == editor::LayerAxis::X ? position.x
                     : range.axis == editor::LayerAxis::Y ? position.y
                                                          : position.z;
    if (range.mode == editor::LayerMode::All || range.mode == editor::LayerMode::Range
        || coordinate == range.boundary())
        return false;
    range.boundary() = coordinate;
    touch();
    return true;
}
bool ViewService::follow(BlockPos const& position) { return mViewState.layerRange.followPlayer && setAt(position); }

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

    auto& layerRange   = mViewState.layerRange;
    bool  changed      = layerRange.minY != minY || !layerRange.enabled || layerRange.axis != editor::LayerAxis::Y
                      || layerRange.mode != editor::LayerMode::Range;
    layerRange.minY    = minY;
    layerRange.enabled = true;
    layerRange.axis    = editor::LayerAxis::Y;
    layerRange.mode    = editor::LayerMode::Range;
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

    auto& layerRange   = mViewState.layerRange;
    bool  changed      = layerRange.maxY != maxY || !layerRange.enabled || layerRange.axis != editor::LayerAxis::Y
                      || layerRange.mode != editor::LayerMode::Range;
    layerRange.maxY    = maxY;
    layerRange.enabled = true;
    layerRange.axis    = editor::LayerAxis::Y;
    layerRange.mode    = editor::LayerMode::Range;
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

    auto& layerRange   = mViewState.layerRange;
    bool  changed      = layerRange.minY != minY || layerRange.maxY != maxY || !layerRange.enabled
                      || layerRange.axis != editor::LayerAxis::Y || layerRange.mode != editor::LayerMode::Range;
    layerRange.minY    = minY;
    layerRange.maxY    = maxY;
    layerRange.enabled = true;
    layerRange.axis    = editor::LayerAxis::Y;
    layerRange.mode    = editor::LayerMode::Range;
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

    layerRange.maxY    = targetMaxY;
    layerRange.enabled = true;
    touch();
    return true;
}

bool ViewService::enableLayerRange() {
    if (mViewState.layerRange.enabled) {
        return false;
    }

    mViewState.layerRange.enabled = true;
    mViewState.layerRange.mode    = editor::LayerMode::Range;
    mViewState.layerRange.axis    = editor::LayerAxis::Y;
    touch();
    return true;
}

bool ViewService::disableLayerRange() {
    if (!mViewState.layerRange.enabled) {
        return false;
    }

    mViewState.layerRange.enabled = false;
    mViewState.layerRange.mode    = editor::LayerMode::All;
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

void ViewService::touch() { ++mViewState.revision; }

} 
