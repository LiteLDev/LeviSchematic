#pragma once

#include "levischematic/app/Result.h"
#include "levischematic/app/RuntimeContext.h"
#include "levischematic/editor/EditorState.h"
#include "levischematic/selection/SelectionExporter.h"
#include "levischematic/selection/GrowSelection.h"
#include "levischematic/util/WorkBudget.h"

#include <string_view>

class Dimension;

namespace levischematic::app {





struct SelectionError {
    enum class Code {
        IncompleteSelection,
        RuntimeUnavailable,
        SaveFailed,
    };

    Code                  code = Code::SaveFailed;
    std::filesystem::path path;
    std::string           detail;

    



    [[nodiscard]] std::string describe(std::string_view target = {}) const;
};





struct SelectionSaveInfo {
    std::string           name;
    BlockPos              size;
    std::filesystem::path path;
};





class SelectionService {
public:
    void        restoreSnapshot(selection::SelectionState);
    bool startGrow(int dimension);
    void cancelGrow();
    void tickGrow(Dimension&,util::WorkBudget const&);
    std::string growStatus() const { return mGrowStatus; }
    void setNormal(bool);
    bool addRegion(std::string);
    bool selectRegion(uint32_t);
    bool removeRegion(uint32_t);
    bool renameRegion(uint32_t, std::string);
    void setOrigin(std::optional<BlockPos>);
    bool resize(BlockPos amount);
    bool fromPlacements(std::vector<std::reference_wrapper<placement::PlacementInstance const>> const&, placement::PlacementId);
    





    SelectionService(
        editor::EditorState&          state,
        RuntimeContext&               runtime,
        selection::SelectionExporter& selectionExporter
    );

    


    [[nodiscard]] selection::SelectionState const& state() const { return mState.selection; }
    


    [[nodiscard]] bool                             isSelectionModeEnabled() const;
    


    [[nodiscard]] selection::SelectionOverlayView  overlay() const;

    



    void setSelectionCorner1(BlockPos pos);
    



    void setSelectionCorner2(BlockPos pos);
    


    void clearSelection();
    



    void setSelectionMode(bool enabled);
    


    void toggleSelectionMode();
    void selectElement(selection::tool::Element element);
    void cycleCornerMode();
    bool toolClick(BlockPos pos, bool secondary);
    bool nudgeSelected(BlockPos delta);

    




    [[nodiscard]] Result<SelectionSaveInfo, SelectionError> saveSelection(
        std::string_view name,
        Dimension&       dimension
    ) const;

private:
    std::optional<selection::GrowSelection> mGrow;
    uint64_t mGrowRevision=0;
    int mGrowDimension=0;
    std::string mGrowStatus;
    


    void touchSelectionState();

    editor::EditorState&          mState;
    RuntimeContext&               mRuntime;
    selection::SelectionExporter& mSelectionExporter;
};

} 
