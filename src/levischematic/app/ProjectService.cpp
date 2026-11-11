#include "ProjectService.h"
#include "AppKernel.h"
#include "levischematic/projects/ProjectDocument.h"
#include "levischematic/projects/SelectionSnapshot.h"
#include "levischematic/schematic/placement/PlacementGeometry.h"
#include "levischematic/session/WorldIdentity.h"
#include "levischematic/util/AtomicFile.h"
#include "levischematic/util/Utf8Path.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include <chrono>
#include <fstream>
#include <set>

namespace levischematic::app {
using projects::Json;
using S = StatusResult<std::string>;
namespace {
Json read(std::filesystem::path const& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Project file unavailable.");
    return Json::parse(in);
}
std::string utf8(std::filesystem::path const& p) {
    auto s = p.generic_u8string();
    return {reinterpret_cast<char const*>(s.data()), s.size()};
}
} 
struct ProjectService::Impl {
    std::filesystem::path base, root, file;
    std::string           identity, detail;
    int                   dimension       = 0;
    int                   checkoutVersion = -1;
    uint64_t              generation = 0, pendingSave = 0, pendingLoad = 0, pendingPaste = 0;
    std::string           versionName, description;
    Json                  document, selection;
    BlockPos              versionOffset{0, 0, 0};
    bool                  active = false;
    explicit Impl(std::filesystem::path directory) : base((directory / "projects").lexically_normal()) {}
    void commit(Json next) {
        projects::validate(next, identity, dimension);
        util::writeAtomic(file, next.dump(2));
        document = std::move(next);
    }
    void remember() {
        util::writeAtomic(
            root / "active.json",
            Json{
                {"identity",  identity                           },
                {"dimension", dimension                          },
                {"folder",    utf8(file.parent_path().filename())}
        }.dump(2)
        );
    }
    std::vector<std::filesystem::path> files() const {
        std::vector<std::filesystem::path> result;
        if (root.empty() || !std::filesystem::exists(root)) return result;
        for (auto const& e : std::filesystem::directory_iterator(root))
            if (e.is_directory() && std::filesystem::is_regular_file(e.path() / "project.json"))
                result.push_back(e.path() / "project.json");
        std::sort(result.begin(), result.end());
        return result;
    }
};
ProjectService::ProjectService(std::filesystem::path directory) : mImpl(std::make_unique<Impl>(std::move(directory))) {}
ProjectService::~ProjectService() = default;
ProjectView ProjectService::view() const {
    auto const& s = *mImpl;
    ProjectView v;
    v.open   = s.active;
    v.saving = s.pendingSave != 0;
    v.detail = s.detail;
    if (!s.active) return v;
    v.name       = s.document.at("name").get<std::string>();
    v.origin     = projects::decodePosition(s.document.at("origin"));
    v.current    = s.document.at("current").get<int>();
    v.lastPasted = s.document.at("last_pasted").get<int>();
    v.work       = s.document.value("work", std::string{});
    int i        = 0;
    for (auto const& version : s.document.at("versions"))
        v.versions.push_back({i++, version.at("name"), version.at("description")});
    return v;
}
std::vector<std::string> ProjectService::list() const {
    auto const&              s = *mImpl;
    std::vector<std::string> result;
    for (auto const& path : s.files()) try {
            auto d = read(path);
            projects::validate(d, s.identity, s.dimension);
            result.push_back(d.at("name"));
        } catch (...) {}
    return result;
}
S ProjectService::create(std::string name, BlockPos origin) {
    auto& s = *mImpl;
    if (s.root.empty()) return S::failure("World identity unavailable.");
    if (s.pendingSave || s.pendingLoad || s.pendingPaste) return S::failure("Project task is still running.");
    if (!projects::validName(name)) return S::failure("Invalid project name.");
    auto existing = list();
    if (std::find(existing.begin(), existing.end(), name) != existing.end()) return S::failure("Project name exists.");
    try {
        auto suffix = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        auto file   = s.root / (session::identityFileKey(name) + "-" + suffix) / "project.json";
        Json d{
            {"version",     1                               },
            {"identity",    s.identity                      },
            {"dimension",   s.dimension                     },
            {"name",        name                            },
            {"origin",      projects::encodePosition(origin)},
            {"current",     -1                              },
            {"last_pasted", -1                              },
            {"work",        ""                              },
            {"versions",    Json::array()                   }
        };
        util::writeAtomic(file, d.dump(2), false);
        s.file     = file;
        s.document = std::move(d);
        s.active   = true;
        s.remember();
        s.detail = "Project created.";
        return ok<std::string>();
    } catch (std::exception const& e) {
        return S::failure(e.what());
    }
}
S ProjectService::open(std::string name) {
    auto& s = *mImpl;
    if (s.pendingSave || s.pendingLoad || s.pendingPaste) return S::failure("Finish the project task first.");
    try {
        for (auto const& file : s.files()) {
            auto d = read(file);
            projects::validate(d, s.identity, s.dimension);
            if (d.at("name") != name) continue;
            s.file     = file;
            s.document = std::move(d);
            s.active   = true;
            s.remember();
            s.detail = "Project opened.";
            return ok<std::string>();
        }
        return S::failure("Project not found in this world/dimension.");
    } catch (std::exception const& e) {
        return S::failure(e.what());
    }
}
S ProjectService::rename(std::string name) {
    auto& s = *mImpl;
    if (!s.active) return S::failure("Open a project first.");
    if (!projects::validName(name)) return S::failure("Invalid project name.");
    auto names = list();
    if (name != s.document.at("name") && std::find(names.begin(), names.end(), name) != names.end())
        return S::failure("Project name exists.");
    try {
        auto d    = s.document;
        d["name"] = name;
        s.commit(std::move(d));
        return ok<std::string>();
    } catch (std::exception const& e) {
        return S::failure(e.what());
    }
}
S ProjectService::setOrigin(BlockPos p) {
    auto& s = *mImpl;
    if (!s.active) return S::failure("Open a project first.");
    if (s.pendingSave || s.pendingLoad || s.pendingPaste)
        return S::failure("Finish project task before changing origin.");
    try {
        auto d           = s.document;
        d["origin"]      = projects::encodePosition(p);
        d["last_pasted"] = -1;
        s.commit(std::move(d));
        return ok<std::string>();
    } catch (std::exception const& e) {
        return S::failure(e.what());
    }
}
S ProjectService::setWork(std::string work) {
    auto& s = *mImpl;
    if (!s.active) return S::failure("Open a project first.");
    if (work.size() > 16384) return S::failure("Work note exceeds 16 KiB.");
    try {
        auto d    = s.document;
        d["work"] = std::move(work);
        s.commit(std::move(d));
        return ok<std::string>();
    } catch (std::exception const& e) {
        return S::failure(e.what());
    }
}
S ProjectService::saveVersion(AppKernel& kernel, std::string name, std::string description) {
    auto& s = *mImpl;
    if (!s.active) return S::failure("Open a project first.");
    if (s.pendingSave || s.pendingLoad || s.pendingPaste) return S::failure("Project task already running.");
    if (name.empty() || name.size() > 256 || description.size() > 16384)
        return S::failure("Invalid version name/description.");
    auto const& selected = kernel.selection().state();
    if (!selected.corner1 || !selected.corner2) return S::failure("Select a complete area first.");
    auto min = selection::Box{*selected.corner1, *selected.corner2}.getMin();
    if (selected.normal)
        for (auto const& r : selected.regions) {
            if (!r.corner1 || !r.corner2) return S::failure("Incomplete region.");
            auto p = selection::Box{*r.corner1, *r.corner2}.getMin();
            min    = {std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z)};
        }
    auto origin = projects::decodePosition(s.document.at("origin"));
    if (selected.normal) min = selected.customOrigin.value_or(min);
    auto offset =
        placement::checkedPosition({int64_t(min.x) - origin.x, int64_t(min.y) - origin.y, int64_t(min.z) - origin.z});
    if (!offset) return S::failure("Version offset overflow.");
    auto version = "v" + std::to_string(s.document.at("versions").size()) + "-"
                 + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    auto result  = kernel.saves().request(
        selected,
        s.file.parent_path() / "versions",
        version,
        s.dimension,
        kernel.sessionGeneration(),
        {true, false}
    );
    if (!result) return S::failure(result.error());
    s.pendingSave   = result.value();
    s.versionName   = std::move(name);
    s.description   = std::move(description);
    s.selection     = projects::encodeSelection(selected);
    s.versionOffset = *offset;
    s.detail        = "Capturing immutable version / 正在采集版本";
    return ok<std::string>();
}
S ProjectService::checkout(AppKernel& kernel, int version) {
    auto& s = *mImpl;
    if (!s.active) return S::failure("Open a project first.");
    if (s.pendingSave || s.pendingLoad || s.pendingPaste) return S::failure("Project task already running.");
    if (!kernel.worldEdit().progress().terminal()) return S::failure("Cancel world task before changing version.");
    if (version < 0 || version >= static_cast<int>(s.document.at("versions").size()))
        return S::failure("Version not found.");
    try {
        auto const& v = s.document.at("versions").at(version);
        auto        a = session::decodePosition(s.document.at("origin")), b = session::decodePosition(v.at("offset"));
        auto        at = worldedit::translate(a, b);
        if (!at) return S::failure("Project origin overflow.");
        auto path = s.file.parent_path() / util::pathFromUtf8(v.at("file").get<std::string>());
        auto load =
            kernel.loads().request(path, {(*at)[0], (*at)[1], (*at)[2]}, s.dimension, kernel.sessionGeneration(), 4);
        if (!load) return S::failure(load.error());
        s.pendingLoad     = load.value();
        s.checkoutVersion = version;
        s.detail          = "Version projection loading; world unchanged.";
        return ok<std::string>();
    } catch (std::exception const& e) {
        return S::failure(e.what());
    }
}
S ProjectService::restoreSelection(AppKernel& kernel) {
    auto& s       = *mImpl;
    auto  current = s.active ? s.document.at("current").get<int>() : -1;
    if (current < 0) return S::failure("Check out a project version first.");
    if (s.pendingLoad || s.pendingSave) return S::failure("Wait for the project task.");
    try {
        auto const& version   = s.document.at("versions").at(current);
        auto        selection = projects::decodeSelection(version.at("selection"));
        auto        old       = projects::decodePosition(version.at("selection_origin"));
        auto        origin    = projects::decodePosition(s.document.at("origin"));
        auto        move      = [&](std::optional<BlockPos>& at) {
            if (!at) return;
            auto p = placement::checkedPosition(
                {int64_t(at->x) + origin.x - old.x,
                 int64_t(at->y) + origin.y - old.y,
                 int64_t(at->z) + origin.z - old.z}
            );
            if (!p) throw std::runtime_error("Restored selection coordinate overflow.");
            at = *p;
        };
        move(selection.corner1);
        move(selection.corner2);
        move(selection.customOrigin);
        for (auto& r : selection.regions) {
            move(r.corner1);
            move(r.corner2);
        }
        kernel.selection().restoreSnapshot(std::move(selection));
        return ok<std::string>();
    } catch (std::exception const& e) {
        return S::failure(e.what());
    }
}
S ProjectService::paste(AppKernel& kernel, ClientInstance& client) {
    auto& s = *mImpl;
    if (!s.active || s.document.at("current").get<int>() < 0) return S::failure("Check out a version first.");
    if (s.pendingLoad || s.pendingSave || s.pendingPaste) return S::failure("Project task already running.");
    auto        selected = kernel.placement().requireSelectedPlacement();
    auto const& v        = s.document.at("versions").at(s.document.at("current").get<int>());
    auto        path     = s.file.parent_path() / util::pathFromUtf8(v.at("file").get<std::string>());
    if (!selected || selected.value()->filePath.lexically_normal() != path.lexically_normal())
        return S::failure("Select this project's current version placement first.");
    kernel.worldEdit().options().operation = worldedit::Operation::Paste;
    auto result                            = kernel.worldEdit().prepare(client, kernel.sessionGeneration());
    if (!result) return S::failure(result.error());
    s.pendingPaste = result.value();
    s.detail       = "Paste preflight queued; review and start in World operations.";
    return ok<std::string>();
}
void ProjectService::cancel(AppKernel& kernel) {
    auto& s = *mImpl;
    if (s.pendingSave && kernel.saves().progress().id == s.pendingSave) kernel.saves().cancel();
    if (s.pendingLoad) kernel.loads().cancel(s.pendingLoad, kernel.sessionGeneration());
    if (s.pendingPaste && kernel.worldEdit().progress().id == s.pendingPaste) kernel.worldEdit().cancel();
    s.pendingSave = s.pendingLoad = s.pendingPaste = 0;
    s.detail                                       = "Project task cancelled; committed versions retained.";
}
void ProjectService::tick(AppKernel& kernel, ClientInstance& client, util::WorkBudget const&) {
    auto& s      = *mImpl;
    auto  player = client.getLocalPlayer();
    if (!player) return;
    try {
        auto identity  = session::worldIdentity(client);
        auto dimension = static_cast<int>(player->getDimensionId());
        if (identity.empty()) return;
        if (identity != s.identity || dimension != s.dimension) {
            cancel(kernel);
            s.identity   = identity;
            s.dimension  = dimension;
            s.generation = kernel.sessionGeneration();
            s.active     = false;
            s.document   = {};
            s.root       = s.base / session::identityFileKey(identity) / std::to_string(dimension);
            auto active  = s.root / "active.json";
            if (std::filesystem::is_regular_file(active)) {
                auto record = read(active);
                auto folder = record.at("folder").get<std::string>();
                if (record.at("identity") != identity || record.at("dimension") != dimension
                    || !projects::validName(folder))
                    throw std::runtime_error("Invalid active project identity.");
                auto file = s.root / util::pathFromUtf8(folder) / "project.json";
                auto d    = read(file);
                projects::validate(d, identity, dimension);
                s.file     = file;
                s.document = std::move(d);
                s.active   = true;
                s.detail   = "Project metadata restored; no world tasks resumed.";
            }
        }
        if (s.pendingSave) {
            auto p = kernel.saves().progress();
            if (p.id != s.pendingSave) {
                s.pendingSave = 0;
                s.detail      = "Save task was replaced; no version committed.";
            } else if (p.terminal) {
                s.pendingSave = 0;
                if (p.phase != "已保存 / Saved") {
                    s.detail = p.phase + ": " + p.detail;
                    return;
                }
                auto d = s.document;
                d["versions"].push_back({
                    {"name", s.versionName},
                    {"description", s.description},
                    {"file", utf8(projects::relativeVersionPath(p.path, s.file.parent_path()))},
                    {"offset", projects::encodePosition(s.versionOffset)},
                    {"selection", s.selection},
                    {"selection_origin", s.document.at("origin")}
                });
                s.commit(std::move(d));
                s.detail = "Version saved; source snapshot is immutable.";
            }
        }
        if (s.pendingLoad) {
            auto status = kernel.loads().status(s.pendingLoad, kernel.sessionGeneration());
            if (!status || status->terminal) {
                s.pendingLoad = 0;
                s.detail      = status ? status->phase + ": " + status->detail : "Load task unavailable.";
                if (status && status->phase == "completed") {
                    auto d       = s.document;
                    d["current"] = s.checkoutVersion;
                    s.commit(std::move(d));
                }
                s.checkoutVersion = -1;
            }
        }
        if (s.pendingPaste) {
            auto p = kernel.worldEdit().progress();
            if (p.id != s.pendingPaste || p.terminal()) {
                bool ownTask   = p.id == s.pendingPaste;
                s.pendingPaste = 0;
                if (ownTask && p.phase == worldedit::Phase::Complete && p.verified == p.total) {
                    auto d           = s.document;
                    d["last_pasted"] = d.at("current");
                    s.commit(std::move(d));
                }
                s.detail = "Paste ended: " + std::string(worldedit::phaseName(p.phase))
                         + "; authoritative=" + std::to_string(p.verified);
            }
        }
    } catch (std::exception const& e) {
        s.detail = e.what();
    }
}
} 
