#pragma once

#include "levishematic/render/ProjectionRenderer.h"
#include "levishematic/schematic/placement/PlacementModel.h"

#include <unordered_map>

namespace levishematic::placement {

class PlacementProjectionCache {
public:
    struct View {
        std::vector<render::ProjEntry> const&                               worldEntries;
        std::unordered_map<uint64_t, render::ProjEntry> const&              byPos;
        std::unordered_map<uint64_t, std::vector<render::ProjEntry>> const& bySubChunk;
        std::unordered_map<uint64_t, verifier::ExpectedBlockSnapshot> const& expectedBlocksByPos;
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
        std::unordered_map<uint64_t, verifier::ExpectedBlockSnapshot> expectedBlocksByPos;
    };

    [[nodiscard]] Record const& ensureRecord(PlacementInstance const& placement);
    [[nodiscard]] static Record buildRecord(PlacementInstance const& placement);

    std::unordered_map<PlacementId, Record> mRecords;
};

} // namespace levishematic::placement
