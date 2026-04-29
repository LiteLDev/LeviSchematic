#pragma once

#include "levischematic/render/ProjectionRenderer.h"
#include "levischematic/schematic/placement/PlacementModel.h"

#include <unordered_map>

namespace levischematic::placement {

class PlacementProjectionCache {
public:
    struct View {
        std::vector<render::ProjEntry> const&                               worldEntries;
        std::unordered_map<uint64_t, render::ProjEntry> const&              byPos;
        std::unordered_map<uint64_t, std::vector<render::ProjEntry>> const& bySubChunk;
        std::unordered_map<util::WorldBlockKey, verifier::ExpectedBlockSnapshot, util::WorldBlockKeyHash> const&
            expectedBlocksByKey;
    };

    [[nodiscard]] View view(PlacementInstance const& placement);
    void               clear();
    void               remove(PlacementId id);

private:
    struct Record {
        uint64_t revision = 0;
        std::vector<render::ProjEntry> worldEntries;
        std::unordered_map<uint64_t, render::ProjEntry> byPos;
        std::unordered_map<uint64_t, std::vector<render::ProjEntry>> bySubChunk;
        std::unordered_map<util::WorldBlockKey, verifier::ExpectedBlockSnapshot, util::WorldBlockKeyHash>
            expectedBlocksByKey;
    };

    [[nodiscard]] Record const& ensureRecord(PlacementInstance const& placement);
    [[nodiscard]] static Record buildRecord(PlacementInstance const& placement);

    std::unordered_map<PlacementId, Record> mRecords;
};

} // namespace levischematic::placement
