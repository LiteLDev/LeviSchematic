#pragma once
#include "levischematic/verifier/model/VerifierTypes.h"
namespace levischematic::verifier {
inline std::optional<VerificationStatus> classifyPresence(bool expectedAir, bool actualAir) {
    if (expectedAir) return actualAir ? VerificationStatus::Matched : VerificationStatus::ExtraBlock;
    if (actualAir) return VerificationStatus::MissingBlock;
    return {};
}
} // namespace levischematic::verifier
