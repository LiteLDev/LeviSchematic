#include "levischematic/verifier/service/VerificationTask.h"
#include "levischematic/schematic/placement/model/VolumeIndex.h"
#include "levischematic/schematic/world/FakeWorld.h"
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
char const* verificationStatusKey(VerificationStatus status) {
    switch (status) {
    case VerificationStatus::Unknown:
        return "verifier.status.unknown";
    case VerificationStatus::Matched:
        return "verifier.status.matched";
    case VerificationStatus::MissingBlock:
        return "verifier.status.missing";
    case VerificationStatus::PropertyMismatch:
        return "verifier.status.wrong_state";
    case VerificationStatus::BlockMismatch:
        return "verifier.status.wrong_block";
    case VerificationStatus::ExtraBlock:
        return "verifier.status.extra";
    }
    return "verifier.status.unknown";
}
} // namespace levischematic::verifier
