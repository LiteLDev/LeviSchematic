#include "PlacementTypes.h"

namespace levischematic::placement {

std::string LoadPlacementError::describe(std::string_view target) const {
    switch (code) {
    case Code::FileNotFound:
        return "Schematic file not found: " + std::string(target);
    case Code::FileReadFailed:
        return detail.empty()
            ? "Failed to read schematic file: " + std::string(target)
            : "Failed to read schematic file: " + detail;
    case Code::NbtParseFailed:
        return detail.empty()
            ? "NBT parse failed for schematic: " + std::string(target)
            : "NBT parse failed for schematic: " + detail;
    case Code::TemplateLoadFailed:
        return detail.empty()
            ? "Structure data is invalid or unsupported: " + std::string(target)
            : "Structure data is invalid or unsupported: " + detail;
    case Code::EmptyStructure:
        return "Schematic is empty: " + std::string(target);
    case Code::RegistryError:
        return detail.empty()
            ? "Block registry is unavailable while loading schematic."
            : "Block registry error while loading schematic: " + detail;
    }

    return "Unknown schematic load error: " + std::string(target);
}

} // namespace levischematic::placement
