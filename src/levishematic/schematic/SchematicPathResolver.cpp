#include "levishematic/schematic/SchematicPathResolver.h"

#include <algorithm>
#include <system_error>
#include <vector>

namespace levishematic::schematic {
namespace {

namespace fs = std::filesystem;

bool pathExists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec) && !ec;
}

fs::path makeAbsoluteIfPossible(const fs::path& path) {
    std::error_code ec;
    auto            absolute = fs::absolute(path, ec);
    return ec ? path : absolute;
}

void appendUnique(std::vector<fs::path>& candidates, fs::path candidate) {
    candidate = candidate.lexically_normal();
    if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) {
        candidates.push_back(std::move(candidate));
    }
}

} // namespace

SchematicPathResolver::SchematicPathResolver(std::filesystem::path schematicDirectory)
: mSchematicDirectory(std::move(schematicDirectory)) {
    if (!mSchematicDirectory.empty()) {
        mSchematicDirectory = mSchematicDirectory.lexically_normal();
    }
}

std::filesystem::path SchematicPathResolver::withMcstructureExtension(const std::filesystem::path& path) const {
    auto result = path;
    if (result.extension().empty()) {
        result += ".mcstructure";
    }
    return result.lexically_normal();
}

std::filesystem::path SchematicPathResolver::makeFilePath(const std::filesystem::path& path) const {
    auto result = path;
    if (!result.is_absolute() && !mSchematicDirectory.empty()) {
        result = mSchematicDirectory / result;
    }

    return withMcstructureExtension(result);
}

std::filesystem::path SchematicPathResolver::resolveExistingPath(const std::filesystem::path& path) const {
    std::vector<fs::path> candidates;

    if (path.is_absolute()) {
        appendUnique(candidates, path);
        appendUnique(candidates, withMcstructureExtension(path));
    } else {
        if (!mSchematicDirectory.empty()) {
            appendUnique(candidates, mSchematicDirectory / path);
            appendUnique(candidates, makeFilePath(path));
        }

        appendUnique(candidates, makeAbsoluteIfPossible(path));
        appendUnique(candidates, makeAbsoluteIfPossible(withMcstructureExtension(path)));
    }

    for (const auto& candidate : candidates) {
        if (pathExists(candidate)) {
            return candidate;
        }
    }

    if (path.is_absolute()) {
        return withMcstructureExtension(path);
    }

    if (!mSchematicDirectory.empty()) {
        return makeFilePath(path);
    }

    return makeAbsoluteIfPossible(withMcstructureExtension(path));
}

} // namespace levishematic::schematic
