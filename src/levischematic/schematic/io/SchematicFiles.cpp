#include "levischematic/schematic/io/SchematicFiles.h"
#include "levischematic/shared/i18n/I18n.h"
#include <algorithm>

namespace levischematic::schematic {
namespace fs = std::filesystem;
namespace {
fs::path fromUtf8(std::string const& value) {
    return fs::path(std::u8string_view{reinterpret_cast<char8_t const*>(value.data()), value.size()});
}
fs::path normal(fs::path const& path) {
    std::error_code ec;
    auto            absolute = fs::absolute(path, ec);
    return (ec ? path : absolute).lexically_normal();
}
std::string lower(std::string text) {
    for (auto& c : text)
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return text;
}
} // namespace

std::string utf8(fs::path const& path) {
    auto value = path.u8string();
    return {reinterpret_cast<char const*>(value.data()), value.size()};
}
bool isWithin(fs::path const& root, fs::path const& path) {
    auto base = normal(root), target = normal(path);
    auto b = base.begin(), t = target.begin();
    for (; b != base.end(); ++b, ++t) {
        if (b->empty() && std::next(b) == base.end()) return true;
        if (t == target.end() || lower(utf8(*b)) != lower(utf8(*t))) return false;
    }
    return true;
}
bool validFileName(std::string const& name) {
    if (name.empty() || name == "." || name == ".." || name.size() > 200) return false;
    if (name.back() == ' ' || name.back() == '.') return false;
    for (unsigned char c : name)
        if (c < 32 || std::string_view("<>:\"/\\|?*").find(static_cast<char>(c)) != std::string_view::npos)
            return false;
    return true;
}
std::string            fileSuffix(fs::path const& path) { return lower(utf8(path.extension())); }
std::vector<FileEntry> listDirectory(fs::path const& directory, std::vector<std::string> const& suffixes) {
    std::vector<FileEntry> dirs, files;
    std::error_code        ec;
    for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        auto const& entry = *it;
        auto        name  = utf8(entry.path().filename());
        if (name.empty() || name.front() == '.') continue;
        std::error_code typeError;
        if (entry.is_directory(typeError)) {
            dirs.push_back({name, entry.path(), true});
            continue;
        }
        if (!entry.is_regular_file(typeError)) continue;
        auto lowered = lower(name);
        if (std::any_of(suffixes.begin(), suffixes.end(), [&](auto const& s) { return lowered.ends_with(s); }))
            files.push_back({name, entry.path(), false});
    }
    auto byName = [](FileEntry const& a, FileEntry const& b) { return lower(a.name) < lower(b.name); };
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);
    dirs.insert(dirs.end(), files.begin(), files.end());
    return dirs;
}
std::string withSuffix(std::string name, fs::path const& original, std::vector<std::string> const& suffixes) {
    auto lowered = lower(name);
    if (std::any_of(suffixes.begin(), suffixes.end(), [&](auto const& s) { return lowered.ends_with(s); })) return name;
    return name + fileSuffix(original);
}
std::string createDirectory(fs::path const& root, fs::path const& parent, std::string const& name) {
    if (!validFileName(name)) return tr("msg.io.invalid_name");
    auto target = parent / fromUtf8(name);
    if (!isWithin(root, target)) return tr("msg.io.path_outside_the_root_directory");
    std::error_code ec;
    if (fs::exists(target, ec)) return tr("msg.io.already_exists");
    if (!fs::create_directories(target, ec) || ec) return tr("msg.io.cannot_create_directory");
    return {};
}
std::string renameFile(fs::path const& root, fs::path const& file, std::string const& name) {
    if (!validFileName(name)) return tr("msg.io.invalid_name");
    auto target = file.parent_path() / fromUtf8(name);
    if (!isWithin(root, file) || !isWithin(root, target)) return tr("msg.io.path_outside_the_root_directory");
    std::error_code ec;
    if (!fs::exists(file, ec)) return tr("msg.io.file_not_found");
    if (fs::exists(target, ec)) return tr("msg.io.target_exists");
    fs::rename(file, target, ec);
    return ec ? tr("msg.io.cannot_rename") + ec.message() : std::string{};
}
std::string copyFile(fs::path const& root, fs::path const& file, std::string const& name) {
    if (!validFileName(name)) return tr("msg.io.invalid_name");
    auto target = file.parent_path() / fromUtf8(name);
    if (!isWithin(root, file) || !isWithin(root, target)) return tr("msg.io.path_outside_the_root_directory");
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) return tr("msg.io.file_not_found");
    if (fs::exists(target, ec)) return tr("msg.io.target_exists");
    fs::copy_file(file, target, fs::copy_options::none, ec);
    return ec ? tr("msg.io.cannot_copy") + ec.message() : std::string{};
}
std::string deleteFile(fs::path const& root, fs::path const& file) {
    if (!isWithin(root, file) || normal(root) == normal(file)) return tr("msg.io.path_outside_the_root_directory");
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) return tr("msg.io.only_files_can_be_deleted");
    fs::remove(file, ec);
    return ec ? tr("msg.io.cannot_delete") + ec.message() : std::string{};
}
} // namespace levischematic::schematic
