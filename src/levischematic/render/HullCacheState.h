#pragma once

#include "levischematic/app/Config.h"
#include <cstdint>

namespace levischematic::render::detail {

struct HullCacheState {
    uint64_t          epoch        = UINT64_MAX;
    uint64_t          viewRevision = UINT64_MAX;
    app::Config::Hull style;

    bool matches(uint64_t currentEpoch, uint64_t currentView, app::Config::Hull const& currentStyle) const {
        return epoch == currentEpoch && viewRevision == currentView && style == currentStyle;
    }
};

struct RenderBatchVersion {
    uint64_t revision = UINT64_MAX;
    bool     complete = false;

    bool matches(uint64_t current) const { return complete && revision == current; }
};

} 
