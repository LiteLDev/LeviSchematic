#pragma once
#include "PlacementModel.h"
#include "levischematic/editor/LayerRange.h"
#include "levischematic/selection/ToolGeometry.h"
#include <functional>

namespace levischematic::placement {
Block const* projectedBlock(PlacementInstance const&, Block const*);

struct ExpectedPosition {
    BlockPos     world;
    Block const* block       = nullptr;
    size_t       sourceIndex = SIZE_MAX;
};
std::optional<ExpectedPosition> expectedPosition(PlacementInstance const&, size_t cursor);
std::optional<ExpectedPosition> expectedAt(PlacementInstance const&, BlockPos const&);
struct ProjectionHit {
    PlacementId  owner = 0;
    BlockPos     position;
    Block const* block    = nullptr;
    double       distance = 0;
};
using PlacementList = std::vector<std::reference_wrapper<PlacementInstance const>>;
std::optional<ProjectionHit> projectionAt(PlacementList const&, int dimension, BlockPos const&);
std::optional<ProjectionHit> traceProjection(
    PlacementList const&,
    int dimension,
    editor::LayerRange const&,
    selection::tool::Vector start,
    selection::tool::Vector direction,
    double                  reach
);
bool nearProjection(PlacementList const&, int dimension, BlockPos const&, int margin = 2);
} 
