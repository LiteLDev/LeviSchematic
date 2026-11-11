#pragma once
#include "PlacementModel.h"
#include "levischematic/util/WorkBudget.h"
class CompoundTag;
namespace levischematic::placement {

std::unique_ptr<CompoundTag> serializeContent(SchematicAsset const&);
Block const*                 localBlock(PlacementInstance const&, Block const* worldBlock);
enum class EditMatch { Single, State, Type, Line };
struct EditRequest {
    PlacementId  placement = 0;
    BlockPos     world;
    Block const* replacement = nullptr;
    EditMatch    match       = EditMatch::Single;
    BlockPos     direction{1, 0, 0};
};
} 
