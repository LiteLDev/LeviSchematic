#include "levischematic/presentation/ui/layout/UiLayout.h"
#include "levischematic/shared/i18n/I18n.h"
#include <algorithm>
#include <cmath>

namespace levischematic::ui {
bool UiRect::contains(float px, float py) const {
    return width > 0 && height > 0 && px >= x && py >= y && px < x + width && py < y + height;
}
UiRect intersect(UiRect a, UiRect b) {
    float x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    return {
        x,
        y,
        std::max(0.f, std::min(a.x + a.width, b.x + b.width) - x),
        std::max(0.f, std::min(a.y + a.height, b.y + b.height) - y)
    };
}
Widget const* UiFrame::hit(float x, float y) const {
    for (auto it = widgets.rbegin(); it != widgets.rend(); ++it)
        if (it->enabled && it->op != UiOp::None && it->rect.contains(x, y) && it->clip.contains(x, y)) return &*it;
    return nullptr;
}
int configCategory(std::string_view p) {
    if (p.find("Color") != p.npos || p.find("color") != p.npos) return 4;
    if (p.starts_with("/hud/") || p == "hud_position" || p == "hud_layout" || p.find("Hud") != p.npos) return 2;
    if (p.starts_with("/projection/") || p.starts_with("/hull/") || p.starts_with("/visibility/")
        || p.starts_with("/selection/"))
        return 3;
    return 1;
}
std::vector<std::string> wrapLines(std::string const& value, float maxWidth, TextWidth const& measure) {
    std::vector<std::string> lines;
    std::string              line;
    auto                     next = [](std::string const& text, size_t pos) {
        if (pos < text.size()) ++pos;
        while (pos < text.size() && (static_cast<unsigned char>(text[pos]) & 0xc0) == 0x80) ++pos;
        return pos;
    };
    for (size_t i = 0; i < value.size();) {
        auto end       = next(value, i);
        auto character = value.substr(i, end - i);
        i              = end;
        if (character == "\n") {
            lines.push_back(std::move(line));
            line.clear();
            continue;
        }
        if (!line.empty() && measure(line + character) > maxWidth) {
            lines.push_back(std::move(line));
            line.clear();
        }
        line += character;
    }
    lines.push_back(std::move(line));
    return lines;
}
UiRect tooltipRect(
    std::vector<std::string> const& lines,
    float                           mouseX,
    float                           mouseY,
    float                           width,
    float                           height,
    TextWidth const&                measure
) {
    float w = 0;
    for (auto const& line : lines) w = std::max(w, measure(line));
    w       += 8;
    float h  = lines.size() * 10.f + 6;
    float x = mouseX + 12, y = mouseY - 12;
    if (x + w > width - 2) x = std::max(2.f, mouseX - 12 - w);
    if (y + h > height - 2) y = height - 2 - h;
    return {x, std::max(2.f, y), w, h};
}
std::vector<UiTextRun>
layoutTextRuns(std::string const& value, UiRect bounds, UiRect clip, TextWidth const& measure, bool center) {
    std::vector<UiTextRun> result;
    if (bounds.width <= 0 || bounds.height <= 0 || clip.width <= 0 || clip.height <= 0) return result;
    auto next = [](std::string const& text, size_t pos) {
        if (pos < text.size()) ++pos;
        while (pos < text.size() && (static_cast<unsigned char>(text[pos]) & 0xc0) == 0x80) ++pos;
        return pos;
    };
    std::vector<std::string> lines;
    std::string              line;
    bool                     multiline = bounds.height > 16;
    for (size_t i = 0; i < value.size();) {
        auto end       = next(value, i);
        auto character = value.substr(i, end - i);
        i              = end;
        if (character == "\n") {
            lines.push_back(std::move(line));
            line.clear();
            if (!multiline) break;
            continue;
        }
        if (multiline && !line.empty() && measure(line + character) > bounds.width) {
            lines.push_back(std::move(line));
            line.clear();
        }
        line += character;
    }
    lines.push_back(std::move(line));
    for (size_t i = 0; i < lines.size(); ++i) {
        float y = bounds.y + i * (multiline ? 10.f : 12.f);

        if (y < clip.y || y + 9 > clip.y + clip.height) continue;
        auto text = lines[i];
        if (!multiline && measure(text) > bounds.width) {
            while (!text.empty() && measure(text + "...") > bounds.width) {
                size_t pos = text.size() - 1;
                while (pos && (static_cast<unsigned char>(text[pos]) & 0xc0) == 0x80) --pos;
                text.resize(pos);
            }
            if (measure(text + "...") <= bounds.width) text += "...";
        }
        float x = center ? bounds.x + (bounds.width - measure(text)) / 2 : bounds.x;
        while (!text.empty() && x < clip.x) {
            auto end  = next(text, 0);
            x        += measure(text.substr(0, end));
            text.erase(0, end);
        }
        while (!text.empty() && x + measure(text) > clip.x + clip.width) {
            size_t pos = text.size() - 1;
            while (pos && (static_cast<unsigned char>(text[pos]) & 0xc0) == 0x80) --pos;
            text.resize(pos);
        }
        if (!text.empty())
            result.push_back({
                text,
                {x, y, measure(text), 8}
            });
    }
    return result;
}

namespace {
constexpr float RowGap = 22, ButtonHeight = 20, Scrollbar = 8;
std::string     onOff(bool value, bool yesNo = false) {
    if (yesNo) return value ? tr("ui.layout.yes") : tr("ui.layout.no");
    return value ? tr("ui.layout.on") : tr("ui.layout.off");
}
int choiceIndex(Row const& r) {
    try {
        return std::stoi(r.value);
    } catch (...) {
        return 0;
    }
}
std::string choiceText(Row const& r) {
    if (r.choices.empty()) return {};
    return r.choices[std::clamp(choiceIndex(r), 0, static_cast<int>(r.choices.size()) - 1)];
}

struct Builder {
    Page const&        page;
    PageId             id;
    UiViewState const& state;
    float              width, height;
    TextWidth const&   measure;
    UiFrame            f;
    UiRect             screen;

    Widget&
    add(std::string key,
        std::string row,
        std::string text,
        UiRect      r,
        WidgetKind  kind,
        UiOp        op    = UiOp::None,
        int         index = 0,
        bool        fixed = true) {
        Widget w;
        w.id    = row.empty() || (page.dismiss && key == "back") ? key : "row:" + key;
        w.row   = std::move(row);
        w.text  = std::move(text);
        w.rect  = r;
        w.clip  = fixed ? screen : f.body;
        w.kind  = kind;
        w.op    = op;
        w.index = index;
        f.widgets.push_back(std::move(w));
        return f.widgets.back();
    }
    float autoWidth(Row const& r) {
        if (r.width > 0) return r.width;
        switch (r.kind) {
        case RowKind::Adjust:
        case RowKind::IconButton:
            return r.icon.valid() ? r.icon.w : 16;
        case RowKind::Checkbox:
            return 14 + measure(r.label);
        case RowKind::Label:
        case RowKind::Info:
            return measure(r.label) + 2;
        case RowKind::Text:
            return 80;
        default:
            break;
        }
        std::string text = r.label;
        if (r.kind == RowKind::Toggle) text += (r.label.empty() ? "" : ": ") + onOff(r.value == "true", r.yesNo);
        if (r.kind == RowKind::Choice) text += (r.label.empty() ? "" : ": ") + choiceText(r);
        return std::max(20.f, measure(text) + 12 + (r.icon.valid() ? r.icon.w + 4 : 0));
    }
    float controlHeight(Row const& r) {
        if (r.kind == RowKind::Adjust || r.kind == RowKind::IconButton) return r.icon.valid() ? r.icon.h : 16;
        if (r.kind == RowKind::Checkbox || r.kind == RowKind::Label || r.kind == RowKind::Info) return 12;
        return r.placed && r.height > 0 ? r.height : ButtonHeight;
    }
    void control(Row const& r, UiRect rect, bool fixed, std::string key = {}) {
        if (key.empty()) key = r.id;
        auto decorate = [&](Widget& w) -> Widget& {
            w.tooltip  = r.tooltip;
            w.color    = r.textColor;
            w.enabled  = r.enabled;
            w.selected = r.selected;
            return w;
        };
        switch (r.kind) {
        case RowKind::Label:
        case RowKind::Info:
            decorate(add(key, r.id, r.label, rect, WidgetKind::Label, UiOp::None, 0, fixed)).enabled = true;
            return;
        case RowKind::Checkbox: {
            auto& w = decorate(add(key, r.id, r.label, rect, WidgetKind::Checkbox, UiOp::Submit, 0, fixed));
            auto  i = r.checked ? icons::CheckboxOn : icons::CheckboxOff;
            w.icon  = {i.u, i.v, i.w, i.h};
            return;
        }
        case RowKind::Adjust:
        case RowKind::IconButton: {
            auto& w = decorate(
                add(key,
                    r.id,
                    {},
                    rect,
                    WidgetKind::IconButton,
                    r.kind == RowKind::Adjust ? UiOp::Adjust : UiOp::Submit,
                    0,
                    fixed)
            );
            auto icon  = r.icon.valid() ? r.icon : icons::PlusMinus;
            w.icon     = {icon.u, icon.v, icon.w, icon.h};
            w.atlas    = icon.atlas;
            w.variants = icon.variants;
            return;
        }
        case RowKind::Text:
        case RowKind::File:
            decorate(add(key + ":value", r.id, r.value, rect, WidgetKind::Text, UiOp::Submit, 0, fixed));
            return;
        case RowKind::Slider: {
            auto& w = decorate(add(key + ":slider", r.id, r.value, rect, WidgetKind::Slider, UiOp::Submit, 0, fixed));
            try {
                w.value = sliderNativeValue(std::stod(r.value), r.minimum, r.maximum, r.step);
            } catch (...) {}
            return;
        }
        case RowKind::Toggle: {
            bool  on = r.value == "true";
            auto& w  = decorate(
                add(key, r.id, r.label.empty() ? "" : r.label + ": ", rect, WidgetKind::Button, UiOp::Submit, 0, fixed)
            );
            w.suffix      = onOff(on, r.yesNo);
            w.suffixColor = on ? colors::Green : colors::Red;
            w.centered    = true;
            return;
        }
        case RowKind::Choice: {
            auto  text = choiceText(r);
            auto& w    = decorate(
                add(key,
                    r.id,
                    r.label.empty() ? text : r.label + ": " + text,
                    rect,
                    WidgetKind::Button,
                    UiOp::Choice,
                    0,
                    fixed)
            );
            w.centered = true;
            return;
        }
        default: {
            auto& w = decorate(add(key, r.id, r.label, rect, WidgetKind::Button, UiOp::Submit, 0, fixed));
            if (r.icon.valid()) {
                w.icon  = {r.icon.u, r.icon.v, r.icon.w, r.icon.h};
                w.atlas = r.icon.atlas;
            }
            w.centered = !r.icon.valid();
            return;
        }
        }
    }

    void title() {
        if (!page.title.empty()) add("title", {}, page.title, {12, 7, width - 24, 12}, WidgetKind::Label);
        if (!page.message.empty()) {
            float w = std::min(width * .5f, measure(page.message) + 12);
            add("message_panel", {}, {}, {width - 8 - w, 4, w, 14}, WidgetKind::Panel);
            add("message", {}, page.message, {width - 8 - w + 6, 7, w - 12, 10}, WidgetKind::Label).color =
                colors::Yellow;
        }
    }
    float topBar(float y) {
        bool  any = false;
        float lx = 12, rx = width - 12;
        for (auto const& r : page.rows) {
            if (r.placed || (r.slot != Slot::TopBar && r.slot != Slot::TopRight)) continue;
            any     = true;
            float w = autoWidth(r), h = controlHeight(r);
            float cy = y + (ButtonHeight - h) / 2;
            if (r.slot == Slot::TopBar) {
                control(r, {lx, cy, w, h}, true);
                lx += w + 2;
            } else {
                rx -= w;
                control(r, {rx, cy, w, h}, true);
                rx -= 2;
            }
        }
        return any ? y + ButtonHeight + 4 : y;
    }
    float footer() {
        bool  any = false;
        float lx = 12, rx = width - 12, y = height - 26;
        for (auto const& r : page.rows) {
            if (r.placed || (r.slot != Slot::FooterLeft && r.slot != Slot::FooterRight)) continue;
            any     = true;
            float w = std::min(autoWidth(r), width - 24);
            float h = controlHeight(r);
            if (r.slot == Slot::FooterLeft) {
                control(r, {lx, y + (ButtonHeight - h) / 2, w, h}, true);
                lx += w + 4;
            } else {
                rx -= w;
                control(r, {rx, y + (ButtonHeight - h) / 2, w, h}, true);
                rx -= 4;
            }
        }
        if (!any && id != PageId::MainMenu) {
            float w = std::max(60.f, measure(tr("ui.layout.main_menu")) + 12);
            add("back", {}, "Back", {width - 12 - w, y, w, ButtonHeight}, WidgetKind::Button, UiOp::Back).centered =
                true;
            any = true;
        }
        float bottom = any ? y - 4 : height - 6;
        int   status = 0;
        for (auto const& r : page.rows)
            if (!r.placed && r.slot == Slot::Status) ++status;
        float sy = bottom - status * 16.f;
        for (auto const& r : page.rows) {
            if (r.placed || r.slot != Slot::Status) continue;
            float w = std::min(autoWidth(r), width - 24);
            control(r, {12, sy + 2, w, controlHeight(r)}, true);
            sy += 16;
        }
        return bottom - status * 16.f;
    }
    void placedRows() {
        for (auto const& r : page.rows) {
            if (!r.placed) continue;
            float w = r.width > 0 ? r.width : autoWidth(r);
            float x = r.x < 0 ? width + r.x : r.x;
            float y = r.y < 0 ? height + r.y : r.y;
            control(r, {x, y, w, controlHeight(r)}, true);
        }
    }
    bool matches(Row const& r) {
        if (r.placed || r.slot != Slot::Body) return false;
        if (id == PageId::Settings && state.category != 0 && configCategory(r.id) != state.category) return false;
        auto text = r.label + " " + r.value + " " + r.id;
        for (auto const& c : r.columns) text += " " + c;
        auto query = state.search;
        auto lower = [](std::string& s) {
            for (auto& c : s)
                if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        };
        lower(text);
        lower(query);
        return query.empty() || text.find(query) != std::string::npos;
    }
    float search(float x, float y, float w) {
        if (!page.searchable) return 0;
        auto& icon   = add("search_icon", {}, {}, {x, y, 12, 12}, WidgetKind::IconButton, UiOp::SearchToggle);
        icon.icon    = {icons::Search.u, icons::Search.v, 12, 12};
        icon.tooltip = tr("ui.layout.search");
        if ((state.searchOpen || !state.search.empty()) && w > 30)
            add("search", {}, state.search, {x + 16, y - 2, w - 16, 16}, WidgetKind::Text, UiOp::Search);
        return 16;
    }
    void scrollbar() {
        if (f.maxScroll <= 0) return;
        float thumb = std::max(12.f, f.body.height * f.body.height / f.contentHeight);
        auto& c =
            add("scrollbar",
                {},
                {},
                {f.body.x + f.body.width + 2, f.body.y + f.scroll / f.maxScroll * (f.body.height - thumb), 6, thumb},
                WidgetKind::Scrollbar,
                UiOp::Scroll);
        c.value = f.scroll / f.maxScroll;
    }
    void finishScroll() {
        f.maxScroll = std::max(0.f, f.contentHeight - f.body.height);
        f.scroll    = std::clamp(std::isfinite(state.scroll) ? state.scroll : 0.f, 0.f, f.maxScroll);
    }
    float infoHeight(Row const& r, float w) {
        return std::max(14.f, wrapLines(r.label, w - 4, measure).size() * 10.f + 4);
    }
    float bodyHeight(Row const& r, float w, float entryHeight) {
        if (r.kind == RowKind::Info || r.kind == RowKind::Label) return infoHeight(r, w);
        if (r.kind == RowKind::TableRow || r.kind == RowKind::Entry) return entryHeight;
        return RowGap;
    }
    void listBody(float entryHeight = RowGap) {
        std::vector<Row const*> rows;
        for (auto const& r : page.rows)
            if (matches(r)) rows.push_back(&r);
        float w = f.body.width;
        for (auto r : rows) f.contentHeight += bodyHeight(*r, w, entryHeight);
        finishScroll();
        float y   = f.body.y - f.scroll;
        bool  odd = false;
        for (auto r : rows) {
            float h = bodyHeight(*r, w, entryHeight);
            if (y + h >= f.body.y && y <= f.body.y + f.body.height) entry(*r, {f.body.x, y, w, h}, odd);
            if (r->kind == RowKind::TableRow || r->kind == RowKind::Entry) odd = !odd;
            y += h;
        }
    }
    void entry(Row const& r, UiRect rect, bool odd) {
        float x = rect.x, y = rect.y, w = rect.width;
        if (r.kind == RowKind::Info || r.kind == RowKind::Label) {
            auto& l =
                add(r.id,
                    r.id,
                    r.label,
                    {x + 2, y + 2, w - 4, rect.height - 2},
                    WidgetKind::Label,
                    UiOp::None,
                    0,
                    false);
            l.color   = r.textColor;
            l.tooltip = r.tooltip;
            return;
        }
        if (r.kind == RowKind::TableRow || r.kind == RowKind::Entry) {
            auto& bg =
                add(r.id, r.id, {}, {x, y, w, rect.height - 1}, WidgetKind::ListRow, UiOp::Submit, odd ? 1 : 0, false);
            bg.selected = r.selected;
            bg.tooltip  = r.tooltip;
            float rx    = x + w - 2;
            for (auto it = r.actions.rbegin(); it != r.actions.rend(); ++it) {
                float bw = autoWidth(*it);
                float bh =
                    it->kind == RowKind::Adjust || it->kind == RowKind::IconButton || it->kind == RowKind::Checkbox
                        ? controlHeight(*it)
                        : std::min(ButtonHeight, rect.height - 2);
                rx -= bw;
                control(*it, {rx, y + (rect.height - bh) / 2, bw, bh}, false);
                rx -= 2;
            }
            float tx = x + 4;
            if (r.icon.valid()) {
                auto& icon =
                    add(r.id + ":icon",
                        r.id,
                        {},
                        {tx, y + (rect.height - r.icon.h) / 2, r.icon.w, r.icon.h},
                        WidgetKind::IconButton,
                        UiOp::None,
                        0,
                        false);
                icon.icon   = {r.icon.u, r.icon.v, r.icon.w, r.icon.h};
                icon.atlas  = r.icon.atlas;
                tx         += r.icon.w + 4;
            }
            float space = std::max(0.f, rx - tx - 4);
            auto& name =
                add(r.id + ":name",
                    r.id,
                    r.label,
                    {tx, y + (rect.height - 8) / 2, space, 10},
                    WidgetKind::Label,
                    UiOp::None,
                    0,
                    false);
            name.color = r.textColor;
            if (!r.value.empty()) {
                float nw = std::min(measure(r.label) + 8, space);
                add(r.id + ":detail",
                    r.id,
                    r.value,
                    {tx + nw, y + (rect.height - 8) / 2, std::max(0.f, space - nw), 10},
                    WidgetKind::Label,
                    UiOp::None,
                    0,
                    false)
                    .color = colors::Gray;
            }
            return;
        }
        bool labelled = r.kind != RowKind::Action && r.kind != RowKind::Checkbox && r.kind != RowKind::IconButton;
        if (!labelled) {
            float h = controlHeight(r);
            control(r, {x, y + (RowGap - 2 - h) / 2, std::min(w, std::max(120.f, autoWidth(r))), h}, false);
            return;
        }
        float cw = std::min(w * .5f, 180.f);
        auto& l =
            add(r.id + ":label",
                r.id,
                r.label,
                {x + 2, y + 6, w - cw - 8, 10},
                WidgetKind::Label,
                UiOp::None,
                0,
                false);
        l.tooltip = r.tooltip;
        if (r.kind == RowKind::Range) {
            add(r.id + ":first",
                r.id,
                r.value,
                {x + w - cw, y, cw / 2 - 3, ButtonHeight},
                WidgetKind::Text,
                UiOp::Submit,
                0,
                false);
            add(r.id + ":second",
                r.id,
                r.value2,
                {x + w - cw / 2 + 3, y, cw / 2 - 3, ButtonHeight},
                WidgetKind::Text,
                UiOp::Submit,
                1,
                false);
            return;
        }
        Row copy = r;
        if (r.kind == RowKind::Toggle || r.kind == RowKind::Choice) copy.label.clear();
        control(copy, {x + w - cw, y, cw, ButtonHeight}, false);
    }

    void menu() {
        float bw = 120;
        for (auto const& r : page.rows)
            if (r.slot == Slot::Body) bw = std::max(bw, autoWidth(r) + 18);
        bool  two    = width >= 24 + bw * 2 + 20;
        float top    = 24;
        float bottom = footer();
        f.body       = {12, top, width - 24 - Scrollbar, std::max(1.f, bottom - top)};
        int slots    = 0;
        for (auto const& r : page.rows)
            if (r.slot == Slot::Body) {
                slots            = std::max(slots, static_cast<int>(r.y) + 1);
                f.contentHeight += two ? 0 : RowGap;
            }
        if (two) f.contentHeight = slots * RowGap;
        finishScroll();
        float y = top - f.scroll;
        for (auto const& r : page.rows) {
            if (r.slot != Slot::Body) continue;
            UiRect rect{12, y, std::min(bw, f.body.width), ButtonHeight};
            if (two) rect = {12 + r.x * (bw + 20), top - f.scroll + r.y * RowGap, bw, ButtonHeight};
            else y += RowGap;
            Row copy    = r;
            copy.placed = false;
            control(copy, rect, false);
        }
    }
    void list() {
        float top     = topBar(22);
        float s       = search(12, top + 2, width * .4f);
        top          += s ? s + 4 : 0;
        float bottom  = footer();
        f.body        = {12, top, width - 24 - Scrollbar, std::max(1.f, bottom - top)};
        listBody();
    }
    void form() {
        placedRows();
        topBar(22);
        float bottom = footer();
        float x = page.listX < 0 ? width + page.listX : page.listX, y = page.listY;
        float w   = page.listWidth <= 0 ? width + page.listWidth - x : page.listWidth;
        float h   = page.listHeight <= 0 ? std::min(bottom, height + page.listHeight) - y : page.listHeight;
        bool  any = std::any_of(page.rows.begin(), page.rows.end(), [](Row const& r) {
            return !r.placed && r.slot == Slot::Body;
        });
        if (!any) {
            f.body = {x, y, std::max(1.f, w), 1};
            return;
        }
        float s  = search(x, y + 2, w * .5f);
        y       += s ? s + 4 : 0;
        h       -= s ? s + 4 : 0;
        f.body   = {x, y, std::max(1.f, w - Scrollbar), std::max(1.f, h)};
        listBody();
    }
    void browser() {
        float bottom = footer();
        float side   = page.sidePanel ? std::clamp(width * .36f, 150.f, 340.f) : 0;
        float top = 22, lx = 12, rx = width - 12;
        for (auto const& r : page.rows) {
            if (r.placed || r.kind == RowKind::IconButton || (r.slot != Slot::TopBar && r.slot != Slot::TopRight))
                continue;
            float w = autoWidth(r);
            if (r.slot == Slot::TopBar) {
                control(r, {lx, 22, w, ButtonHeight}, true);
                lx += w + 2;
            } else {
                rx -= w;
                control(r, {rx, 22, w, ButtonHeight}, true);
                rx -= 2;
            }
            top = 46;
        }
        UiRect panel{12, top, width - 24 - (side > 0 ? side + 6 : 0), std::max(40.f, bottom - top)};
        add("browser_panel", {}, {}, panel, WidgetKind::Panel);
        float x = panel.x + 4, y = panel.y + 4;
        for (auto const& r : page.rows) {
            if (r.placed || r.slot != Slot::TopBar || r.kind != RowKind::IconButton) continue;
            float w = autoWidth(r);
            control(r, {x, y, w, controlHeight(r)}, true);
            x += w + 4;
        }
        float pathWidth = panel.x + panel.width - 20 - x;
        if (state.searchOpen || !state.search.empty())
            add("search",
                {},
                state.search,
                {x, y - 2, std::max(10.f, pathWidth - 4), 16},
                WidgetKind::Text,
                UiOp::Search);
        else
            add("path", {}, page.path, {x + 2, y + 2, std::max(0.f, pathWidth - 4), 10}, WidgetKind::Label).color =
                colors::Gray;
        auto& icon =
            add("search_icon",
                {},
                {},
                {panel.x + panel.width - 16, y, 12, 12},
                WidgetKind::IconButton,
                UiOp::SearchToggle);
        icon.icon    = {icons::Search.u, icons::Search.v, 12, 12};
        icon.tooltip = tr("ui.layout.search");
        f.body       = {panel.x + 2, panel.y + 20, panel.width - 4 - Scrollbar, panel.height - 22};
        listBody(16);
        if (side <= 0) return;
        UiRect info{width - 12 - side, panel.y, side, panel.height};
        add("info_panel", {}, {}, info, WidgetKind::Panel);
        float iy = info.y + 4, iw = info.width - 8;
        int   n = 0;
        for (auto const& line : page.info) {
            auto  key = "info:" + std::to_string(n++);
            float lw  = measure(line.label);
            if (line.value.empty() || lw + measure(line.value) > iw) {
                add(key + ":label", {}, line.label, {info.x + 4, iy, iw, 10}, WidgetKind::Label).color  = colors::Gray;
                iy                                                                                     += 11;
                if (!line.value.empty()) {
                    auto lines = wrapLines(" " + line.value, iw, measure);
                    add(key, {}, " " + line.value, {info.x + 4, iy, iw, lines.size() * 10.f + 1}, WidgetKind::Label)
                        .color  = line.color ? line.color : colors::White;
                    iy         += lines.size() * 10.f + 1;
                }
            } else {
                add(key + ":label", {}, line.label, {info.x + 4, iy, lw + 2, 10}, WidgetKind::Label).color =
                    colors::Gray;
                add(key, {}, line.value, {info.x + 4 + lw, iy, iw - lw, 10}, WidgetKind::Label).color =
                    line.color ? line.color : colors::White;
                iy += 11;
            }
            if (iy > info.y + info.height - 10) break;
        }
    }
    void config() {
        float                        x = 12, top = 22;
        static constexpr char const* names[] = {
            "ui.layout.all",
            "ui.layout.generic",
            "ui.layout.info_hud",
            "ui.layout.visuals",
            "ui.layout.colors",
            "ui.layout.hotkeys",
            "ui.layout.render_layers"
        };
        int selected = id == PageId::KeyBindings ? 5 : id == PageId::Layers ? 6 : state.category;
        for (int i = 0; i < ConfigTabCount; ++i) {
            float w = std::min(width - 24, measure(tr(names[i])) + 12);
            if (x + w > width - 12) {
                x    = 12;
                top += 22;
            }
            auto& c =
                add("category:" + std::to_string(i),
                    {},
                    tr(names[i]),
                    {x, top, w, ButtonHeight},
                    WidgetKind::Button,
                    UiOp::Category,
                    i);
            c.enabled   = i != selected;
            c.centered  = true;
            x          += w + 2;
        }
        top          += 24;
        float s       = search(12, top + 2, width * .4f);
        top          += s + 4;
        float bottom  = footer();
        f.body        = {12, top, width - 24 - Scrollbar, std::max(1.f, bottom - top)};
        std::vector<Row const*> rows;
        float                   labelWidth = 60;
        for (auto const& r : page.rows)
            if (matches(r)) {
                rows.push_back(&r);
                if (r.kind != RowKind::Info && r.kind != RowKind::Label && r.kind != RowKind::Action)
                    labelWidth = std::max(labelWidth, measure(r.label) + 10);
            }
        labelWidth     = std::min(labelWidth, f.body.width * .45f);
        float resetW   = std::max(40.f, measure(tr("ui.layout.reset")) + 10);
        float controlW = std::clamp(f.body.width - labelWidth - resetW - 8, 40.f, 170.f);
        for (auto r : rows)
            f.contentHeight +=
                r->kind == RowKind::Info || r->kind == RowKind::Label ? infoHeight(*r, f.body.width) : RowGap;
        finishScroll();
        float y = top - f.scroll;
        for (auto r : rows) {
            if (r->kind == RowKind::Info || r->kind == RowKind::Label) {
                float h = infoHeight(*r, f.body.width);
                add(r->id,
                    r->id,
                    r->label,
                    {14, y + 2, f.body.width - 4, h - 2},
                    WidgetKind::Label,
                    UiOp::None,
                    0,
                    false)
                    .color  = r->textColor;
                y          += h;
                continue;
            }
            if (r->kind == RowKind::Action) {
                control(*r, {14, y, std::min(f.body.width - 4, autoWidth(*r)), ButtonHeight}, false);
                y += RowGap;
                continue;
            }
            float cx = 12 + labelWidth;
            auto& l =
                add(r->id + ":label",
                    r->id,
                    r->label,
                    {14, y + 6, labelWidth - 6, 10},
                    WidgetKind::Label,
                    UiOp::None,
                    0,
                    false);
            l.tooltip  = r->tooltip;
            Row copy   = *r;
            copy.label = {};
            if (r->kind == RowKind::Color) {
                add(r->id + ":value",
                    r->id,
                    r->value,
                    {cx, y, controlW - 24, ButtonHeight},
                    WidgetKind::Text,
                    UiOp::Submit,
                    0,
                    false);
                auto& sw =
                    add(r->id + ":swatch",
                        r->id,
                        {},
                        {cx + controlW - 20, y, 20, ButtonHeight},
                        WidgetKind::Swatch,
                        UiOp::None,
                        0,
                        false);
                auto to  = [](double v) { return static_cast<uint32_t>(std::clamp(v, 0., 1.) * 255 + .5); };
                sw.color = to(r->color[3]) << 24 | to(r->color[0]) << 16 | to(r->color[1]) << 8 | to(r->color[2]);
            } else if (r->kind == RowKind::KeyBinding) {
                auto& binding =
                    add(r->id + ":binding",
                        r->id,
                        r->value,
                        {cx, y, controlW - 22, ButtonHeight},
                        WidgetKind::Button,
                        UiOp::Submit,
                        0,
                        false);
                binding.centered = true;
                binding.color    = r->textColor;
                binding.tooltip  = r->tooltip;
                auto& settings =
                    add(r->id + ":settings",
                        r->id,
                        {},
                        {cx + controlW - 20, y, 20, ButtonHeight},
                        WidgetKind::IconButton,
                        UiOp::Activate,
                        0,
                        false);
                settings.parts   = r->parts;
                settings.tooltip = r->partsTooltip;
            } else if (r->kind == RowKind::Range) {
                add(r->id + ":first",
                    r->id,
                    r->value,
                    {cx, y, controlW / 2 - 2, ButtonHeight},
                    WidgetKind::Text,
                    UiOp::Submit,
                    0,
                    false);
                add(r->id + ":second",
                    r->id,
                    r->value2,
                    {cx + controlW / 2 + 2, y, controlW / 2 - 2, ButtonHeight},
                    WidgetKind::Text,
                    UiOp::Submit,
                    1,
                    false);
            } else control(copy, {cx, y, controlW, ButtonHeight}, false, r->id + ":control");
            if (r->reset) {
                add(r->id + ":reset",
                    r->id,
                    tr("ui.layout.reset"),
                    {cx + controlW + 4, y, resetW, ButtonHeight},
                    WidgetKind::Button,
                    UiOp::Reset,
                    0,
                    false)
                    .centered = true;
            }
            y += RowGap;
        }
    }
    void table() {
        placedRows();
        float  top    = topBar(22);
        float  bottom = footer();
        UiRect head{12, top, width - 24 - Scrollbar, 18};
        size_t columns = std::max<size_t>(1, page.header.size());
        float  actionW = 0;
        for (auto const& r : page.rows)
            if (matches(r))
                for (auto const& a : r.actions) actionW = std::max(actionW, autoWidth(a) + 4);
        float first = (head.width - actionW) * (columns > 1 ? .38f : 1.f);
        float other = columns > 1 ? (head.width - actionW - first) / (columns - 1) : 0;
        auto  cellX = [&](size_t i) { return head.x + (i == 0 ? 0 : first + (i - 1) * other); };
        auto  cellW = [&](size_t i) { return i == 0 ? first : other; };
        add("header_bg", {}, {}, {head.x, head.y, head.width + Scrollbar, head.height}, WidgetKind::Panel);
        for (size_t i = 0; i < page.header.size(); ++i) {
            auto& c =
                add("header:" + std::to_string(i),
                    {},
                    page.header[i],
                    {cellX(i) + 1, head.y + 1, cellW(i) - 2, head.height - 2},
                    WidgetKind::HeaderCell,
                    page.sort ? UiOp::Sort : UiOp::None,
                    static_cast<int>(i));
            c.selected = static_cast<int>(i) == page.sortColumn;
            if (c.selected) {
                auto arrow = page.sortDescending ? icons::ArrowDown : icons::ArrowUp;
                c.icon     = {arrow.u, arrow.v, arrow.w, arrow.h};
            }
        }
        search(head.x + head.width - 12, head.y + 3, 0);
        if (state.searchOpen || !state.search.empty())
            add("search",
                {},
                state.search,
                {head.x + head.width - 132, head.y + 1, 116, 16},
                WidgetKind::Text,
                UiOp::Search);
        f.body = {head.x, head.y + head.height + 2, head.width, std::max(1.f, bottom - head.y - head.height - 2)};
        std::vector<Row const*> rows;
        for (auto const& r : page.rows)
            if (matches(r)) rows.push_back(&r);
        for (auto r : rows) f.contentHeight += r->kind == RowKind::TableRow ? RowGap : infoHeight(*r, f.body.width);
        finishScroll();
        float y   = f.body.y - f.scroll;
        bool  odd = false;
        for (auto r : rows) {
            if (r->kind != RowKind::TableRow) {
                float h = infoHeight(*r, f.body.width);
                add(r->id,
                    r->id,
                    r->label,
                    {f.body.x + 2, y + 2, f.body.width - 4, h - 2},
                    WidgetKind::Label,
                    UiOp::None,
                    0,
                    false)
                    .color  = r->textColor;
                y          += h;
                continue;
            }
            auto& bg =
                add(r->id,
                    r->id,
                    {},
                    {f.body.x, y, f.body.width, RowGap - 1},
                    WidgetKind::ListRow,
                    UiOp::Submit,
                    odd ? 1 : 0,
                    false);
            bg.tooltip  = r->tooltip;
            bg.selected = r->selected;
            odd         = !odd;
            float iconW = r->item.empty() ? 0 : 18;
            if (!r->item.empty()) {
                auto& icon =
                    add(r->id + ":item",
                        r->id,
                        {},
                        {cellX(0) + 3, y + 2, 16, 16},
                        WidgetKind::ItemIcon,
                        UiOp::None,
                        r->itemAux,
                        false);
                icon.texture = r->item;
            }
            for (size_t i = 0; i < r->columns.size() && i < columns; ++i)
                add(r->id + ":cell" + std::to_string(i),
                    r->id,
                    r->columns[i],
                    {cellX(i) + 4 + (i == 0 ? iconW : 0), y + 6, cellW(i) - 6 - (i == 0 ? iconW : 0), 10},
                    WidgetKind::Label,
                    UiOp::None,
                    0,
                    false)
                    .color = i < r->columnColors.size() ? r->columnColors[i] : r->textColor;
            float rx = f.body.x + f.body.width - 2;
            for (auto it = r->actions.rbegin(); it != r->actions.rend(); ++it) {
                float bw  = autoWidth(*it);
                rx       -= bw;
                control(*it, {rx, y + 1, bw, ButtonHeight - 1}, false);
                rx -= 2;
            }
            y += RowGap;
        }
    }
    void modal() {
        float  panelWidth  = std::min(460.f, width - 24);
        float  lines       = std::max(1.f, std::ceil(measure(page.message) / (panelWidth - 16)))
                           + static_cast<float>(std::count(page.message.begin(), page.message.end(), '\n'));
        float  panelHeight = std::min(height - 24, std::max(120.f, 42 + lines * 12 + page.rows.size() * 22));
        UiRect panel{(width - panelWidth) / 2, (height - panelHeight) / 2, panelWidth, panelHeight};
        f.body =
            {panel.x + 8, panel.y + 26, panel.width - 16, std::max(12.f, panel.height - 36 - page.rows.size() * 22)};
        f.contentHeight = lines * 12;
        finishScroll();
        add("dialog", {}, {}, panel, WidgetKind::Panel);
        add("title",
            {},
            page.modalTitle.empty() ? tr("ui.layout.confirmation") : page.modalTitle,
            {panel.x + 8, panel.y + 8, panel.width - 16, 14},
            WidgetKind::Label);
        add("message",
            {},
            page.message,
            {f.body.x, f.body.y - f.scroll, f.body.width, std::max(f.body.height, f.contentHeight)},
            WidgetKind::Label,
            UiOp::None,
            0,
            false);
        float y = panel.y + panel.height - page.rows.size() * 22 - 6;
        for (auto const& r : page.rows) {
            if (r.kind == RowKind::Toggle || r.kind == RowKind::Choice) {
                float bw = std::min(160.f, panel.width * .4f);
                auto& l =
                    add(r.id + ":label",
                        r.id,
                        r.label,
                        {panel.x + 10, y + 6, panel.width - bw - 24, 10},
                        WidgetKind::Label);
                l.tooltip = r.tooltip;
                Row copy  = r;
                copy.label.clear();
                control(copy, {panel.x + panel.width - 8 - bw, y, bw, 20}, true);
                y += 22;
                continue;
            }
            if (r.kind == RowKind::Text)
                add(r.id + ":value",
                    r.id,
                    r.value,
                    {panel.x + 8, y, panel.width - 16, 20},
                    WidgetKind::Text,
                    UiOp::Submit);
            else
                add(r.id == "cancel" ? "back" : r.id,
                    r.id,
                    r.label,
                    {panel.x + 8, y, panel.width - 16, 20},
                    WidgetKind::Button,
                    UiOp::Submit)
                    .centered = true;
            y += 22;
        }
    }
};
} // namespace

UiFrame
layoutPage(Page const& page, PageId id, UiViewState const& state, float width, float height, TextWidth const& measure) {
    Builder b{
        page,
        id,
        state,
        width,
        height,
        measure,
        {},
        {0, 0, width, height}
    };
    b.f.width  = width;
    b.f.height = height;
    if (!std::isfinite(width) || !std::isfinite(height) || width < 96 || height < 96) return b.f;
    if (page.dismiss) {
        b.modal();
        return std::move(b.f);
    }
    b.title();
    switch (page.layout) {
    case PageLayout::Menu:
        b.menu();
        break;
    case PageLayout::Browser:
        b.browser();
        break;
    case PageLayout::Form:
        b.form();
        break;
    case PageLayout::Config:
        b.config();
        break;
    case PageLayout::Table:
        b.table();
        break;
    case PageLayout::List:
        b.list();
        break;
    }
    if (page.layout != PageLayout::Form && page.layout != PageLayout::Table) b.placedRows();
    b.scrollbar();
    return std::move(b.f);
}
nlohmann::ordered_json
frameJson(UiFrame const& f, PageId page, uint64_t frame, std::string const& language, float scale) {
    using J      = nlohmann::ordered_json;
    auto rect    = [](UiRect r) { return J::array({r.x, r.y, r.width, r.height}); };
    J    widgets = J::array();
    for (auto const& c : f.widgets) {
        auto visible = intersect(c.rect, c.clip);
        widgets.push_back({
            {"id",         c.id                                   },
            {"row",        c.row                                  },
            {"kind",       static_cast<int>(c.kind)               },
            {"rect",       rect(c.rect)                           },
            {"clip",       rect(c.clip)                           },
            {"visible",    visible.width > 0 && visible.height > 0},
            {"text",       c.text + c.suffix                      },
            {"textBounds", rect(c.textBounds)                     },
            {"selected",   c.selected                             },
            {"hovered",    c.hovered                              },
            {"enabled",    c.enabled                              },
            {"texture",    c.texture                              },
            {"iconUV",     rect(c.icon)                           },
            {"tooltip",    c.tooltip                              }
        });
    }
    return {
        {"frame",     frame                 },
        {"page",      static_cast<int>(page)},
        {"language",  language              },
        {"guiScale",  scale                 },
        {"viewport",  {f.width, f.height}   },
        {"body",      rect(f.body)          },
        {"scroll",    f.scroll              },
        {"maxScroll", f.maxScroll           },
        {"widgets",   std::move(widgets)    }
    };
}
} // namespace levischematic::ui
