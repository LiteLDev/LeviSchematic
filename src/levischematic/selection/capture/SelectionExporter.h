#pragma once

#include "levischematic/selection/model/SelectionState.h"
#include "levischematic/shared/result/Result.h"

#include <filesystem>
#include <string>
#include <string_view>

class Dimension;

namespace levischematic::selection {

struct ExportSelectionError {
    enum class Code {
        IncompleteSelection,
        RegistryUnavailable,
        WriteFailed,
    };

    Code                  code = Code::WriteFailed;
    std::filesystem::path path;
    std::string           detail;

    [[nodiscard]] std::string describe(std::string_view target = {}) const;
};

class SelectionExporter {
public:
    [[nodiscard]] app::Result<std::filesystem::path, ExportSelectionError> saveToMcstructure(
        SelectionState const&        state,
        std::filesystem::path const& schematicDirectory,
        std::string_view             name,
        Dimension&                   dimension
    ) const;
};

}
