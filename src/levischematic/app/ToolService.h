#pragma once
#include "PlacementService.h"
#include "SelectionService.h"

namespace levischematic::app {

class ToolService {
    editor::EditorState& mState;
    SelectionService&    mSelection;
    PlacementService&    mPlacements;

public:
    ToolService(editor::EditorState& state, SelectionService& selection, PlacementService& placements)
    : mState(state),
      mSelection(selection),
      mPlacements(placements) {}
    editor::ToolState const& state() const { return mState.tool; }
    void                     toggle() {
        mState.tool.enabled = !mState.tool.enabled;
        ++mState.tool.revision;
    }
    void setMode(editor::ToolMode mode) {
        mSelection.setSelectionMode(editor::areaTool(mode));
        mState.tool.activeTool = mode;
        ++mState.tool.revision;
    }
    void cycle(int direction = 1, bool creative = false) {
        auto modes = std::vector{editor::ToolMode::Selection, editor::ToolMode::Placement, editor::ToolMode::Edit};
        if (creative)
            for (int i = static_cast<int>(editor::ToolMode::Paste); i <= static_cast<int>(editor::ToolMode::Move); ++i)
                modes.push_back(static_cast<editor::ToolMode>(i));
        auto it=std::find(modes.begin(),modes.end(),mState.tool.activeTool);
        auto index=it==modes.end()?0:static_cast<int>(it-modes.begin());
        setMode(modes[(index + (direction < 0 ? static_cast<int>(modes.size()) - 1 : 1)) % modes.size()]);
    }
    void sampleBlock(Block const* block) {mState.tool.sampledBlock=block;++mState.tool.revision;}
    void configureEdit(placement::EditMatch match, bool adjacent) {
        mState.tool.editMatch = match; mState.tool.editAdjacent = adjacent; ++mState.tool.revision;
    }
    bool editRay(selection::tool::Vector const&, selection::tool::Vector const&, int dimension, int action);
    void cycleSubmode() {
        if (mSelection.isSelectionModeEnabled()) mSelection.cycleCornerMode();
        else if (mState.tool.activeTool==editor::ToolMode::Edit) {
            auto match=static_cast<int>(mState.tool.editMatch);
            configureEdit(static_cast<placement::EditMatch>((match+1)%4),mState.tool.editAdjacent);
        }
    }
    bool click(BlockPos pos, bool secondary, int dimension);
    bool nudge(selection::tool::Vector const& direction, int amount, int dimension);
    void select(selection::tool::Vector const& start, selection::tool::Vector const& direction, int dimension);
};
} 
