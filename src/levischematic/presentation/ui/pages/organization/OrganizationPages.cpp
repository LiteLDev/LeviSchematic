#include "levischematic/presentation/ui/pages/organization/OrganizationPages.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/presentation/ui/i18n/UiI18n.h"
#include "levischematic/presentation/ui/pages/common/PageCommon.h"
#include "levischematic/render/actor/ActorProjection.h"
#include "levischematic/schematic/io/SchematicFiles.h"
#include "levischematic/schematic/placement/view/ProjectionRefresh.h"
#include "levischematic/shared/i18n/I18n.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/level/block/Block.h"

namespace levischematic::ui {
namespace {
void confirmDiscard(
    std::shared_ptr<Page> const& p,
    std::string                  message,
    uint64_t                     resource,
    std::function<void()>        action
) {
    beginModal(p, std::move(message));
    PageBuilder b{p};
    b.button("save_first", tr("ui.organization.save_and_continue"), [p, resource, action] {
        auto s = app::accessSession();
        if (!s) return;
        auto result = s->placement().resources().save(resource, {}, true);
        if (!result) {
            p->message = result.error();
            return;
        }
        s.reset();
        dismissModal(p);
        action();
    });
    b.button("confirm", tr("ui.organization.discard_and_continue"), [p, action = std::move(action)] {
         dismissModal(p);
         action();
     }).button("cancel", tr("ui.organization.cancel"), [p] { dismissModal(p); });
}

void loaded(std::shared_ptr<Page> const& p) {
    p->title  = tr("ui.organization.loaded_schematics");
    p->layout = PageLayout::List;
    auto s    = app::accessSession();
    if (!s) return;
    PageBuilder b{p};
    auto        records = s->placement().resources().list();
    for (auto const& r : records) {
        auto        id   = r.id;
        auto        key  = "resource:" + std::to_string(id);
        auto        acts = std::make_shared<Page>();
        PageBuilder a{acts};
        bool        modified = r.content->modified;
        a.button(key + ":place", tr("ui.organization.create_placement"), [p, id] {
             auto s   = app::accessSession();
             auto pos = playerBlockPos();
             if (!s || !pos) return;
             auto result = s->placement().resources().createPlacement(id, *pos, playerDimension());
             if (!result) {
                 p->message = result.error();
                 return;
             }
             auto placementId = static_cast<placement::PlacementId>(result.value());
             if (currentModifiers().shift)
                 if (auto const* item = s->placement().findPlacement(placementId); item && item->enabled)
                     (void)s->placement().togglePlacementEnabled(placementId);
             app::refreshCurrentClientProjectionState(*s);
             p->message = tr("ui.organization.created");
         }).tip(tr("ui.organization.create_a_placement_at_the_player"));
        a.button(key + ":save", tr("ui.organization.save_to_file"), [p, id, source = r.path] {
             prompt(
                 p,
                 tr("ui.organization.save_as_empty_overwrites_the_source"),
                 {},
                 [p, id, source](std::string name) {
                     if (!name.empty())
                         name = schematic::withSuffix(name, source, {".levischematic", ".litematic", ".mcstructure"});
                     auto s = app::accessSession();
                     if (!s) return;
                     auto result = name.empty() ? s->placement().resources().save(id, {}, true)
                                                : s->placement().resources().save(
                                                      id,
                                                      s->runtime().schematicDirectory() / pathFromUtf8(name),
                                                      false
                                                  );
                     p->message  = result ? tr("ui.organization.saved") : result.error();
                     if (result && !name.empty() && schematic::fileSuffix(pathFromUtf8(name)) == ".mcstructure")
                         for (auto const& r : s->placement().resources().list())
                             if (r.id == id && r.content->regions.size() > 1)
                                 p->message += " - " + tr("save.notice.merged", r.content->regions.size());
                     requestOpen(PageId::Resources);
                 }
             );
         }).tip(tr("ui.organization.save_the_schematic_content_including"));
        a.button(key + ":reload", tr("ui.organization.reload"), [p, id] {
             confirmDiscard(p, tr("ui.organization.reload_discards_unsaved_edits"), id, [p, id] {
                 auto s = app::accessSession();
                 if (!s) return;
                 auto result = s->placement().resources().reload(id, true);
                 p->message  = result ? tr("ui.organization.reloaded") : result.error();
                 if (result) app::refreshCurrentClientProjectionState(*s);
                 requestOpen(PageId::Resources);
             });
         }).tip(tr("ui.organization.re_read_the_file_from_disk"));
        a.button(key + ":unload", tr("ui.organization.unload"), [p, id] {
             auto s = app::accessSession();
             if (!s) return;
             auto result = s->placement().resources().unload(id, true, true);
             p->message  = result ? tr("ui.organization.unloaded") : result.error();
             if (result) app::refreshCurrentClientProjectionState(*s);
             s.reset();
             requestOpen(PageId::Resources);
         }).tip(tr("ui.organization.unload_this_schematic_and_all_of_its"));
        b.button(key, r.content->defaultName + (modified ? " !" : ""), [] {});
        auto& row     = b.last();
        row.kind      = RowKind::TableRow;
        row.icon      = r.path.empty() ? icons::Memory : icons::Litematic;
        row.textColor = modified ? colors::Gold : colors::Default;
        row.tooltip =
            tr("ui.organization.file") + utf8Path(r.path) + "\n" + tr("ui.organization.regions")
            + std::to_string(r.content->regions.empty() ? 1 : r.content->regions.size()) + "\n"
            + tr("ui.organization.blocks") + std::to_string(r.content->totalNonAirBlocks())
            + (modified ? "\n" + tr("ui.organization.unsaved_edits") : "")
            + (r.content->sourceConflict ? "\n" + tr("ui.organization.source_changed_save_under_a_new_name") : "");
        row.actions = std::move(acts->rows);
    }
    if (records.empty()) b.info("empty", tr("ui.organization.no_schematics_loaded")).color(colors::Gray);
    navButton(b, "load", tr("ui.organization.load_schematics"), PageId::Load, Slot::FooterLeft, icons::MenuLoad);
    navButton(
        b,
        "placements",
        tr("ui.organization.schematic_placements"),
        PageId::Projection,
        Slot::FooterLeft,
        icons::MenuPlacements
    );
    mainMenuButton(b);
}

void edit(std::shared_ptr<Page> const& p) {
    auto s = app::accessSession();
    if (!s) return;
    p->title      = tr("ui.organization.edit_schematic");
    p->layout     = PageLayout::List;
    p->searchable = false;
    PageBuilder b{p};
    b.info("help", tr("ui.organization.configured_tool_middle_samples_left")).color(colors::Gray);
    b.button("mode", tr("ui.organization.enable_edit_mode"), [] {
         if (auto s = app::accessSession()) s->tools().setMode(editor::ToolMode::Edit);
     }).in(Slot::TopBar);
    b.input("replacement", tr("ui.organization.replacement_block_id"), "", [p](std::string name) {
        auto block = Block::tryGetFromRegistry(HashedString{name});
        if (!block) {
            p->message = tr("ui.organization.unknown_block");
            return;
        }
        if (auto s = app::accessSession()) s->tools().sampleBlock(&*block);
        p->message = tr("ui.organization.replacement_selected");
    });
    b.input(
         "match",
         tr("ui.organization.edit_match"),
         std::to_string(static_cast<int>(s->tools().state().editMatch)),
         [](std::string text) {
             int value;
             if (!parseInt(text, value) || value < 0 || value > 3) return;
             if (auto s = app::accessSession())
                 s->tools().configureEdit(static_cast<placement::EditMatch>(value), s->tools().state().editAdjacent);
         }
    ).kind(RowKind::Choice);
    b.last().choices = {
        tr("ui.organization.single"),
        tr("ui.organization.same_state"),
        tr("ui.organization.same_type"),
        tr("ui.organization.contiguous_line")
    };
    b.toggle("adjacent", tr("ui.organization.add_adjacent"), s->tools().state().editAdjacent, [] {
        if (auto s = app::accessSession())
            s->tools().configureEdit(s->tools().state().editMatch, !s->tools().state().editAdjacent);
    });
    auto progress = s->placement().resources().progress();
    b.info(
        "progress",
        progress.detail + " " + std::to_string(progress.scanned) + " / " + std::to_string(progress.total)
            + " changed=" + std::to_string(progress.changed)
    );
    auto stats = s->actors().stats();
    b.info(
         "actors",
         "Actors: " + std::to_string(stats.loaded) + " submitted=" + std::to_string(stats.submitted)
             + " highlighted=" + std::to_string(stats.highlighted) + " " + stats.detail
    )
        .color(colors::Gray);
    b.button("cancel", tr("ui.organization.cancel_edit"), [] {
         if (auto s = app::accessSession()) s->placement().resources().cancel();
     }).in(Slot::FooterLeft);
    b.button("refresh", tr("ui.organization.refresh"), [] { requestOpen(PageId::Edit); }).in(Slot::FooterLeft);
    navButton(
        b,
        "resources",
        tr("ui.organization.loaded_schematics_2"),
        PageId::Resources,
        Slot::FooterLeft,
        icons::MenuLoaded
    );
    mainMenuButton(b);
}
} // namespace

void populateOrganizationPage(std::shared_ptr<Page> const& p, PageId id) {
    if (id == PageId::Resources) loaded(p);
    else edit(p);
}
} // namespace levischematic::ui
