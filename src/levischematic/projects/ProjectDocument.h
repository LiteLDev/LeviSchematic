#pragma once
#include "levischematic/session/SessionDocument.h"
#include <filesystem>
#include <string>
namespace levischematic::projects {
using Json = session::Json;
inline bool validName(std::string const& name) {
    if (name.empty() || name.size() > 100 || name == "." || name == "..") return false;
    for (unsigned char c : name)
        if (c < 32 || std::string_view("/\\:*?\"<>|").find(c) != std::string_view::npos) return false;
    return name.back() != '.' && name.back() != ' ';
}
inline void validateVersionPath(std::filesystem::path const& path) {
    if (path.empty() || path.is_absolute() || path.has_root_name()) throw std::runtime_error("Invalid version path.");
    for (auto const& part : path)
        if (part == "..") throw std::runtime_error("Version path escapes project.");
}
inline std::filesystem::path
relativeVersionPath(std::filesystem::path const& file, std::filesystem::path const& project) {
    auto relative = file.lexically_normal().lexically_relative(project.lexically_normal());
    validateVersionPath(relative);
    return relative;
}
inline void validate(Json const& doc, std::string const& identity, int dimension) {
    if (session::decodeInteger(doc.at("version")) != 1 || doc.at("identity") != identity
        || session::decodeInteger(doc.at("dimension")) != dimension)
        throw std::runtime_error("Project world/dimension/version differs.");
    if (!validName(doc.at("name").get<std::string>())) throw std::runtime_error("Invalid project name.");
    (void)session::decodePosition(doc.at("origin"));
    if (!doc.at("versions").is_array()) throw std::runtime_error("Invalid project versions.");
    int selected = session::decodeInteger(doc.at("current"));
    int pasted   = session::decodeInteger(doc.at("last_pasted"));
    if (selected < -1 || selected >= static_cast<int>(doc.at("versions").size()) || pasted < -1
        || pasted >= static_cast<int>(doc.at("versions").size()))
        throw std::runtime_error("Invalid project version index.");
    for (auto const& v : doc.at("versions")) {
        auto                  file = v.at("file").get<std::string>();
        std::filesystem::path path(file);
        validateVersionPath(path);
        (void)session::decodePosition(v.at("offset"));
    }
}
} 
