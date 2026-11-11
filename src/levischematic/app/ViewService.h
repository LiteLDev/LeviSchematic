#pragma once

#include "levischematic/app/Result.h"
#include "levischematic/editor/EditorState.h"

#include <string>

namespace levischematic::app {





struct ViewError {
    enum class Code {
        InvalidLayerRange,
    };

    Code        code = Code::InvalidLayerRange;
    std::string detail;

    


    [[nodiscard]] std::string describe() const;
};

using ViewMutationStatus = StatusResult<ViewError>;





class ViewService {
public:
    



    explicit ViewService(editor::ViewState& viewState) : mViewState(viewState) {}

    


    [[nodiscard]] editor::LayerRange const& currentLayerRange() const { return mViewState.layerRange; }

    



    [[nodiscard]] ViewMutationStatus setMinY(int minY);
    



    [[nodiscard]] ViewMutationStatus setMaxY(int maxY);
    




    [[nodiscard]] ViewMutationStatus setRange(int minY, int maxY);
    



    [[nodiscard]] bool               adjustMaxYBy(int delta);
    


    [[nodiscard]] bool               enableLayerRange();
    


    [[nodiscard]] bool               disableLayerRange();
    [[nodiscard]] ViewMutationStatus configure(editor::LayerRange range);
    bool cycleMode(int direction);
    bool step(int delta, double playerCoordinate = 0);
    bool setAt(BlockPos const& position);
    bool follow(BlockPos const& position);

private:
    




    [[nodiscard]] static ViewMutationStatus validateRange(int minY, int maxY);
    


    void                                    touch();

    editor::ViewState& mViewState;
};

} 
