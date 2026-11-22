#include "levischematic/schematic/io/SchematicPathResolver.h"

#include <algorithm>
#include <system_error>
#include <vector>

namespace levischematic::schematic {
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

}

SchematicPathResolver::SchematicPathResolver(std::filesystem::path schematicDirectory)
: mSchematicDirectory(std::move(schematicDirectory)) {
    if (!mSchematicDirectory.empty()) {
        mSchematicDirectory = mSchematicDirectory.lexically_normal();
    }
}

std::filesystem::path SchematicPathResolver::withExtension(const std::filesystem::path& path, char const* extension) {
    auto result = path;
    if (result.extension().empty()) result += extension;
    return result.lexically_normal();
}

std::filesystem::path SchematicPathResolver::makeFilePath(const std::filesystem::path& path) const {
    auto result = path;
    if (!result.is_absolute() && !mSchematicDirectory.empty()) {
        result = mSchematicDirectory / result;
    }
    return withExtension(result, ".levischematic");
}

std::filesystem::path SchematicPathResolver::resolveExistingPath(const std::filesystem::path& path) const {
    std::vector<fs::path> candidates;
    auto                  addAll = [&](fs::path const& base, bool absolute) {
        auto add = [&](fs::path value) { appendUnique(candidates, absolute ? makeAbsoluteIfPossible(value) : value); };
        add(base);
        for (auto extension : {".levischematic", ".litematic", ".mcstructure"}) add(withExtension(base, extension));
    };
    if (path.is_absolute()) addAll(path, false);
    else {
        if (!mSchematicDirectory.empty()) addAll(mSchematicDirectory / path, false);
        addAll(path, true);
    }
    for (const auto& candidate : candidates) {
        if (pathExists(candidate)) {
            return candidate;
        }
    }
    if (path.is_absolute()) return withExtension(path, ".levischematic");
    if (!mSchematicDirectory.empty()) return makeFilePath(path);
    return makeAbsoluteIfPossible(withExtension(path, ".levischematic"));
}

}
