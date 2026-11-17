#pragma once
#include "levischematic/schematic/placement/model/LayerRange.h"
#include "levischematic/schematic/placement/model/PlacementModel.h"
#include "levischematic/selection/model/ToolGeometry.h"
#include <functional>

namespace levischematic::placement {
Block const* projectedBlock(PlacementInstance const&, Block const*);
struct ExpectedPosition {
    BlockPos     world;
    Block const* block       = nullptr;
    size_t       sourceIndex = SIZE_MAX;
};
std::optional<ExpectedPosition> expectedPosition(PlacementInstance const&, size_t cursor, bool voidAsAir = false);
std::optional<ExpectedPosition> expectedAt(PlacementInstance const&, BlockPos const&, bool voidAsAir = false);
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
