#pragma once
#include "mc/world/level/BlockPos.h"
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
class CompoundTag;
class BlockSource;
class StructureTemplate;
namespace levischematic::selection {

std::vector<uint64_t> captureEntityIds(BlockSource&, StructureTemplate const&, BlockPos origin, BlockPos size);

class StructureCapture {
public:
    StructureCapture(BlockPos origin, BlockPos size);
    ~StructureCapture();
    void append(CompoundTag const& tile, BlockPos offset, std::span<uint64_t const> entityIds);
    
    std::unique_ptr<CompoundTag> finish();

private:
    BlockPos                             mOrigin, mSize;
    std::unique_ptr<CompoundTag>         mTag;
    std::vector<int>                     mPrimary, mExtra;
    std::unordered_map<std::string, int> mPalette;
    std::unordered_set<uint64_t>         mEntities;
};
} 
