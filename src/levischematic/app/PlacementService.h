#pragma once

#include "levischematic/app/Result.h"
#include "levischematic/app/RuntimeContext.h"
#include "levischematic/editor/EditorState.h"
#include "levischematic/schematic/placement/PlacementStore.h"
#include "levischematic/schematic/placement/PlacementTypes.h"

#include <filesystem>
#include <functional>
#include <optional>

namespace levischematic::app {

struct PlacementError {
    enum class Code {
        NoSelection,
        PlacementNotFound,
        LoadFailed,
        InvalidBlockOverride,
        OverrideNotFound,
    };

    Code                                         code = Code::PlacementNotFound;
    std::optional<placement::PlacementId>        placementId;
    std::filesystem::path                        path;
    std::optional<placement::LoadPlacementError> loadError;
    std::string                                  detail;


    [[nodiscard]] std::string describe() const;
};


struct PlacementLoadInfo {
    placement::PlacementId id = 0;
    std::filesystem::path  resolvedPath;
    std::string            name;
};


struct RemovedPlacementInfo {
    placement::PlacementId id = 0;
    std::string            name;
};

using PlacementMutationResult = Result<placement::PlacementInstance const*, PlacementError>;
using PlacementStatus         = StatusResult<PlacementError>;


class PlacementService {
public:
    PlacementService(editor::EditorState& state, RuntimeContext& runtime, placement::PlacementStore& placementStore);


    [[nodiscard]] RuntimeContext const& runtime() const { return mRuntime; }


    [[nodiscard]] uint64_t revision() const noexcept { return mState.placements.revision; }

    verifier::VerifierState const& verification() const { return mState.verifier; }

    editor::ViewState const& viewState() const { return mState.view; }


    [[nodiscard]] std::unique_lock<std::recursive_mutex> lockPlacements() const {
        return std::unique_lock{mState.placements.mutex};
    }


    [[nodiscard]] std::unique_lock<std::recursive_mutex> tryLockPlacements() const {
        return std::unique_lock{mState.placements.mutex, std::try_to_lock};
    }


    PlacementLoadInfo adoptAsset(
        std::shared_ptr<placement::SchematicAsset const> asset,
        std::filesystem::path const&                     path,
        BlockPos                                         origin,
        int                                              dimensionId
    );


    [[nodiscard]] placement::PlacementInstance const* findPlacement(placement::PlacementId id) const;


    [[nodiscard]] Result<placement::PlacementInstance const*, PlacementError>
    requirePlacement(placement::PlacementId id) const;


    [[nodiscard]] Result<placement::PlacementInstance const*, PlacementError> requireSelectedPlacement() const;


    [[nodiscard]] std::optional<placement::PlacementId> selectedPlacementId() const;


    [[nodiscard]] std::vector<std::reference_wrapper<placement::PlacementInstance const>> orderedPlacements() const;


    [[nodiscard]] Result<RemovedPlacementInfo, PlacementError> removePlacement(placement::PlacementId id);


    void clearPlacements();


    [[nodiscard]] PlacementStatus selectPlacement(placement::PlacementId id);


    [[nodiscard]] PlacementMutationResult togglePlacementEnabled(placement::PlacementId id);


    [[nodiscard]] PlacementMutationResult togglePlacementRender(placement::PlacementId id);


    [[nodiscard]] PlacementMutationResult movePlacement(placement::PlacementId id, int dx, int dy, int dz);


    [[nodiscard]] PlacementMutationResult setPlacementOrigin(placement::PlacementId id, BlockPos origin);


    [[nodiscard]] PlacementMutationResult
    rotatePlacement(placement::PlacementId id, placement::PlacementInstance::Rotation delta);


    [[nodiscard]] PlacementMutationResult
    setPlacementMirror(placement::PlacementId id, placement::PlacementInstance::Mirror mirror);


    [[nodiscard]] PlacementMutationResult resetPlacementTransform(placement::PlacementId id);


    [[nodiscard]] PlacementMutationResult
    patchPlacementBlock(placement::PlacementId id, BlockPos worldPos, placement::PatchOp const& op);

private:
    [[nodiscard]] PlacementError
    makeNotFoundError(std::optional<placement::PlacementId> id, std::string detail = {}) const;

    editor::EditorState&       mState;
    RuntimeContext&            mRuntime;
    placement::PlacementStore& mPlacementStore;
};

} 
