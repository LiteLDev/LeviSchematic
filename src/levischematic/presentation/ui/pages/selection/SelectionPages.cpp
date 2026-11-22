#include "levischematic/presentation/ui/pages/selection/SelectionPages.h"
#include "levischematic/app/lifecycle/AppKernel.h"
#include "levischematic/presentation/ui/pages/browser/BrowserPages.h"
#include "levischematic/presentation/ui/pages/common/PageCommon.h"
#include "levischematic/selection/model/Box.h"
#include "levischematic/shared/i18n/I18n.h"
#include <sstream>

namespace levischematic::ui {
namespace {
using selection::tool::CornerMode;
using selection::tool::Element;

BlockPos automaticOrigin(selection::SelectionState const& state) {
    std::optional<BlockPos> minimum;
    auto                    include = [&](std::optional<BlockPos> a, std::optional<BlockPos> b) {
        if (!a || !b) return;
        BlockPos at{std::min(a->x, b->x), std::min(a->y, b->y), std::min(a->z, b->z)};
        minimum =
            minimum ? BlockPos{std::min(minimum->x, at.x), std::min(minimum->y, at.y), std::min(minimum->z, at.z)} : at;
    };
    if (state.normal)
        for (auto const& r : state.regions) include(r.corner1, r.corner2);
    else include(state.corner1, state.corner2);
    return minimum.value_or(BlockPos{0, 0, 0});
}
int& axisOf(BlockPos& pos, int axis) { return axis == 0 ? pos.x : axis == 1 ? pos.y : pos.z; }

void column(PageBuilder& b, std::shared_ptr<Page> const& p, int point, float x, selection::SelectionState const& st) {
    std::optional<BlockPos> value  = point == 0 ? st.corner1 : point == 1 ? st.corner2 : st.customOrigin;
    bool                    active = point == 0 ? st.selectedElement == Element::Corner1
                                   : point == 1 ? st.selectedElement == Element::Corner2
                                                : false;
    auto                    set    = [p, point](auto change) {
        auto s = app::accessSession();
        if (!s) return;
        auto const& state = s->selection().state();
        auto        pos   = (point == 0 ? state.corner1 : point == 1 ? state.corner2 : state.customOrigin);
        BlockPos    at    = pos ? *pos : playerBlockPos().value_or(BlockPos{0, 0, 0});
        change(at);
        if (point == 0) s->selection().setSelectionCorner1(at);
        else if (point == 1) s->selection().setSelectionCorner2(at);
        else s->selection().setOrigin(at);
        requestOpen(PageId::Selection);
    };
    auto key = std::string("point") + std::to_string(point);
    if (point < 2)
        b.checkbox(
             key,
             point == 0 ? tr("ui.selection.corner_1") : tr("ui.selection.corner_2"),
             active,
             [point] {
                 if (auto s = app::accessSession())
                     s->selection().selectElement(
                         s->selection().state().selectedElement == (point == 0 ? Element::Corner1 : Element::Corner2)
                             ? Element::None
                             : (point == 0 ? Element::Corner1 : Element::Corner2)
                     );
                 requestOpen(PageId::Selection);
             }
        )
            .at(x, 112, 0, 12)
            .tip(tr("ui.selection.when_checked_tool_clicks_move_this"))
            .color(value ? colors::White : colors::Gray);
    else b.label(key, tr("ui.selection.origin")).at(x, 113, 0, 12);
    for (int axis = 0; axis < 3; ++axis) {
        float y = 128 + axis * 20.f;
        b.label(key + ":label" + std::to_string(axis), std::string(1, "XYZ"[axis]) + ":").at(x, y + 4, 12, 12);
        b.input(
             key + ":" + std::to_string(axis),
             {},
             value ? std::to_string(
                         axis == 0   ? value->x
                         : axis == 1 ? value->y
                                     : value->z
                     )
                   : "",
             [p, set, axis](std::string text) {
                 int n;
                 if (!parseInt(text, n)) {
                     p->message = tr("ui.selection.integer_required");
                     return;
                 }
                 set([axis, n](BlockPos& at) { axisOf(at, axis) = n; });
             }
        ).at(x + 14, y, 70, 16);
        b.adjuster(
             key + ":adjust" + std::to_string(axis),
             1,
             [set, axis](int delta) { set([axis, delta](BlockPos& at) { axisOf(at, axis) += delta; }); }
        )
            .at(x + 86, y, 16, 16)
            .tip(tr(PlusMinusTipCtrl));
    }
    b.button(key + ":player", tr("ui.selection.move_to_player"), [set] {
         auto pos = playerBlockPos();
         if (pos) set([pos](BlockPos& at) { at = *pos; });
     }).at(x + 4, 190, 100);
}

void editor(std::shared_ptr<Page> const& p) {
    auto s = app::accessSession();
    if (!s) return;
    auto const& st = s->selection().state();
    p->title       = st.normal ? tr("ui.selection.area_editor_normal") : tr("ui.selection.area_editor_simple");
    p->crumb       = tr("ui.selection.area_editor");
    p->layout      = PageLayout::Form;
    PageBuilder b{p};
    b.toggle(
         "mode",
         tr("ui.selection.selection_mode"),
         st.normal,
         [] {
             if (auto s = app::accessSession()) s->selection().setNormal(!s->selection().state().normal);
             requestOpen(PageId::Selection);
         }
    )
        .in(Slot::TopBar)
        .tip(tr("ui.selection.simple_one_box_normal_several_named"));
    b.last().kind    = RowKind::Choice;
    b.last().value   = st.normal ? "1" : "0";
    b.last().choices = {tr("ui.selection.simple"), tr("ui.selection.normal")};
    b.last().submit  = [](std::string) {
        if (auto s = app::accessSession()) s->selection().setNormal(!s->selection().state().normal);
        requestOpen(PageId::Selection);
    };
    b.input(
         "corner_mode",
         tr("ui.selection.corner_mode"),
         st.cornerMode == CornerMode::Corners ? "0" : "1",
         [](std::string) {
             if (auto s = app::accessSession()) s->selection().cycleCornerMode();
             requestOpen(PageId::Selection);
         }
    )
        .kind(RowKind::Choice)
        .in(Slot::TopBar)
        .tip(tr("ui.selection.corners_tool_clicks_set_the_corners"));
    b.last().choices = {tr("ui.selection.corners"), tr("ui.selection.expand")};
    b.toggle(
         "manual_origin",
         tr("ui.selection.manual_origin"),
         st.customOrigin.has_value(),
         [] {
             if (auto s = app::accessSession()) {
                 auto const& state = s->selection().state();
                 s->selection().setOrigin(state.customOrigin ? std::optional<BlockPos>{} : automaticOrigin(state));
             }
             requestOpen(PageId::Selection);
         }
    )
        .in(Slot::TopBar)
        .tip(tr("ui.selection.when_off_the_origin_is_the_minimum"));
    b.label("name_label", tr("ui.selection.selection_name")).at(12, 46, 0, 12);
    b.input("name", {}, st.name, [](std::string) {}).at(12, 58, 200, 18);
    b.button("set_name", tr("ui.selection.set"), [p] {
         std::string name;
         if (auto row = findRow(*p, "name")) name = row->value;
         if (auto s = app::accessSession()) s->selection().setName(name);
         requestOpen(PageId::Selection);
     }).at(216, 57, 0, 20);
    auto region =
        std::find_if(st.regions.begin(), st.regions.end(), [&](auto const& r) { return r.id == st.selectedRegion; });
    if (st.normal && region != st.regions.end()) {
        b.label("box_label", tr("ui.selection.sub_region_name")).at(12, 80, 0, 12);
        b.input("box_name", {}, region->name, [](std::string) {}).at(12, 92, 200, 18);
        b.button("set_box_name", tr("ui.selection.set"), [p, id = region->id] {
             std::string name;
             if (auto row = findRow(*p, "box_name")) name = row->value;
             if (auto s = app::accessSession())
                 if (!s->selection().renameRegion(id, name)) p->message = tr("ui.selection.empty_or_duplicate_name");
             requestOpen(PageId::Selection);
         }).at(216, 91, 0, 20);
    }
    column(b, p, 0, 12, st);
    column(b, p, 1, 122, st);
    if (st.customOrigin) column(b, p, 2, 232, st);
    b.button("save", tr("ui.selection.save_schematic"), [] { requestOpen(PageId::SaveSelection); })
        .at(16, 214, 0, 20)
        .tip(tr("ui.selection.save_the_world_blocks_in_the"));
    b.button(
         "analyze",
         tr("ui.selection.analyze_area"),
         [p] {
             auto s = app::accessSession();
             if (!s) return;
             auto const&                                state = s->selection().state();
             std::vector<std::pair<BlockPos, BlockPos>> boxes;
             if (state.normal)
                 for (auto const& r : state.regions) {
                     if (r.corner1 && r.corner2) boxes.push_back({*r.corner1, *r.corner2});
                 }
             else if (state.corner1 && state.corner2) boxes.push_back({*state.corner1, *state.corner2});
             if (boxes.empty()) {
                 p->message = tr("ui.selection.incomplete_selection");
                 return;
             }
             s->survival().materials().requestArea(std::move(boxes), playerDimension(), state.name);
             requestOpen(PageId::Materials);
         }
    )
        .at(126, 214, 0, 20)
        .tip(tr("ui.selection.count_the_materials_of_the_world"));
    if (st.normal) {
        p->listX = -150, p->listY = 46, p->listWidth = -12, p->listHeight = -32;
        p->searchable = false;
        for (auto const& r : st.regions) {
            auto        acts = std::make_shared<Page>();
            PageBuilder a{acts};
            a.button(
                 "region:" + std::to_string(r.id) + ":remove",
                 "-",
                 [id = r.id] {
                     if (auto s = app::accessSession()) (void)s->selection().removeRegion(id);
                     requestOpen(PageId::Selection);
                 }
            )
                .color(colors::Red)
                .tip(tr("ui.selection.remove_this_sub_region"));
            b.button("region:" + std::to_string(r.id), r.name, [id = r.id] {
                if (auto s = app::accessSession()) (void)s->selection().selectRegion(id);
                requestOpen(PageId::Selection);
            });
            auto& row    = b.last();
            row.kind     = RowKind::TableRow;
            row.selected = r.id == st.selectedRegion;
            row.actions  = std::move(acts->rows);
            row.tooltip  = r.corner1 && r.corner2
                             ? "(" + std::to_string(r.corner1->x) + ", " + std::to_string(r.corner1->y) + ", "
                                   + std::to_string(r.corner1->z) + ") → (" + std::to_string(r.corner2->x) + ", "
                                   + std::to_string(r.corner2->y) + ", " + std::to_string(r.corner2->z) + ")"
                             : tr("ui.selection.incomplete");
        }
        b.button("add_region", tr("ui.selection.create_sub_region"), [p] {
             auto count = app::accessSession() ? app::accessSession()->selection().state().regions.size() : 0;
             prompt(
                 p,
                 tr("ui.selection.sub_region_name_2"),
                 "Region " + std::to_string(count + 1),
                 [p](std::string name) {
                     auto s = app::accessSession();
                     if (s && !s->selection().addRegion(std::move(name)))
                         p->message = tr("ui.selection.empty_or_duplicate_name");
                     requestOpen(PageId::Selection);
                 }
             );
         }).in(Slot::FooterLeft);
    }
    b.button(
         "grow",
         tr("ui.selection.grow"),
         [p] {
             auto s = app::accessSession();
             if (s)
                 p->message = s->selection().startGrow(playerDimension()) ? tr("ui.selection.growing")
                                                                          : tr("ui.selection.incomplete_selection");
         }
    )
        .in(Slot::FooterLeft)
        .right([] {
            if (auto s = app::accessSession()) s->selection().cancelGrow();
        })
        .tip(tr("ui.selection.grow_the_box_to_enclose_the"));
    b.button(
         "resize",
         tr("ui.selection.expand_shrink"),
         [p] {
             prompt(p, tr("ui.selection.amount_per_axis_negative_shrinks_e_g"), "1 0 1", [p](std::string text) {
                 std::istringstream input(text);
                 BlockPos           amount{0, 0, 0};
                 if (!(input >> amount.x >> amount.y >> amount.z)) {
                     p->message = tr("ui.selection.enter_three_integers");
                     return;
                 }
                 auto s = app::accessSession();
                 if (s && !s->selection().resize(amount)) p->message = tr("ui.selection.invalid_selection_or_overflow");
                 requestOpen(PageId::Selection);
             });
         }
    )
        .in(Slot::FooterLeft)
        .tip(tr("ui.selection.expand_or_shrink_the_box_on_every"));
    b.button(
         "clear",
         tr("ui.selection.clear"),
         [] {
             if (auto s = app::accessSession()) s->selection().clearSelection();
             requestOpen(PageId::Selection);
         }
    )
        .in(Slot::FooterLeft)
        .tip(tr("ui.selection.clear_both_corners"));
    navButton(
        b,
        "browser",
        tr("ui.selection.area_selections"),
        PageId::AreaSelections,
        Slot::FooterLeft,
        icons::MenuAreaSelections
    );
    mainMenuButton(b);
    if (auto file = currentAreaSelectionFile())
        p->message = tr("ui.selection.selection_file") + utf8Path(file->filename());
    p->save = [] {
        saveCurrentAreaSelection();
        return true;
    };
    p->refresh = [weak = std::weak_ptr<Page>(p), revision = st.revision] {
        auto page = weak.lock();
        auto s    = app::accessSession();
        if (!page || !s || page->dismiss) return;
        auto status = s->selection().growStatus();
        if (status.starts_with("Growing") && page->message != status) page->message = status;
        if (s->selection().state().revision != revision) requestOpen(PageId::Selection);
    };
}
} // namespace

void populateSelectionPage(std::shared_ptr<Page> const& page) { editor(page); }
} // namespace levischematic::ui
