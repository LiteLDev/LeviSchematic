#pragma once

#include "levishematic/schematic/placement/PlacementModel.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace levishematic::render {
struct PatchOp;
}

namespace levishematic::placement {

struct PlacementState {
    std::unordered_map<PlacementId, PlacementInstance> placements;
    std::vector<PlacementId>                           order;
    std::optional<PlacementId>                         selectedId;
    uint64_t                                           revision = 0;
    PlacementId                                        nextPlacementId = 1;
    std::filesystem::path                              schematicDirectory;
};

class PlacementStore {
public:
    explicit PlacementStore(PlacementState& state) : mState(state) {}

    PlacementState&       state() { return mState; }
    PlacementState const& state() const { return mState; }

    void setSchematicDirectory(std::filesystem::path directory);
    std::filesystem::path const& schematicDirectory() const { return mState.schematicDirectory; }

    PlacementId createPlacement(
        std::shared_ptr<const SchematicAsset> asset,
        BlockPos                              origin,
        std::string                           name,
        std::filesystem::path                 filePath
    );

    PlacementInstance*       get(PlacementId id);
    PlacementInstance const* get(PlacementId id) const;
    PlacementInstance*       selected();
    PlacementInstance const* selected() const;

    [[nodiscard]] bool remove(PlacementId id);
    void               clear();
    [[nodiscard]] bool select(PlacementId id);
    void               selectLast();

    [[nodiscard]] bool move(PlacementId id, int dx, int dy, int dz);
    [[nodiscard]] bool setOrigin(PlacementId id, BlockPos origin);
    [[nodiscard]] bool rotate(PlacementId id, PlacementInstance::Rotation delta);
    [[nodiscard]] bool setMirror(PlacementId id, PlacementInstance::Mirror mirror);
    [[nodiscard]] bool resetTransform(PlacementId id);
    [[nodiscard]] bool toggleEnabled(PlacementId id);
    [[nodiscard]] bool toggleRender(PlacementId id);
    [[nodiscard]] bool patchBlock(PlacementId id, BlockPos worldPos, render::PatchOp const& op);

    [[nodiscard]] std::filesystem::path    resolveSchematicPath(std::string const& filename) const;
    [[nodiscard]] std::vector<std::string> listAvailableFiles() const;

private:
    void touchState();
    void touchPlacement(PlacementInstance& placement);

    PlacementState& mState;
};

} // namespace levishematic::placement
