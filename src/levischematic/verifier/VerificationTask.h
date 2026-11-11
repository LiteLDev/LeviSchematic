#pragma once
#include "VerifierTypes.h"
#include "levischematic/schematic/placement/ProjectionScan.h"
#include "levischematic/schematic/placement/ProjectionQuery.h"
#include "levischematic/editor/LayerRange.h"
#include "levischematic/schematic/placement/PlacementModel.h"
#include <array>
#include <unordered_set>

namespace levischematic::verifier {
struct VerificationGroup {
    std::string        id, expected, actual;
    VerificationStatus status  = VerificationStatus::Unknown;
    size_t             count   = 0;
    bool               ignored = false;
};
struct VerificationLocation {
    BlockPos           position;
    VerificationStatus status;
    std::string        group;
};
struct VerificationReport {
    uint32_t                          placementId = 0;
    bool                              running = false, building = false, layerOnly = false, stale = false;
    size_t                            scanned = 0, total = 0, ignored = 0;
    std::array<size_t, 6>             counts{};
    std::vector<VerificationGroup>    groups;
    std::vector<VerificationLocation> nearby;
};

struct VerificationTask {
    placement::ProjectionScan scan;
    uint32_t                                                               placementId       = 0;
    uint64_t                                                               placementRevision = 0, viewRevision = 0;
    uint64_t                                                               contentRevision = 0;
    BlockPos                                                               origin;
    int                                                                    dimension      = 0;
    size_t                                                                 overrideCursor = 0;
    placement::SchematicAsset const*                                       asset          = nullptr;
    placement::PlacementInstance::Rotation                                 rotation{};
    placement::PlacementInstance::Mirror                                   mirror{};
    bool                                                                   enabled         = true;
    bool                                                                   overrideStarted = false;
    std::unordered_map<uint64_t, placement::OverrideEntry>::const_iterator nextOverride;
    bool                                           running = true, building = true, layerOnly = false, stale = false;
    size_t                                         cursor = 0, pollCursor = 0;
    std::unordered_map<uint64_t, VerificationCell> cells;
    std::vector<uint64_t>                          keys;
    std::unordered_set<std::string>                ignored;
    std::array<bool, 6>                            highlighted{false, false, true, true, true, true};
};

using placement::ExpectedPosition;
using placement::expectedPosition;
std::string                     verificationGroupId(VerificationCell const&);
std::string                     verificationBlockLabel(Block const*);
char const*                     verificationStatusLabel(VerificationStatus);
} 
