#include "levischematic/presentation/ui/pages/browser/BrowserPages.h"
#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/presentation/ui/i18n/UiI18n.h"
#include "levischematic/presentation/ui/pages/common/PageCommon.h"
#include "levischematic/presentation/ui/pages/survival/SurvivalPages.h"
#include "levischematic/schematic/io/SchematicFiles.h"
#include "levischematic/schematic/io/SchematicInfo.h"
#include "levischematic/schematic/placement/view/ProjectionRefresh.h"
#include "levischematic/selection/service/AreaSelectionStore.h"
#include "levischematic/shared/i18n/I18n.h"
#include <Windows.h>
#include <map>
#include <shellapi.h>

namespace levischematic::ui {
namespace {
namespace fs = std::filesystem;
struct BrowserState {
    fs::path dir, selected;
};
std::map<PageId, BrowserState> gBrowsers;
schematic::SchematicInfoCache  gInfo;
bool                           gCreatePlacement = true;
int                            gEditType = 0, gExportType = 0, gFileOp = 0;
std::optional<uint64_t>        gLoadTask;
std::optional<fs::path>        gAreaFile;
std::vector<std::string> const SchematicSuffixes{".levischematic", ".litematic", ".mcstructure"};
std::vector<std::string> const SelectionSuffixes{".json"};

fs::path areaRoot() { return LeviSchematic::getInstance().getSelf().getDataDir() / "area_selections"; }
fs::path schematicRoot() {
    auto s = app::accessSession();
    return s ? s->runtime().schematicDirectory() : fs::path{};
}
void openInSystem(fs::path const& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
IconRef fileIcon(schematic::FileEntry const& entry) {
    if (entry.directory) return icons::Directory;
    auto suffix = schematic::fileSuffix(entry.path);
    if (suffix == ".litematic") return icons::Litematic;
    if (suffix == ".mcstructure" || suffix == ".levischematic") return icons::Structure;
    return icons::Json;
}
std::string number(int64_t value) { return value < 0 ? "?" : std::to_string(value); }

void fillInfo(Page& p, fs::path const& file) {
    p.info.clear();
    if (file.empty()) return;
    auto info = gInfo.get(file);
    if (!info) {
        p.info.push_back({tr("ui.browser.reading"), {}});
        return;
    }
    if (!info->ok) {
        p.info.push_back({tr("ui.browser.cannot_read"), info->error, colors::Red});
        return;
    }
    p.info.push_back({tr("ui.browser.name"), info->name.empty() ? utf8Path(file.stem()) : info->name});
    if (auto const* meta = info->litematic ? &*info->litematic : info->metadata ? &*info->metadata : nullptr) {
        auto const& m = *meta;
        if (!m.author.empty()) p.info.push_back({tr("ui.browser.author"), m.author});
        if (auto t = formatTime(m.timeCreated); !t.empty()) p.info.push_back({tr("ui.browser.created"), t});
        if (m.timeModified > m.timeCreated)
            if (auto t = formatTime(m.timeModified); !t.empty()) p.info.push_back({tr("ui.browser.modified"), t});
    }
    p.info.push_back({tr("ui.browser.regions"), std::to_string(info->regions)});
    if (info->blocks >= 0 || info->volume >= 0)
        p.info.push_back({tr("ui.browser.blocks_volume"), number(info->blocks) + "/" + number(info->volume)});
    if (info->size != std::array<int, 3>{})
        p.info.push_back(
            {tr("ui.browser.enclosing_size"),
             std::to_string(info->size[0]) + " x " + std::to_string(info->size[1]) + " x "
                 + std::to_string(info->size[2])}
        );
    if (info->litematic) {
        p.info.push_back({tr("ui.browser.litematic_version"), std::to_string(info->litematic->version)});
        if (info->litematic->dataVersion)
            p.info.push_back({tr("ui.browser.data_version"), std::to_string(info->litematic->dataVersion)});
    } else if (info->format == "mcstructure") {
        p.info.push_back({tr("ui.browser.format"), "mcstructure v" + std::to_string(info->formatVersion)});
    } else if (info->format == "levischematic") {
        p.info.push_back(
            {tr("ui.browser.format"), "levischematic v" + std::to_string(info->metadata ? info->metadata->version : 0)}
        );
        if (info->formatVersion)
            p.info.push_back({tr("ui.browser.structure_format_version"), std::to_string(info->formatVersion)});
    }
}

BrowserState& browse(
    std::shared_ptr<Page> const&                                  p,
    PageId                                                        id,
    fs::path const&                                               root,
    std::vector<std::string> const&                               suffixes,
    std::function<void(Row&, schematic::FileEntry const&)> const& onFile = {}
) {
    auto&           state = gBrowsers[id];
    std::error_code ec;
    if (state.dir.empty() || !schematic::isWithin(root, state.dir) || !fs::is_directory(state.dir, ec))
        state.dir = root;
    if (!state.selected.empty() && !fs::exists(state.selected, ec)) state.selected.clear();
    p->layout     = PageLayout::Browser;
    auto relative = state.dir.lexically_relative(root.parent_path());
    p->path       = utf8Path(relative.empty() ? root.filename() : relative);
    PageBuilder b{p};
    b.button(
         "nav:root",
         {},
         [id, root] {
             gBrowsers[id].dir = root;
             requestOpen(id);
         }
    )
        .kind(RowKind::IconButton)
        .icon(icons::DirectoryRoot)
        .in(Slot::TopBar)
        .tip(tr("ui.browser.go_to_the_root_directory_right_click"))
        .right([root] { openInSystem(root); });
    b.button(
         "nav:up",
         {},
         [id, root] {
             auto& s = gBrowsers[id];
             if (s.dir != root && schematic::isWithin(root, s.dir.parent_path())) s.dir = s.dir.parent_path();
             requestOpen(id);
         }
    )
        .kind(RowKind::IconButton)
        .icon(icons::DirectoryUp)
        .in(Slot::TopBar)
        .tip(tr("ui.browser.parent_directory"));
    b.button(
         "nav:mkdir",
         {},
         [p, id, root] {
             prompt(p, tr("ui.browser.new_folder_name"), {}, [p, id, root](std::string name) {
                 auto error = schematic::createDirectory(root, gBrowsers[id].dir, name);
                 p->message = error.empty() ? tr("ui.browser.created_2") : error;
                 requestOpen(id);
             });
         }
    )
        .kind(RowKind::IconButton)
        .icon(icons::NewFolder)
        .in(Slot::TopBar)
        .tip(tr("ui.browser.create_a_folder"));
    b.button("nav:open", {}, [id] { openInSystem(gBrowsers[id].dir); })
        .kind(RowKind::IconButton)
        .icon(icons::Directory)
        .in(Slot::TopBar)
        .tip(tr("ui.browser.open_the_current_folder_in_the"));
    int n = 0;
    for (auto const& entry : schematic::listDirectory(state.dir, suffixes)) {
        auto key = "entry:" + std::to_string(n++);
        b.button(key, entry.name, [id, entry] {
            auto& s = gBrowsers[id];
            if (entry.directory) {
                s.dir = entry.path;
                s.selected.clear();
            } else s.selected = s.selected == entry.path ? fs::path{} : entry.path;
            requestOpen(id);
        });
        auto& row    = b.last();
        row.kind     = RowKind::TableRow;
        row.icon     = fileIcon(entry);
        row.selected = !entry.directory && entry.path == state.selected;
        if (!entry.directory && onFile) onFile(row, entry);
    }
    if (n == 0) b.info("empty", tr("ui.browser.empty")).color(colors::Gray);
    return state;
}

std::optional<app::LoadMode> loadMode() {
    if (!gCreatePlacement) return app::LoadMode::MemoryOnly;
    return currentModifiers().shift ? app::LoadMode::DisabledPlacement : app::LoadMode::Placement;
}
std::optional<uint64_t> requestLoad(std::shared_ptr<Page> const& p, fs::path const& file, app::LoadMode mode) {
    auto s   = app::accessSession();
    auto pos = playerBlockPos();
    if (!s || !pos) return {};
    auto result = s->loads().request(
        file,
        *pos,
        playerDimension(),
        s->sessionGeneration(),
        app::configStore().snapshot().performance.maxLoadRequests,
        mode
    );
    if (!result) {
        p->message = result.error();
        return {};
    }
    p->message = tr("ui.browser.load_queued") + std::to_string(result.value());
    return result.value();
}
void pollTasks(Page& p) {
    auto s = app::accessSession();
    if (!s) return;
    for (auto* task : {&gLoadTask}) {
        if (!*task) continue;
        auto status = s->loads().status(**task, s->sessionGeneration());
        if (!status) {
            task->reset();
            continue;
        }
        p.message = status->phase + (status->detail.empty() ? "" : ": " + status->detail);
        if (!status->terminal) continue;
        task->reset();
    }
}
void renameSchematicPrompt(std::shared_ptr<Page> const& p, PageId route, fs::path const& file, bool author) {
    auto info   = gInfo.get(file);
    auto suffix = schematic::fileSuffix(file);
    if (suffix != ".litematic" && suffix != ".levischematic") {
        p->message = tr("error.metadata_unsupported");
        return;
    }
    auto const* meta    = info && info->litematic ? &*info->litematic
                        : info && info->metadata  ? &*info->metadata
                                                  : nullptr;
    std::string initial = meta ? (author ? meta->author : meta->name) : "";
    prompt(
        p,
        author ? tr("ui.browser.new_author") : tr("ui.browser.new_schematic_name"),
        initial,
        [p, route, file, author](std::string text) {
            auto error = schematic::editLitematicMetadata(
                file,
                author ? std::nullopt : std::optional<std::string>(text),
                author ? std::optional<std::string>(text) : std::nullopt
            );
            p->message = error.empty() ? tr("ui.browser.updated") : error;
            gInfo.clear();
            requestOpen(route);
        }
    );
}
void renameFilePrompt(
    std::shared_ptr<Page> const&    p,
    PageId                          route,
    fs::path const&                 root,
    fs::path const&                 file,
    std::vector<std::string> const& suffixes
) {
    auto name = utf8Path(file.filename());
    prompt(p, tr("ui.browser.new_file_name"), name, [p, route, root, file, suffixes](std::string text) {
        auto target = schematic::withSuffix(text, file, suffixes);
        auto error  = schematic::renameFile(root, file, target);
        p->message  = error.empty() ? tr("ui.browser.renamed") : error;
        if (error.empty()) gBrowsers[route].selected = file.parent_path() / pathFromUtf8(target);
        requestOpen(route);
    });
}

void loadPage(std::shared_ptr<Page> const& p) {
    p->title     = tr("ui.browser.load_schematics");
    p->sidePanel = true;
    auto root    = schematicRoot();
    if (root.empty()) return;
    auto&       state = browse(p, PageId::Load, root, SchematicSuffixes);
    auto        file  = state.selected;
    PageBuilder b{p};
    b.checkbox(
         "create_placement",
         tr("ui.browser.create_placement"),
         gCreatePlacement,
         [] {
             gCreatePlacement = !gCreatePlacement;
             requestOpen(PageId::Load);
         }
    )
        .in(Slot::Status)
        .tip(tr("ui.browser.create_and_select_a_placement_at_the"));
    bool has = !file.empty();
    b.button(
         "load",
         tr("ui.browser.load_schematic"),
         [p, file] {
             if (auto mode = loadMode()) gLoadTask = requestLoad(p, file, *mode);
         }
    )
        .in(Slot::FooterLeft)
        .enabled(has)
        .tip(tr("ui.browser.load_the_selected_schematic_shift"));
    b.button(
         "material_list",
         tr("ui.browser.material_list"),
         [file] {
             auto s = app::accessSession();
             if (!s) return;
             std::error_code ec;
             auto            normalized = fs::weakly_canonical(file, ec);
             for (auto const& ref : s->placement().managedPlacements()) {
                 std::error_code other;
                 if (!ref.get().filePath.empty() && fs::weakly_canonical(ref.get().filePath, other) == normalized && !ec
                     && !other) {
                     (void)s->placement().selectPlacement(ref.get().id);
                     s.reset();
                     openPlacementMaterials();
                     return;
                 }
             }
             s.reset();
             openFileMaterials(file);
         }
    )
        .in(Slot::FooterLeft)
        .enabled(has)
        .tip(tr("ui.browser.placed_counted_against_the_world_not"));
    b.button(
         "rename_schematic",
         tr("ui.browser.rename_schematic"),
         [p, file] { renameSchematicPrompt(p, PageId::Load, file, false); }
    )
        .in(Slot::FooterLeft)
        .enabled(has && schematic::fileSuffix(file) == ".litematic")
        .tip(tr("ui.browser.change_the_name_stored_in_the"));
    b.button(
         "rename_file",
         tr("ui.browser.rename_file"),
         [p, root, file] { renameFilePrompt(p, PageId::Load, root, file, SchematicSuffixes); }
    )
        .in(Slot::FooterLeft)
        .enabled(has);
    navButton(b, "loaded", tr("ui.browser.loaded_schematics"), PageId::Resources, Slot::FooterLeft, icons::MenuLoaded);
    mainMenuButton(b);
    fillInfo(*p, file);
    p->refresh = [weak = std::weak_ptr<Page>(p), file] {
        if (auto page = weak.lock()) {
            if (!page->dismiss) fillInfo(*page, file);
            pollTasks(*page);
        }
    };
}

void managerPage(std::shared_ptr<Page> const& p) {
    p->title     = tr("ui.browser.schematic_manager");
    p->sidePanel = true;
    auto root    = schematicRoot();
    if (root.empty()) return;
    auto&       state  = browse(p, PageId::SchematicManager, root, SchematicSuffixes);
    auto        file   = state.selected;
    bool        has    = !file.empty();
    auto        suffix = schematic::fileSuffix(file);
    PageBuilder b{p};
    bool        litematic = suffix == ".litematic" || suffix == ".levischematic";
    b.button(
         "edit",
         tr("ui.browser.schematic_edit"),
         [p, file] { renameSchematicPrompt(p, PageId::SchematicManager, file, gEditType == 1); }
    )
        .in(Slot::FooterLeft)
        .enabled(has && litematic)
        .tip(tr("ui.browser.levischematic_run_the_selected"));
    b.input(
         "edit_type",
         {},
         std::to_string(gEditType),
         [](std::string text) {
             int n;
             if (parseInt(text, n)) gEditType = n;
         }
    )
        .kind(RowKind::Choice)
        .in(Slot::FooterLeft)
        .enabled(has && litematic);
    b.last().choices = {tr("ui.browser.rename_schematic"), tr("ui.browser.change_author")};
    b.button(
         "export",
         tr("ui.browser.export_as"),
         [p, file] {
             static char const* const targets[] = {".levischematic", ".mcstructure", ".litematic"};
             auto                     target    = targets[std::clamp(gExportType, 0, 2)];
             if (schematic::fileSuffix(file) == target) {
                 p->message = tr("ui.browser.already_in_this_format");
                 return;
             }
             auto stem = utf8Path(file.filename());
             stem      = stem.substr(0, stem.size() - schematic::fileSuffix(file).size());
             prompt(p, tr("ui.browser.export_file_name"), stem, [p, file, target](std::string name) {
                 if (!schematic::validFileName(name)) {
                     p->message = tr("ui.browser.invalid_name");
                     return;
                 }
                 auto s = app::accessSession();
                 if (!s) return;
                 auto result = s->conversions().request(file, file.parent_path() / pathFromUtf8(name + target), false);
                 p->message  = result ? tr("ui.browser.conversion_started") : result.error();
                 requestOpen(PageId::SchematicManager);
             });
         }
    )
        .in(Slot::FooterLeft)
        .enabled(has)
        .tip(tr("ui.browser.convert_the_selected_file_to_the"));
    b.input(
         "export_type",
         {},
         std::to_string(gExportType),
         [](std::string text) {
             int n;
             if (parseInt(text, n)) gExportType = n;
         }
    )
        .kind(RowKind::Choice)
        .in(Slot::FooterLeft)
        .enabled(has);
    b.last().choices = {"levischematic", "mcstructure", "litematic"};
    b.button(
         "file_op",
         tr("ui.browser.file"),
         [p, root, file] {
             if (gFileOp == 0) renameFilePrompt(p, PageId::SchematicManager, root, file, SchematicSuffixes);
             else if (gFileOp == 1)
                 prompt(
                     p,
                     tr("ui.browser.copy_name"),
                     "Copy of " + utf8Path(file.filename()),
                     [p, root, file](std::string name) {
                         auto error =
                             schematic::copyFile(root, file, schematic::withSuffix(name, file, SchematicSuffixes));
                         p->message = error.empty() ? tr("ui.browser.copied") : error;
                         requestOpen(PageId::SchematicManager);
                     }
                 );
             else
                 confirm(p, tr("ui.browser.delete_file_confirm", utf8Path(file.filename())), [p, root, file] {
                     auto error = schematic::deleteFile(root, file);
                     p->message = error.empty() ? tr("ui.browser.deleted") : error;
                     requestOpen(PageId::SchematicManager);
                 });
         }
    )
        .in(Slot::FooterLeft)
        .enabled(has)
        .tip(tr("ui.browser.run_the_selected_file_operation"));
    b.input(
         "file_op_type",
         {},
         std::to_string(gFileOp),
         [](std::string text) {
             int n;
             if (parseInt(text, n)) gFileOp = n;
         }
    )
        .kind(RowKind::Choice)
        .in(Slot::FooterLeft)
        .enabled(has);
    b.last().choices = {tr("ui.browser.rename"), tr("ui.browser.copy"), tr("ui.browser.delete")};
    mainMenuButton(b);
    fillInfo(*p, file);
    p->refresh = [weak = std::weak_ptr<Page>(p), file] {
        auto page = weak.lock();
        if (!page || page->dismiss) return;
        fillInfo(*page, file);
        auto s = app::accessSession();
        if (!s) return;
        if (auto status = s->conversions().status(); status && !status->terminal)
            page->message = status->phase + (status->detail.empty() ? "" : ": " + status->detail);
    };
}

void selectionsPage(std::shared_ptr<Page> const& p) {
    p->title             = tr("ui.browser.area_selections");
    auto            root = areaRoot();
    std::error_code ec;
    fs::create_directories(root, ec);
    PageBuilder b{p};
    navButton(b, "area_editor", tr("ui.browser.area_editor"), PageId::Selection, Slot::TopBar, icons::MenuAreaEditor);
    b.button(
         "unselect",
         tr("ui.browser.unselect"),
         [] {
             saveCurrentAreaSelection();
             gAreaFile.reset();
             if (auto s = app::accessSession()) s->selection().clearSelection();
             requestOpen(PageId::AreaSelections);
         }
    )
        .in(Slot::TopRight)
        .tip(tr("ui.browser.save_and_deselect_the_current"));
    b.button(
         "from_placement",
         tr("ui.browser.from_placement"),
         [p] {
             auto s = app::accessSession();
             if (!s) return;
             auto        id   = s->placement().selectedPlacementId();
             auto const* item = id ? s->placement().findPlacement(*id) : nullptr;
             if (!item) {
                 p->message = tr("ui.browser.select_a_placement_first");
                 return;
             }
             prompt(p, tr("ui.browser.selection_name"), item->name, [p, id = *id](std::string name) {
                 if (!schematic::validFileName(name)) {
                     p->message = tr("ui.browser.invalid_name");
                     return;
                 }
                 saveCurrentAreaSelection();
                 auto s = app::accessSession();
                 if (!s || !s->selection().fromPlacements(s->placement().orderedPlacements(), id)) {
                     p->message = tr("ui.browser.cannot_create_from_placement");
                     return;
                 }
                 s->selection().setName(name);
                 auto file  = gBrowsers[PageId::AreaSelections].dir / pathFromUtf8(name + ".json");
                 auto error = selection::saveAreaSelection(file, s->selection().state());
                 p->message = error.empty() ? tr("ui.browser.created_2") : error;
                 if (error.empty()) gAreaFile = file;
                 requestOpen(PageId::AreaSelections);
             });
         }
    )
        .in(Slot::TopRight)
        .tip(tr("ui.browser.create_a_selection_from_the_selected"));
    b.button("new_selection", tr("ui.browser.new_selection"), [p] {
         prompt(p, tr("ui.browser.selection_name"), "Unnamed", [p](std::string name) {
             if (!schematic::validFileName(name)) {
                 p->message = tr("ui.browser.invalid_name");
                 return;
             }
             saveCurrentAreaSelection();
             auto s = app::accessSession();
             if (!s) return;
             selection::SelectionState fresh;
             fresh.name = name;
             s->selection().restoreSnapshot(fresh);
             auto file  = gBrowsers[PageId::AreaSelections].dir / pathFromUtf8(name + ".json");
             auto error = selection::saveAreaSelection(file, s->selection().state());
             p->message = error.empty() ? tr("ui.browser.created_2") : error;
             if (error.empty()) gAreaFile = file;
             requestOpen(PageId::AreaSelections);
         });
     }).in(Slot::TopRight);
    browse(p, PageId::AreaSelections, root, SelectionSuffixes, [p, root](Row& row, schematic::FileEntry const& entry) {
        auto file    = entry.path;
        row.selected = gAreaFile && *gAreaFile == file;
        row.tooltip  = selection::describeAreaSelection(file);
        row.submit   = [p, file](std::string) {
            if (gAreaFile && *gAreaFile == file) {
                saveCurrentAreaSelection();
                gAreaFile.reset();
                requestOpen(PageId::AreaSelections);
                return;
            }
            saveCurrentAreaSelection();
            auto s = app::accessSession();
            if (!s) return;
            std::string error;
            auto        loaded = selection::loadAreaSelection(file, s->selection().state(), error);
            if (!loaded) {
                p->message = error;
                return;
            }
            s->selection().restoreSnapshot(*loaded);
            gAreaFile = file;
            requestOpen(PageId::AreaSelections);
        };
        auto        acts = std::make_shared<Page>();
        PageBuilder a{acts};
        auto        key = row.id;
        a.button(key + ":configure", tr("ui.browser.configure"), [p, file] {
            saveCurrentAreaSelection();
            auto s = app::accessSession();
            if (!s) return;
            std::string error;
            auto        loaded = selection::loadAreaSelection(file, s->selection().state(), error);
            if (!loaded) {
                p->message = error;
                return;
            }
            s->selection().restoreSnapshot(*loaded);
            gAreaFile = file;
            requestOpen(PageId::Selection);
        });
        a.button(key + ":copy", tr("ui.browser.copy"), [p, root, file] {
            auto name = utf8Path(file.stem());
            prompt(p, tr("ui.browser.copy_name_2"), name + " copy", [p, root, file](std::string text) {
                auto error = schematic::copyFile(root, file, schematic::withSuffix(text, file, SelectionSuffixes));
                p->message = error.empty() ? tr("ui.browser.copied") : error;
                requestOpen(PageId::AreaSelections);
            });
        });
        a.button(key + ":rename", tr("ui.browser.rename"), [p, root, file] {
            prompt(p, tr("ui.browser.new_name"), utf8Path(file.stem()), [p, root, file](std::string text) {
                auto target = schematic::withSuffix(text, file, SelectionSuffixes);
                auto error  = schematic::renameFile(root, file, target);
                p->message  = error.empty() ? tr("ui.browser.renamed") : error;
                if (error.empty() && gAreaFile && *gAreaFile == file)
                    gAreaFile = file.parent_path() / pathFromUtf8(target);
                requestOpen(PageId::AreaSelections);
            });
        });
        a.button(
             key + ":delete",
             "-",
             [p, root, file] {
                 confirm(p, tr("ui.browser.delete_selection_confirm", utf8Path(file.filename())), [p, root, file] {
                     auto error = schematic::deleteFile(root, file);
                     p->message = error.empty() ? tr("ui.browser.deleted") : error;
                     if (error.empty() && gAreaFile && *gAreaFile == file) gAreaFile.reset();
                     requestOpen(PageId::AreaSelections);
                 });
             }
        )
            .color(colors::Red)
            .tip(tr("ui.browser.delete_this_selection_file"));
        row.actions = std::move(acts->rows);
    });
    if (gAreaFile)
        b.label("current", tr("ui.browser.current_selection") + utf8Path(gAreaFile->stem())).in(Slot::Status);
    mainMenuButton(b);
}
} // namespace

std::optional<fs::path> currentAreaSelectionFile() { return gAreaFile; }
void                    saveCurrentAreaSelection() {
    if (!gAreaFile) return;
    auto s = app::accessSession();
    if (!s) return;
    (void)selection::saveAreaSelection(*gAreaFile, s->selection().state());
}

void populateBrowserPage(std::shared_ptr<Page> const& page, PageId id) {
    switch (id) {
    case PageId::Load:
        loadPage(page);
        break;
    case PageId::SchematicManager:
        managerPage(page);
        break;
    case PageId::AreaSelections:
        selectionsPage(page);
        break;
    default:
        break;
    }
}
} // namespace levischematic::ui
