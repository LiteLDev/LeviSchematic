#pragma once

#include <array>
#include <cstddef>

namespace levischematic::render::diagnostics {






struct BlockEntityTrace {
    std::size_t chestCalls      = 0;
    std::size_t modelCalls      = 0; 
    std::size_t modelGeometries = 0;
    std::size_t groupCalls      = 0;
    std::size_t hiddenGroups    = 0;
    std::size_t meshCalls       = 0;
    std::size_t hiddenMeshes    = 0;
    
    
    std::size_t          indexCountOverride = 0;
    std::size_t          validMeshes        = 0;
    std::size_t          meshVertices       = 0;
    std::size_t          cpuIndices         = 0;
    std::array<float, 3> modelTranslation{};
};





class BlockEntityTraceScope {
public:
    



    explicit BlockEntityTraceScope(BlockEntityTrace* trace) noexcept;
    


    ~BlockEntityTraceScope();
    



    BlockEntityTraceScope(BlockEntityTraceScope const&)            = delete;
    



    BlockEntityTraceScope& operator=(BlockEntityTraceScope const&) = delete;

private:
    BlockEntityTrace* mPrevious;
};




void registerHooks();



void unregisterHooks();

} 
