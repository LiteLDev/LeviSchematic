#include "levischematic/presentation/ui/pages/placement/PlacementPages.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/presentation/ui/pages/common/PageCommon.h"
#include "levischematic/presentation/ui/pages/survival/SurvivalPages.h"
#include "levischematic/schematic/placement/view/ProjectionRefresh.h"
#include "levischematic/shared/i18n/I18n.h"

namespace levischematic::ui {
namespace {
using PI = placement::PlacementInstance;
std::string gSubRegion;

template <typename F>
void mutate(std::shared_ptr<Page> const& p, PageId route, F&& operation) {
    auto s = app::accessSession();
    if (!s) return;
    auto result = operation(s->placement());
    if (!result) {
        p->message = result.error().describe();
        return;
    }
    app::refreshCurrentClientProjectionState(*s);
    requestOpen(route);
}
constexpr char const* RotationNames[] = {"NONE", "CW_90", "CW_180", "CCW_90"};
constexpr char const* MirrorNames[]   = {"NONE", "MIRROR_X", "MIRROR_Z"};
std::string           placementTip(PI const& item) {
    auto o   = item.origin;
    auto tip = tr("ui.placement.schematic") + item.name + "\n" + tr("ui.placement.file")
             + (item.filePath.empty() ? tr("ui.placement.memory") : utf8Path(item.filePath.filename())) + "\n"
             + tr("ui.placement.origin") + std::to_string(o.x) + ", " + std::to_string(o.y) + ", "
             + std::to_string(o.z);
    if (item.asset)
        tip += "\n" + tr("ui.placement.size") + std::to_string(item.asset->size.x) + " x "
             + std::to_string(item.asset->size.y) + " x " + std::to_string(item.asset->size.z);
    return tip;
}

void placementList(std::shared_ptr<Page> const& p) {
    p->title  = tr("ui.placement.schematic_placements");
    p->layout = PageLayout::List;
    auto s    = app::accessSession();
    if (!s) return;
    PageBuilder b{p};
    auto        selected = s->placement().selectedPlacementId();
    for (auto const& ref : s->placement().managedPlacements()) {
        auto const& item = ref.get();
        auto        id   = item.id;
        auto        key  = "placement:" + std::to_string(id);
        auto        acts = std::make_shared<Page>();
        PageBuilder a{acts};
        if (item.locked)
            a.button(key + ":lock", {}, [] {})
                .kind(RowKind::IconButton)
                .icon(icons::Lock)
                .tip(tr("ui.placement.locked"));
        a.button(key + ":configure", tr("ui.placement.configure"), [id] {
             if (auto s = app::accessSession()) (void)s->placement().selectPlacement(id);
             requestOpen(PageId::Placement);
         }).tip(tr("ui.placement.open_the_placement_configuration"));
        a.toggle(key + ":enabled", tr("ui.placement.enabled"), item.enabled, [p, id] {
            mutate(p, PageId::Projection, [id](auto& svc) { return svc.togglePlacementEnabled(id); });
        });
        a.button(key + ":remove", tr("ui.placement.remove"), [p, id] {
             auto s = app::accessSession();
             if (!s) return;
             auto const* item = s->placement().findPlacement(id);
             if (item && item->locked && !currentModifiers().shift) {
                 p->message = tr("ui.placement.placement_is_locked_hold_shift_to");
                 return;
             }
             auto result = s->placement().removePlacement(id);
             p->message  = result ? tr("ui.placement.removed") : result.error().describe();
             if (result) app::refreshCurrentClientProjectionState(*s);
             requestOpen(PageId::Projection);
         }).tip(tr("ui.placement.remove_this_placement_the_schematic"));
        b.button(key, item.name, [id] {
            if (auto s = app::accessSession()) {
                if (s->placement().selectedPlacementId() == id) s->placement().clearSelectedPlacement();
                else (void)s->placement().selectPlacement(id);
            }
            requestOpen(PageId::Projection);
        });
        auto& row     = b.last();
        row.kind      = RowKind::TableRow;
        row.selected  = selected == id;
        row.textColor = item.enabled ? colors::Green : colors::Red;
        row.icon      = item.filePath.empty() ? icons::Memory : icons::Litematic;
        row.tooltip   = placementTip(item) + "\n" + tr("ui.placement.click_to_select_or_deselect");
        row.actions   = std::move(acts->rows);
    }
    if (s->placement().managedPlacements().empty())
        b.info("empty", tr("ui.placement.no_placements")).color(colors::Gray);
    navButton(
        b,
        "loaded",
        tr("ui.placement.loaded_schematics"),
        PageId::Resources,
        Slot::FooterLeft,
        icons::MenuLoaded
    );
    b.button("clear_all", tr("ui.placement.remove_all"), [p] {
         confirm(p, tr("ui.placement.remove_all_placements_and_cancel"), [] {
             auto s = app::accessSession();
             if (!s) return;
             s->loads().cancelAll();
             s->placement().clearPlacements();
             app::refreshCurrentClientProjectionState(*s);
             requestOpen(PageId::Projection);
         });
     }).in(Slot::FooterLeft);
    mainMenuButton(b);
}

bool regionsModified(PI const& item) {
    for (auto const& [id, settings] : item.regions)
        if (!settings.enabled || settings.rotation || settings.mirror || settings.offset != BlockPos{0, 0, 0})
            return true;
    return false;
}

void placementConfig(std::shared_ptr<Page> const& p) {
    p->title  = tr("ui.placement.configure_placement");
    p->layout = PageLayout::Form;
    auto s    = app::accessSession();
    if (!s) return;
    PageBuilder b{p};
    auto        selected = s->placement().selectedPlacementId();
    auto const* item     = selected ? s->placement().findPlacement(*selected) : nullptr;
    if (!item) {
        b.info("none", tr("ui.placement.no_placement_selected")).at(12, 30, 300, 12);
        navButton(b, "list", tr("ui.placement.placement_list"), PageId::Projection, Slot::FooterLeft);
        mainMenuButton(b);
        return;
    }
    auto id = item->id;
    b.input("name", {}, item->name, [](std::string) {}).at(12, 22, 210, 20).tip(tr("ui.placement.placement_name"));
    b.button("rename", tr("ui.placement.rename"), [p, id] {
         std::string name;
         if (auto row = findRow(*p, "name")) name = row->value;
         mutate(p, PageId::Placement, [id, name](auto& svc) { return svc.renamePlacement(id, name); });
     }).at(226, 22, 0, 20);
    auto const& regions = item->asset ? item->asset->regions : std::vector<placement::SchematicRegion>{};
    b.label("sub_count", tr("ui.placement.subregion_count", std::max<size_t>(1, regions.size()))).at(14, 50, 0, 12);
    auto setAll = [p, id](bool enabled) {
        auto s = app::accessSession();
        if (!s) return;
        auto const* item = s->placement().findPlacement(id);
        if (!item || !item->asset) return;
        for (auto const& r : item->asset->regions) {
            auto settings    = item->regions.contains(r.id) ? item->regions.at(r.id) : placement::RegionSettings{};
            settings.enabled = enabled;
            (void)s->placement().setPlacementRegion(id, r.id, settings);
        }
        app::refreshCurrentClientProjectionState(*s);
        requestOpen(PageId::Placement);
    };
    b.button("all_on", tr("ui.placement.all_on"), [setAll] { setAll(true); })
        .at(-276, 44, 58)
        .color(colors::Green)
        .enabled(!regions.empty());
    b.button("all_off", tr("ui.placement.all_off"), [setAll] { setAll(false); })
        .at(-214, 44, 58)
        .color(colors::Red)
        .enabled(!regions.empty());
    p->listX = 12, p->listY = 66, p->listWidth = -140, p->listHeight = -32;
    for (auto const& r : regions) {
        auto        settings = item->regions.contains(r.id) ? item->regions.at(r.id) : placement::RegionSettings{};
        auto        acts     = std::make_shared<Page>();
        PageBuilder a{acts};
        auto        key = "region:" + r.id;
        a.button(key + ":configure", tr("ui.placement.configure"), [region = r.id] {
            gSubRegion = region;
            requestOpen(PageId::SubRegion);
        });
        a.toggle(key + ":enabled", tr("ui.placement.enabled"), settings.enabled, [p, id, region = r.id] {
            mutate(p, PageId::Placement, [id, region](auto& svc) {
                auto const* item = svc.findPlacement(id);
                auto        settings =
                    item && item->regions.contains(region) ? item->regions.at(region) : placement::RegionSettings{};
                settings.enabled = !settings.enabled;
                return svc.setPlacementRegion(id, region, settings);
            });
        });
        b.button(key, r.name, [] {});
        auto& row     = b.last();
        row.kind      = RowKind::TableRow;
        row.icon      = icons::Litematic;
        row.textColor = settings.enabled ? colors::Green : colors::Red;
        row.tooltip = tr("ui.placement.region") + r.name + "\n" + tr("ui.placement.offset") + std::to_string(r.offset.x)
                    + ", " + std::to_string(r.offset.y) + ", " + std::to_string(r.offset.z);
        row.actions = std::move(acts->rows);
    }
    if (regions.empty()) {
        b.button("region:main", item->name, [] {}).kind(RowKind::TableRow).icon(icons::Litematic).color(colors::Green);
    }
    float x = -130;
    b.toggle("enabled", tr("ui.placement.enabled"), item->enabled, [p, id] {
         mutate(p, PageId::Placement, [id](auto& svc) { return svc.togglePlacementEnabled(id); });
     }).at(x, 22, 98);
    b.button(
         "render",
         tr("ui.placement.r"),
         [p, id] { mutate(p, PageId::Placement, [id](auto& svc) { return svc.togglePlacementRender(id); }); }
    )
        .at(x + 100, 22, 20)
        .color(item->renderEnabled ? colors::Green : colors::Red)
        .tip(tr("ui.placement.rendering_tip", tr(item->renderEnabled ? "ui.layout.on" : "ui.layout.off")));
    b.toggle(
         "locked",
         tr("ui.placement.locked"),
         item->locked,
         [p, id] {
             mutate(p, PageId::Placement, [id](auto& svc) {
                 auto const* item = svc.findPlacement(id);
                 return svc
                     .setPlacementLocks(id, !(item && item->locked), item ? item->axisLocked : std::array<bool, 3>{});
             });
         }
    )
        .at(x, 43, 98)
        .tip(tr("ui.placement.a_locked_placement_cannot_be_moved"));
    b.button(
         "box",
         {},
         [p, id] {
             mutate(p, PageId::Placement, [id](auto& svc) {
                 auto const* item = svc.findPlacement(id);
                 return svc.setPlacementBox(id, !(item && item->boxVisible));
             });
         }
    )
        .kind(RowKind::IconButton)
        .icon(item->boxVisible ? icons::BoxOn : icons::BoxOff)
        .at(x + 102, 45, 16, 16)
        .tip(tr("ui.placement.box_tip", tr(item->boxVisible ? "ui.layout.on" : "ui.layout.off")));
    b.toggle(
         "ignore_entities",
         tr("ui.placement.ignore_entities"),
         !item->entitiesEnabled,
         [p, id] {
             mutate(p, PageId::Placement, [id](auto& svc) {
                 auto const* item = svc.findPlacement(id);
                 return svc.setPlacementEntities(id, !(item && item->entitiesEnabled));
             });
         }
    )
        .at(x, 64, 120)
        .tip(tr("ui.placement.hide_the_entities_of_this_placement"));
    b.label("origin_label", tr("ui.placement.placement_origin")).at(x, 87, 120, 12);
    for (int axis = 0; axis < 3; ++axis) {
        float y     = 99 + axis * 18.f;
        int   value = axis == 0 ? item->origin.x : axis == 1 ? item->origin.y : item->origin.z;
        auto  key   = std::string("origin") + "xyz"[axis];
        b.label(key + ":label", std::string(1, "XYZ"[axis]) + ":").at(x, y + 4, 12, 12);
        b.input(key, {}, std::to_string(value), [p, id, axis](std::string text) {
             int n;
             if (!parseInt(text, n)) {
                 p->message = tr("ui.placement.integer_required");
                 return;
             }
             mutate(p, PageId::Placement, [id, axis, n](auto& svc) {
                 auto const* item                                         = svc.findPlacement(id);
                 auto        origin                                       = item ? item->origin : BlockPos{0, 0, 0};
                 (axis == 0 ? origin.x : axis == 1 ? origin.y : origin.z) = n;
                 return svc.setPlacementOrigin(id, origin);
             });
         }).at(x + 14, y, 70, 16);
        b.adjuster(
             key + ":adjust",
             0,
             [p, id, axis](int delta) {
                 mutate(p, PageId::Placement, [id, axis, delta](auto& svc) {
                     return svc.movePlacement(id, axis == 0 ? delta : 0, axis == 1 ? delta : 0, axis == 2 ? delta : 0);
                 });
             }
        )
            .at(x + 86, y, 16, 16)
            .tip(tr(PlusMinusTip));
        b.checkbox(
             key + ":lock",
             {},
             item->axisLocked[axis],
             [p, id, axis] {
                 mutate(p, PageId::Placement, [id, axis](auto& svc) {
                     auto const* item = svc.findPlacement(id);
                     auto        axes = item ? item->axisLocked : std::array<bool, 3>{};
                     axes[axis]       = !axes[axis];
                     return svc.setPlacementLocks(id, item && item->locked, axes);
                 });
             }
        )
            .at(x + 106, y + 2, 12, 12)
            .tip(tr(LockCoordinateTip));
    }
    b.button("move_to_player", tr("ui.placement.move_to_player"), [p, id] {
         auto pos = playerBlockPos();
         if (!pos) return;
         mutate(p, PageId::Placement, [id, pos](auto& svc) { return svc.setPlacementOrigin(id, *pos); });
     }).at(x, 155, 120);
    b.input(
         "rotation",
         tr("ui.placement.rotation"),
         std::to_string(static_cast<int>(item->rotation)),
         [p, id](std::string text) {
             int choice;
             if (!parseInt(text, choice)) return;
             mutate(p, PageId::Placement, [id, choice](auto& svc) {
                 auto const* item  = svc.findPlacement(id);
                 auto        delta = (choice - (item ? static_cast<int>(item->rotation) : 0) + 4) % 4;
                 return svc.rotatePlacement(id, static_cast<PI::Rotation>(delta));
             });
         }
    )
        .kind(RowKind::Choice)
        .at(x, 176, 120)
        .tip(tr("ui.placement.left_click_next_right_click_previous"));
    b.last().choices = {RotationNames[0], RotationNames[1], RotationNames[2], RotationNames[3]};
    b.input(
         "mirror",
         tr("ui.placement.mirror"),
         std::to_string(static_cast<int>(item->mirror)),
         [p, id](std::string text) {
             int choice;
             if (!parseInt(text, choice) || choice < 0 || choice > 2) return;
             mutate(p, PageId::Placement, [id, choice](auto& svc) {
                 return svc.setPlacementMirror(id, static_cast<PI::Mirror>(choice));
             });
         }
    )
        .kind(RowKind::Choice)
        .at(x, 197, 120)
        .tip(tr("ui.placement.left_click_next_right_click_previous_2"));
    b.last().choices = {MirrorNames[0], MirrorNames[1], MirrorNames[2]};
    bool modified    = regionsModified(*item);
    b.button(
         "reset_regions",
         tr("ui.placement.reset_sub_regions"),
         [p, id] {
             auto s = app::accessSession();
             if (!s) return;
             auto const* item = s->placement().findPlacement(id);
             if (!item || !item->asset) return;
             for (auto const& r : item->asset->regions) (void)s->placement().setPlacementRegion(id, r.id, {});
             app::refreshCurrentClientProjectionState(*s);
             requestOpen(PageId::Placement);
         }
    )
        .at(x, 218, 120)
        .enabled(modified)
        .color(modified ? colors::Gold : colors::Default)
        .tip(tr("ui.placement.restore_every_sub_region_to_its"));
    b.button("materials", tr("ui.placement.material_list"), [] { openPlacementMaterials(); })
        .in(Slot::FooterLeft)
        .tip(tr("ui.placement.materials_of_this_placement_counted"));
    navButton(b, "verifier", tr("ui.placement.schematic_verifier"), PageId::Verifier, Slot::FooterLeft);
    navButton(b, "grid", tr("ui.placement.repeat_grid"), PageId::PlacementGrid, Slot::FooterLeft);
    b.last().tooltip = tr("ui.placement.repeat_this_placement_on_a_grid");
    navButton(b, "list", tr("ui.placement.placement_list"), PageId::Projection, Slot::FooterLeft);
}

void subRegion(std::shared_ptr<Page> const& p) {
    p->title  = tr("ui.placement.configure_sub_region");
    p->layout = PageLayout::Form;
    auto s    = app::accessSession();
    if (!s) return;
    PageBuilder                       b{p};
    auto                              selected = s->placement().selectedPlacementId();
    auto const*                       item     = selected ? s->placement().findPlacement(*selected) : nullptr;
    placement::SchematicRegion const* region   = nullptr;
    if (item && item->asset)
        for (auto const& r : item->asset->regions)
            if (r.id == gSubRegion) region = &r;
    navButton(b, "back_config", tr("ui.placement.placement_configuration"), PageId::Placement, Slot::FooterLeft);
    mainMenuButton(b);
    if (!item || !region) {
        b.info("none", tr("ui.placement.no_sub_region_selected")).at(12, 30, 300, 12);
        return;
    }
    auto id       = item->id;
    auto regionId = region->id;
    auto settings = item->regions.contains(regionId) ? item->regions.at(regionId) : placement::RegionSettings{};
    auto update   = [p, id, regionId](auto change) {
        mutate(p, PageId::SubRegion, [id, regionId, change](auto& svc) {
            auto const* item = svc.findPlacement(id);
            auto        settings =
                item && item->regions.contains(regionId) ? item->regions.at(regionId) : placement::RegionSettings{};
            change(settings);
            return svc.setPlacementRegion(id, regionId, settings);
        });
    };
    b.label("name", tr("ui.placement.region") + region->name + "  (" + item->name + ")").at(12, 24, 0, 12);
    b.toggle("enabled", tr("ui.placement.enabled"), settings.enabled, [update] {
         update([](placement::RegionSettings& v) { v.enabled = !v.enabled; });
     }).at(12, 40, 160);
    b.label("offset_label", tr("ui.placement.offset_2")).at(12, 66, 160, 12);
    for (int axis = 0; axis < 3; ++axis) {
        float y     = 78 + axis * 18.f;
        int   value = axis == 0 ? settings.offset.x : axis == 1 ? settings.offset.y : settings.offset.z;
        auto  key   = std::string("offset") + "xyz"[axis];
        b.label(key + ":label", std::string(1, "XYZ"[axis]) + ":").at(12, y + 4, 12, 12);
        b.input(key, {}, std::to_string(value), [p, update, axis](std::string text) {
             int n;
             if (!parseInt(text, n)) {
                 p->message = tr("ui.placement.integer_required");
                 return;
             }
             update([axis, n](placement::RegionSettings& v) {
                 (axis == 0 ? v.offset.x : axis == 1 ? v.offset.y : v.offset.z) = n;
             });
         }).at(26, y, 70, 16);
        b.adjuster(
             key + ":adjust",
             0,
             [update, axis](int delta) {
                 update([axis, delta](placement::RegionSettings& v) {
                     (axis == 0 ? v.offset.x : axis == 1 ? v.offset.y : v.offset.z) += delta;
                 });
             }
        )
            .at(98, y, 16, 16)
            .tip(tr(PlusMinusTip));
    }
    b.input(
         "rotation",
         tr("ui.placement.rotation"),
         std::to_string(settings.rotation),
         [update](std::string text) {
             int n;
             if (parseInt(text, n)) update([n](placement::RegionSettings& v) { v.rotation = n; });
         }
    )
        .kind(RowKind::Choice)
        .at(12, 136, 160);
    b.last().choices = {RotationNames[0], RotationNames[1], RotationNames[2], RotationNames[3]};
    b.input(
         "mirror",
         tr("ui.placement.mirror"),
         std::to_string(settings.mirror),
         [update](std::string text) {
             int n;
             if (parseInt(text, n)) update([n](placement::RegionSettings& v) { v.mirror = n; });
         }
    )
        .kind(RowKind::Choice)
        .at(12, 157, 160);
    b.last().choices = {MirrorNames[0], MirrorNames[1], MirrorNames[2]};
    b.button("reset", tr("ui.placement.reset"), [update] {
         update([](placement::RegionSettings& v) { v = {}; });
     }).at(12, 178, 160);
}

void grid(std::shared_ptr<Page> const& p) {
    p->title      = tr("ui.placement.repeat_grid");
    p->layout     = PageLayout::List;
    p->searchable = false;
    auto s        = app::accessSession();
    if (!s) return;
    PageBuilder b{p};
    auto        selected = s->placement().selectedPlacementId();
    auto const* item     = selected ? s->placement().findPlacement(*selected) : nullptr;
    navButton(b, "back_config", tr("ui.placement.placement_configuration"), PageId::Placement, Slot::FooterLeft);
    mainMenuButton(b);
    if (!item) {
        b.info("none", tr("ui.placement.no_placement_selected"));
        return;
    }
    auto id = item->id;
    b.info("help", tr("ui.placement.repeat_the_placement_along_each_axis")).color(colors::Gray);
    b.toggle("grid:enabled", tr("ui.placement.repeated_grid"), item->grid.enabled, [p, id] {
        mutate(p, PageId::PlacementGrid, [id](auto& svc) {
            auto const* item = svc.findPlacement(id);
            auto        grid = item ? item->grid : placement::GridSettings{};
            grid.enabled     = !grid.enabled;
            return svc.setPlacementGrid(id, grid);
        });
    });
    for (int axis = 0; axis < 3; ++axis)
        for (int field = 0; field < 3; ++field) {
            int value = field == 0 ? item->grid.spacing[axis]
                      : field == 1 ? item->grid.negative[axis]
                                   : item->grid.positive[axis];
            b.input(
                "grid:" + std::to_string(axis) + ":" + std::to_string(field),
                std::string(1, "XYZ"[axis])
                    + (field == 0   ? tr("ui.placement.spacing")
                       : field == 1 ? tr("ui.placement.negative_copies")
                                    : tr("ui.placement.positive_copies")),
                std::to_string(value),
                [p, id, axis, field](std::string text) {
                    int n;
                    if (!parseInt(text, n)) {
                        p->message = tr("ui.placement.integer_required");
                        return;
                    }
                    mutate(p, PageId::PlacementGrid, [id, axis, field, n](auto& svc) {
                        auto const* item = svc.findPlacement(id);
                        auto        grid = item ? item->grid : placement::GridSettings{};
                        (field == 0 ? grid.spacing : field == 1 ? grid.negative : grid.positive)[axis] = n;
                        return svc.setPlacementGrid(id, grid);
                    });
                }
            );
        }
}
} // namespace

void populatePlacementPage(std::shared_ptr<Page> const& page, PageId id) {
    switch (id) {
    case PageId::Projection:
        placementList(page);
        break;
    case PageId::Placement:
        page->placementSettings = true;
        placementConfig(page);
        break;
    case PageId::SubRegion:
        subRegion(page);
        break;
    case PageId::PlacementGrid:
        grid(page);
        break;
    default:
        break;
    }
}
} // namespace levischematic::ui
