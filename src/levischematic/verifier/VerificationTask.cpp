#include "VerificationTask.h"
#include "levischematic/schematic/FakeWorld.h"
#include "levischematic/schematic/placement/VolumeIndex.h"
#include "mc/world/level/block/states/VanillaBlockStateTransformUtils.h"
#include <limits>

namespace levischematic::verifier {
std::string verificationBlockLabel(Block const* block) {
    if (!block) return "Unknown";
    auto spec = buildCompareSpecFromBlock(*block);
    auto text = block->getBlockType().mNameInfo->mFullName->getString();
    for (auto const& state : spec.exactStates) text += " " + state.name + "=" + std::to_string(state.value);
    return text;
}
std::string verificationGroupId(VerificationCell const& cell) {
    return std::to_string(static_cast<int>(cell.status)) + ":" + verificationBlockLabel(cell.expected) + " -> "
         + verificationBlockLabel(cell.observed);
}
char const* verificationStatusLabel(VerificationStatus status) {
    switch (status) {
    case VerificationStatus::Unknown:
        return "未知 / Unknown";
    case VerificationStatus::Matched:
        return "匹配 / Matched";
    case VerificationStatus::MissingBlock:
        return "缺失 / Missing";
    case VerificationStatus::PropertyMismatch:
        return "状态错误 / Wrong state";
    case VerificationStatus::BlockMismatch:
        return "类型错误 / Wrong block";
    case VerificationStatus::ExtraBlock:
        return "多余 / Extra";
    }
    return "Unknown";
}
} 
