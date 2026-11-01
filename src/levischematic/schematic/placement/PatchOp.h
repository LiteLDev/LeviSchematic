#pragma once
class Block;
namespace levischematic::placement {

struct PatchOp {
    enum class Kind { Remove, SetBlock, ClearOverride };
    Kind         kind  = Kind::Remove;
    Block const* block = nullptr;

    static PatchOp remove() { return {Kind::Remove}; }

    static PatchOp clearOverride() { return {Kind::ClearOverride}; }

    static PatchOp setBlock(Block const* block) { return {Kind::SetBlock, block}; }
};
} 
