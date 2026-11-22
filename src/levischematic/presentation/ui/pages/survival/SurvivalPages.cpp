#include "levischematic/presentation/ui/pages/survival/SurvivalPages.h"
#include "levischematic/app/bootstrap/LeviSchematic.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/presentation/ui/pages/common/PageCommon.h"
#include "levischematic/shared/i18n/I18n.h"
#include <charconv>
#include <cstdio>
#include <sstream>
#include <tuple>

namespace levischematic::ui {
namespace {
struct MaterialDraft {
    bool hideAvailable = false, layerOnly = false, descending = true;
    int  sort = 1, multiplier = 1, exportType = 1;
};
MaterialDraft gDraft;
std::string   stacks(uint64_t count) {
    char box[32];
    std::snprintf(box, sizeof(box), "%.2f", static_cast<double>(count) / (27 * 64));
    return std::to_string(count) + " = " + std::to_string(count / 64) + " × 64 + " + std::to_string(count % 64) + " = "
         + box + tr("ui.survival.shulker_boxes");
}
void saveSurvival(std::shared_ptr<Page> const& p, bool app::Config::Survival::* member) {
    auto c             = app::configStore().snapshot();
    c.survival.*member = !(c.survival.*member);
    auto error         = app::configStore().submit(c);
    if (error.empty()) error = app::configStore().save();
    if (!error.empty()) p->message = error;
}
void materials(std::shared_ptr<Page> const& p) {
    p->rows.clear();
    p->layout    = PageLayout::Table;
    p->fullTitle = true;
    auto session = app::accessSession();
    if (!session) {
        p->title   = tr("ui.survival.material_list");
        p->message = tr("ui.survival.no_world_session");
        return;
    }
    auto&       service = session->survival().materials();
    auto        report  = service.report();
    std::string name;
    if (report.source == app::MaterialSource::Placement)
        if (auto const* item = report.placementId ? session->placement().findPlacement(report.placementId) : nullptr)
            name = item->name;
    if (report.file) name = report.fileName;
    p->title              = report.area   ? tr("ui.survival.area_material_list", report.areaName)
                          : !name.empty() ? tr("ui.survival.schematic_material_list", name)
                                          : std::string(tr("ui.survival.material_list"));
    bool        placement = report.source == app::MaterialSource::Placement;
    PageBuilder b{p};
    b.button(
         "refresh",
         tr("ui.survival.refresh_list"),
         [] {
             auto s = app::accessSession();
             if (!s) return;
             auto& materials = s->survival().materials();
             if (materials.source() == app::MaterialSource::Placement) materials.request(gDraft.layerOnly);
             else materials.refreshInventory();
             requestOpen(PageId::Materials);
         }
    )
        .in(Slot::TopBar)
        .tip(tr("ui.survival.re_read_the_inventory_placements_are"));
    if (placement) {
        b.input(
             "list_type",
             tr("ui.survival.list_type"),
             gDraft.layerOnly ? "1" : "0",
             [](std::string) {
                 gDraft.layerOnly = !gDraft.layerOnly;
                 if (auto s = app::accessSession()) s->survival().materials().request(gDraft.layerOnly);
                 requestOpen(PageId::Materials);
             }
        )
            .kind(RowKind::Choice)
            .in(Slot::TopBar)
            .tip(tr("ui.survival.all_the_whole_placement_render"));
        b.last().choices = {tr("ui.survival.all"), tr("ui.survival.render_layers")};
    }
    b.toggle(
         "hide_available",
         tr("ui.survival.hide_available"),
         gDraft.hideAvailable,
         [] {
             gDraft.hideAvailable = !gDraft.hideAvailable;
             requestOpen(PageId::Materials);
         }
    )
        .in(Slot::TopBar)
        .tip(tr("ui.survival.hide_items_the_inventory_already"));
    b.toggle(
         "hud",
         tr("ui.survival.info_hud"),
         app::configStore().snapshot().survival.materialHud,
         [p] {
             saveSurvival(p, &app::Config::Survival::materialHud);
             requestOpen(PageId::Materials);
         }
    )
        .in(Slot::TopBar)
        .tip(tr("ui.survival.show_the_most_missing_materials_on"));
    b.input(
         "multiplier",
         {},
         std::to_string(gDraft.multiplier),
         [](std::string text) {
             int n;
             gDraft.multiplier = parseInt(text, n) && n > 0 ? n : 1;
             requestOpen(PageId::Materials);
         }
    )
        .in(Slot::TopRight)
        .tip(tr("ui.survival.count_multiplier_e_g_for_several"));
    b.last().width = 40;
    b.label("multiplier_label", tr("ui.survival.multiplier")).in(Slot::TopRight);
    b.button("info", {}, [] {})
        .kind(RowKind::IconButton)
        .icon(icons::Info)
        .at(-23, 6, 11, 11)
        .tip(tr("ui.survival.total_required_by_the_placement"));
    p->header =
        {tr("ui.survival.item_2"), tr("ui.survival.total_2"), tr("ui.survival.missing_2"), tr("ui.survival.available")};
    p->sortColumn     = gDraft.sort;
    p->sortDescending = gDraft.descending;
    p->sort           = [](int column) {
        if (gDraft.sort == column) gDraft.descending = !gDraft.descending;
        else {
            gDraft.sort       = column;
            gDraft.descending = column != 0;
        }
        requestOpen(PageId::Materials);
    };
    uint64_t m = static_cast<uint64_t>(gDraft.multiplier);
    if (report.building)
        b.info(
             "progress",
             tr("ui.survival.counting") + std::to_string(report.scanned) + " / " + std::to_string(report.total)
        )
            .color(colors::Gold);
    if (report.file && !report.building)
        b.info("file_note", tr("ui.survival.not_loaded_every_block_counts_as")).color(colors::Gray);
    if (report.unresolved)
        b.info("unresolved", tr("ui.survival.unresolved") + std::to_string(report.unresolved)).color(colors::Gray);
    size_t warning = 0;
    for (auto const& text : report.warnings)
        if (warning < 3) b.info("warning" + std::to_string(warning++), text).color(colors::Gray);
    auto rows = report.rows;
    std::erase_if(rows, [&](auto const& r) {
        return service.ignored(r.item) || (gDraft.hideAvailable && r.counts.available >= r.counts.remaining() * m);
    });
    std::stable_sort(rows.begin(), rows.end(), [&](auto const& a, auto const& c) {
        auto key = [&](auto const& r) -> uint64_t {
            return gDraft.sort == 1 ? r.counts.total : gDraft.sort == 2 ? r.counts.remaining() : r.counts.available;
        };
        if (gDraft.sort == 0) return gDraft.descending ? c.label < a.label : a.label < c.label;
        return gDraft.descending ? key(a) > key(c) : key(a) < key(c);
    });
    uint64_t total = 0, done = 0, missing = 0, mismatch = 0;
    for (auto const& r : report.rows) {
        total    += r.counts.total;
        missing  += r.counts.missing;
        mismatch += r.counts.wrongBlock + r.counts.wrongState;
    }
    done         = total - std::min(total, missing + mismatch);
    size_t index = 0;
    for (auto const& row : rows) {
        auto const& c    = row.counts;
        uint64_t    need = c.remaining() * m;
        auto        key  = "material" + std::to_string(index++);
        auto        acts = std::make_shared<Page>();
        PageBuilder a{acts};
        a.button(key + ":ignore", tr("ui.survival.ignore"), [item = row.item] {
             if (auto s = app::accessSession()) s->survival().materials().ignore(item);
             requestOpen(PageId::Materials);
         }).tip(tr("ui.survival.hide_this_item_from_the_list_and_hud"));
        b.button(key, row.label, [] {});
        auto& r        = b.last();
        r.kind         = RowKind::TableRow;
        r.item         = row.item.name;
        r.itemAux      = row.item.aux;
        r.columns      = {row.label, std::to_string(c.total * m), std::to_string(need), std::to_string(c.available)};
        r.columnColors = {
            colors::White,
            colors::White,
            need == 0             ? colors::Green
            : c.available >= need ? colors::Gold
                                  : colors::Red,
            c.available >= need ? colors::Green : colors::Red
        };
        r.tooltip = tr("ui.survival.item") + row.label + " (" + row.item.name + ")\n" + tr("ui.survival.total")
                  + stacks(c.total * m) + "\n" + tr("ui.survival.missing") + stacks(need);
        if (c.unknown) r.tooltip += "\n" + tr("ui.survival.unknown_unloaded") + std::to_string(c.unknown);
        r.actions = std::move(acts->rows);
    }
    if (rows.empty() && !report.building) b.info("empty", tr("ui.survival.no_materials")).color(colors::Gray);
    if (placement && total) {
        auto percent = [&](uint64_t v) {
            char text[16];
            std::snprintf(text, sizeof(text), "%.1f %%", total ? 100.0 * v / total : 0.0);
            return std::string(text);
        };
        b.label("summary", tr("ui.survival.summary", total * m, percent(done), percent(missing), percent(mismatch)))
            .in(Slot::Status);
    }
    b.button("clear_ignored", tr("ui.survival.clear_ignored"), [] {
         if (auto s = app::accessSession()) s->survival().materials().clearIgnored();
         requestOpen(PageId::Materials);
     }).in(Slot::FooterLeft);
    b.button(
         "clear_cache",
         tr("ui.survival.clear_cache"),
         [p] {
             if (auto s = app::accessSession()) s->survival().materials().clearCache();
             p->message = tr("ui.survival.cache_cleared_recounting");
             requestOpen(PageId::Materials);
         }
    )
        .in(Slot::FooterLeft)
        .tip(tr("ui.survival.discard_cached_counts_and_recount"));
    b.button(
         "export",
         tr("ui.survival.export"),
         [p] {
             auto s = app::accessSession();
             if (!s) return;
             using F    = app::MaterialService::ExportFormat;
             p->message = s->survival().materials().exportReport(
                 s->runtime().schematicDirectory(),
                 gDraft.exportType == 0   ? F::Text
                 : gDraft.exportType == 1 ? F::Csv
                                          : F::Json,
                 gDraft.multiplier
             );
         }
    )
        .in(Slot::FooterLeft)
        .tip(tr("ui.survival.write_the_list_to_the_materials"));
    b.input(
         "export_type",
         {},
         std::to_string(gDraft.exportType),
         [](std::string text) {
             int n;
             if (parseInt(text, n)) gDraft.exportType = n;
         }
    )
        .kind(RowKind::Choice)
        .in(Slot::FooterLeft)
        .tip(tr("ui.survival.export_format"));
    b.last().choices = {tr("ui.survival.text_file"), tr("ui.survival.csv_file"), tr("ui.survival.json_file")};
    mainMenuButton(b);
    auto stamp = [](app::MaterialReport const& r) { return std::pair{r.version, r.building ? r.scanned / 4096 : 0}; };
    p->refresh = [weak = std::weak_ptr<Page>(p), stamp, last = stamp(report)] {
        auto page = weak.lock();
        auto s    = app::accessSession();
        if (!page || !s || page->dismiss) return;
        if (stamp(s->survival().materials().report()) != last) requestOpen(PageId::Materials);
    };
}
void options(std::shared_ptr<Page> const& p) {
    p->rows.clear();
    p->title = tr("ui.survival.survival_assistance");
    PageBuilder b{p};
    auto        cfg = app::configStore().snapshot();
    using S         = app::Config::Survival;
    for (auto const& [id, label, member] : {
             std::tuple{"pick",         tr("ui.survival.pick_projection"),       &S::pickBlock           },
             {"easy",         tr("ui.survival.easy_place"),            &S::easyPlace           },
             {"restriction",  tr("ui.survival.placement_restriction"), &S::placementRestriction},
             {"hold",         tr("ui.survival.hold_to_place"),         &S::holdToPlace         },
             {"material_hud", tr("ui.survival.material_hud"),          &S::materialHud         },
             {"verifier_hud", tr("ui.survival.verifier_hud"),          &S::verifierHud         }
    }) {
        b.input(id, label, cfg.survival.*member ? "true" : "false", [p, member, id = std::string(id)](std::string) {
            auto c             = app::configStore().snapshot();
            c.survival.*member = !(c.survival.*member);
            auto error         = app::configStore().submit(c);
            if (error.empty()) error = app::configStore().save();
            p->message = error.empty() ? tr("ui.survival.saved") : error;
            if (error.empty())
                for (auto& row : p->rows)
                    if (row.id == id) row.value = c.survival.*member ? "true" : "false";
        });
        p->rows.back().kind = RowKind::Toggle;
    }
    b.input("delay", tr("ui.survival.swap_delay_ms"), std::to_string(cfg.survival.swapDelayMs), [p](std::string text) {
        int value;
        auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size()) {
            p->message = tr("ui.survival.integer_required");
            return;
        }
        auto c                 = app::configStore().snapshot();
        c.survival.swapDelayMs = value;
        p->message             = app::configStore().submit(c);
        if (p->message.empty()) p->message = app::configStore().save();
    });
    std::string slots;
    for (int slot : cfg.survival.hotbarSlots) {
        if (!slots.empty()) slots += ",";
        slots += std::to_string(slot + 1);
    }
    b.input("slots", tr("ui.survival.pick_slots"), slots, [p](std::string text) {
        std::vector<int>   slots;
        std::istringstream stream(text);
        std::string        part;
        while (std::getline(stream, part, ',')) {
            int n             = 0;
            auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), n);
            if (error != std::errc{} || end != part.data() + part.size()) {
                p->message = tr("ui.survival.use_comma_separated_slots");
                return;
            }
            slots.push_back(n - 1);
        }
        auto c                 = app::configStore().snapshot();
        c.survival.hotbarSlots = slots;
        p->message             = app::configStore().submit(c);
        if (p->message.empty()) p->message = app::configStore().save();
    });
    navButton(b, "materials", tr("ui.survival.material_list"), PageId::Materials, Slot::FooterLeft);
    mainMenuButton(b);
    b.info("help", tr("ui.survival.hold_i_for_block_information"));
}
} // namespace
void populateSurvivalPage(std::shared_ptr<Page> const& p, PageId id) {
    if (id == PageId::Materials) {
        if (auto s = app::accessSession(); s && s->survival().materials().source() == app::MaterialSource::None)
            s->survival().materials().request(gDraft.layerOnly);
        materials(p);
    } else options(p);
}
void openPlacementMaterials() {
    if (auto s = app::accessSession()) s->survival().materials().request(gDraft.layerOnly);
    requestOpen(PageId::Materials);
}
void openFileMaterials(std::filesystem::path const& file) {
    if (auto s = app::accessSession()) s->survival().materials().requestFile(file);
    requestOpen(PageId::Materials);
}
} // namespace levischematic::ui
