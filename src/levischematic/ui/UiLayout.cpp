#include "UiLayout.h"
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
    if (p.find("Color") != p.npos) return 3;
    if (p.starts_with("/hud/") || p == "hud_position" || p == "hud_layout" || p.find("Hud") != p.npos) return 1;
    if (p.starts_with("/projection/") || p.starts_with("/hull/") || p.starts_with("/visibility/")
        || p.starts_with("/selection/"))
        return 2;
    return 0;
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
        float y = bounds.y + i * 12;
        
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
UiFrame
layoutPage(Page const& page, PageId id, UiViewState const& state, float width, float height, TextWidth const& measure) {
    UiFrame f;
    f.width  = width;
    f.height = height;
    if (!std::isfinite(width) || !std::isfinite(height) || width < 96 || height < 96) return f;
    UiRect screen{0, 0, width, height};
    auto   add = [&](std::string key,
                     std::string row,
                     std::string text,
                     UiRect      r,
                     WidgetKind  kind,
                     UiOp        op    = UiOp::None,
                     int         index = 0,
                     bool        fixed = false) -> Widget& {
        f.widgets.push_back(
            {row.empty() || (page.dismiss && key == "back") ? key : "row:" + key,
             std::move(row),
             std::move(text),
             r,
             fixed ? screen : f.body,
             kind,
             op,
             index}
        );
        return f.widgets.back();
    };
    if (page.dismiss) {
        float  panelWidth  = std::min(460.f, width - 24);
        float  lines       = std::max(1.f, std::ceil(measure(page.message) / (panelWidth - 16)))
                           + static_cast<float>(std::count(page.message.begin(), page.message.end(), '\n'));
        float  panelHeight = std::min(height - 24, std::max(120.f, 42 + lines * 12 + page.rows.size() * 22));
        UiRect panel{(width - panelWidth) / 2, (height - panelHeight) / 2, panelWidth, panelHeight};
        f.body =
            {panel.x + 8, panel.y + 26, panel.width - 16, std::max(12.f, panel.height - 36 - page.rows.size() * 22)};
        f.contentHeight = lines * 12;
        f.maxScroll     = std::max(0.f, f.contentHeight - f.body.height);
        f.scroll        = std::clamp(state.scroll, 0.f, f.maxScroll);
        add("dialog", {}, {}, panel, WidgetKind::Panel, UiOp::None, 0, true);
        add("title",
            {},
            "确认 / Confirmation",
            {panel.x + 8, panel.y + 8, panel.width - 16, 14},
            WidgetKind::Label,
            UiOp::None,
            0,
            true);
        add("message",
            {},
            page.message,
            {f.body.x, f.body.y - f.scroll, f.body.width, std::max(f.body.height, f.contentHeight)},
            WidgetKind::Label);
        float y = panel.y + panel.height - page.rows.size() * 22 - 6;
        for (auto const& r : page.rows) {
            add(r.id == "cancel" ? "back" : r.id,
                r.id,
                r.label,
                {panel.x + 8, y, panel.width - 16, 20},
                WidgetKind::Button,
                UiOp::Submit,
                0,
                true);
            y += 22;
        }
        return f;
    }
    add("title", {}, page.title, {12, 9, width - 24, 14}, WidgetKind::Label, UiOp::None, 0, true);
    add("back",
        {},
        "Back",
        {12, height - 26, std::min(100.f, width - 24), 20},
        WidgetKind::Button,
        UiOp::Back,
        0,
        true);
    add("message",
        {},
        page.message,
        id == PageId::MainMenu ? UiRect{12, height - 42, width - 24, 12}
                               : UiRect{120, height - 24, std::max(0.f, width - 132), 18},
        WidgetKind::Label,
        UiOp::None,
        0,
        true);
    bool  config = id == PageId::Settings || id == PageId::KeyBindings || id == PageId::Layers;
    float top    = 30;
    if (config) {
        for (int i = 0; i < 6; ++i) {
            static constexpr char const* names[] =
                {"Generic", "Info overlays", "Visuals", "Colors", "Hotkeys", "Render layers"};
            float w = std::min(width - 24, std::max(56.f, measure(names[i]) + 12));
            float x = 12;
            if (i > 0) {
                auto const& prev = f.widgets.back();
                x                = prev.rect.x + prev.rect.width + 2;
                if (x + w > width - 12) {
                    x    = 12;
                    top += 22;
                }
            }
            auto& c =
                add("category:" + std::to_string(i),
                    {},
                    names[i],
                    {x, top, w, 20},
                    WidgetKind::Button,
                    UiOp::Category,
                    i,
                    true);
            c.selected = id == PageId::KeyBindings ? i == 4 : id == PageId::Layers ? i == 5 : i == state.category;
        }
        top += 24;
    }
    if (id != PageId::MainMenu) {
        auto& search =
            add("search", {}, state.search, {12, top, width - 24, 18}, WidgetKind::Text, UiOp::Search, 0, true);
        search.icon     = {201, 0, 12, 12};
        search.texture  = "malilib/gui_widgets.png";
        top            += 22;
    }
    f.body       = {12, top, width - 30, std::max(1.f, height - top - (id == PageId::MainMenu ? 48 : 32))};
    auto matches = [&](Row const& r) {
        if (id == PageId::Settings && r.kind != RowKind::ActionBar && configCategory(r.id) != state.category)
            return false;
        auto text = r.label + " " + r.value + " " + r.id;
        for (auto const& a : r.actions) text += " " + a.label;
        auto query = state.search;
        auto lower = [](std::string& s) {
            for (auto& c : s)
                if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        };
        lower(text);
        lower(query);
        return query.empty() || text.find(query) != std::string::npos;
    };
    float buttonWidth = 160;
    if (id == PageId::MainMenu)
        for (auto const& r : page.rows) buttonWidth = std::max(buttonWidth, measure(r.label) + 30);
    bool columns  = id == PageId::MainMenu && width >= 24 + buttonWidth * 2 + 20;
    auto mainSlot = [](Row const& row) -> std::pair<int, int> {
        auto is = [&](PageId route) { return row.id == "navigate" + std::to_string(static_cast<int>(route)); };
        if (is(PageId::Projection)) return {0, 0};
        if (is(PageId::Resources)) return {0, 1};
        if (is(PageId::Load)) return {0, 2};
        if (is(PageId::Selection)) return {0, 4};
        if (is(PageId::SaveSelection)) return {0, 5};
        if (is(PageId::Placement)) return {0, 6};
        if (is(PageId::Settings)) return {1, 0};
        if (is(PageId::Tasks)) return {1, 5};
        if (is(PageId::Projects)) return {1, 6};
        if (is(PageId::Layers)) return {0, 8};
        if (is(PageId::Verifier)) return {1, 8};
        if (is(PageId::Materials)) return {0, 9};
        if (is(PageId::Survival)) return {1, 9};
        if (is(PageId::Session)) return {0, 10};
        if (is(PageId::Edit)) return {1, 10};
        if (is(PageId::WorldOperations)) return {0, 11};
        if (is(PageId::Signs)) return {1, 11};
        if (is(PageId::KeyBindings)) return {1, 12};
        return {0, 12};
    };
    auto rowHeight = [&](Row const& r) {
        if (r.kind == RowKind::Entry)
            return 48.f + std::ceil(r.actions.size() / std::max(1.f, std::floor(f.body.width / 100))) * 22;
        if (r.kind == RowKind::Info) {
            float       lines = 1;
            std::string line;
            for (char c : r.label) {
                if (c == '\n') {
                    lines += std::max(1.f, std::ceil(measure(line) / f.body.width));
                    line.clear();
                } else line += c;
            }
            lines += std::max(1.f, std::ceil(measure(line) / f.body.width)) - 1;
            return std::max(22.f, lines * 12 + 4);
        }
        return r.kind == RowKind::Color ? 84.f : 22.f;
    };
    for (auto const& r : page.rows)
        if (matches(r)) f.contentHeight += rowHeight(r);
    if (columns) f.contentHeight = 13 * 22;
    f.maxScroll = std::max(0.f, f.contentHeight - f.body.height);
    f.scroll    = std::clamp(std::isfinite(state.scroll) ? state.scroll : 0.f, 0.f, f.maxScroll);
    float y     = top - f.scroll;
    for (auto const& r : page.rows) {
        if (!matches(r)) continue;
        if (id == PageId::MainMenu && r.id == "tool_mode") continue;
        float h = rowHeight(r), x = 12, w = f.body.width;
        if (id == PageId::MainMenu) {
            if (columns) {
                auto [col, slot]  = mainSlot(r);
                x                += col * (buttonWidth + 20);
                y                 = top - f.scroll + slot * 22;
            }
            w = std::min(buttonWidth, f.body.width);
        }
        if (r.kind == RowKind::Info) add(r.id, r.id, r.label, {x, y, w, h - 2}, WidgetKind::Label);
        else if (r.kind == RowKind::TableRow) {
            auto& entry    = add(r.id, r.id, {}, {x, y, w, 20}, WidgetKind::ListRow, UiOp::Submit);
            entry.selected = r.selected;
            auto  columns  = r.columns.empty() ? std::vector{r.label} : r.columns;
            float cx       = x + 4;
            for (size_t i = 0; i < columns.size(); ++i) {
                float cw = columns.size() == 1 ? w - 8 : i == 0 ? w * .55f : w * .45f / (columns.size() - 1);
                add(r.id + ":cell" + std::to_string(i), r.id, columns[i], {cx, y + 5, cw - 4, 14}, WidgetKind::Label);
                cx += cw;
            }
        } else if (r.kind == RowKind::Entry) {
            auto& entry    = add(r.id, r.id, r.label, {x, y, w, 20}, WidgetKind::Button, UiOp::Submit);
            entry.selected = r.selected;
            add(r.id + ":detail", r.id, r.value, {x + 4, y + 24, w - 8, 20}, WidgetKind::Label);
            float bx = x, by = y + 46;
            int   cols = std::max(1, static_cast<int>(w / 100));
            float bw   = (w - (cols - 1) * 2) / cols;
            for (size_t i = 0; i < r.actions.size(); ++i) {
                auto const& a = r.actions[i];
                add(a.id, a.id, a.label, {bx, by, bw, 20}, WidgetKind::Button, UiOp::Submit);
                bx += bw + 2;
                if ((i + 1) % cols == 0) {
                    bx  = x;
                    by += 22;
                }
            }
        } else if (r.kind == RowKind::ActionBar) {
            float bx = x;
            for (auto const& action : r.actions) {
                float bw = std::min((w - 4) / r.actions.size(), std::max(60.f, measure(action.label) + 12));
                add(action.id, action.id, action.label, {bx, y, bw, 20}, WidgetKind::Button, UiOp::Submit);
                bx += bw + 2;
            }
        } else if (r.kind == RowKind::Action) {
            auto& c    = add(r.id, r.id, r.label, {x, y, w, 20}, WidgetKind::Button, UiOp::Submit);
            c.selected = r.selected;
            c.enabled  = r.enabled;
            if (id == PageId::MainMenu) {
                auto is = [&](PageId route) { return r.id == "navigate" + std::to_string(static_cast<int>(route)); };
                int  v  = -1;
                if (is(PageId::Projection) || is(PageId::Placement)) v = 42;
                else if (is(PageId::Settings) || is(PageId::KeyBindings)) v = 84;
                else if (is(PageId::Selection)) v = 70;
                else if (is(PageId::Resources)) v = 14;
                else if (is(PageId::Load)) v = 28;
                else if (is(PageId::Projects)) v = 98;
                else if (is(PageId::Tasks)) v = 112;
                if (v >= 0) c.icon = {116, static_cast<float>(v), 14, 14};
            }
            if (r.id.starts_with("file:")) c.icon = {144, 36, 12, 12};
        } else {
            float fieldWidth = w < 260 ? w * .48f : std::min(220.f, w * .48f);
            float fx         = x + w - fieldWidth;
            if (id == PageId::Settings && r.reset) {
                add(r.id + ":reset", r.id, "R", {x + w - 20, y, 20, 20}, WidgetKind::Button, UiOp::Reset);
                fieldWidth -= 24;
            }
            add(r.id + ":label", r.id, r.label, {x + 2, y + 5, w - fieldWidth - 8, 14}, WidgetKind::Label);
            if (r.kind == RowKind::Range) {
                add(r.id + ":first", r.id, r.value, {fx, y, fieldWidth / 2 - 3, 20}, WidgetKind::Text, UiOp::Submit);
                add(r.id + ":second",
                    r.id,
                    r.value2,
                    {fx + fieldWidth / 2 + 3, y, fieldWidth / 2 - 3, 20},
                    WidgetKind::Text,
                    UiOp::Submit,
                    1);
            } else if (r.kind == RowKind::KeyBinding) {
                add(r.id + ":binding", r.id, r.value, {fx, y, fieldWidth - 44, 20}, WidgetKind::Button, UiOp::Submit);
                add(r.id + ":reset", r.id, "R", {x + w - 42, y, 20, 20}, WidgetKind::Button, UiOp::Reset);
                add(r.id + ":clear", r.id, "X", {x + w - 20, y, 20, 20}, WidgetKind::Button, UiOp::Clear);
            } else if (r.kind == RowKind::Toggle || r.kind == RowKind::Choice) {
                std::string text = r.value;
                if (r.kind == RowKind::Toggle) text = r.value == "true" ? "On / 已开启" : "Off / 已关闭";
                if (r.kind == RowKind::Choice) {
                    int i = 0;
                    try {
                        i = std::stoi(r.value);
                    } catch (...) {}
                    if (i >= 0 && i < static_cast<int>(r.choices.size())) text = r.choices[i];
                }
                add(r.id + ":value",
                    r.id,
                    text,
                    {fx, y, fieldWidth, 20},
                    WidgetKind::Button,
                    r.kind == RowKind::Choice ? UiOp::Choice : UiOp::Submit);
            } else if (r.kind == RowKind::Slider) {
                auto& c =
                    add(r.id + ":slider", r.id, r.value, {fx, y, fieldWidth, 20}, WidgetKind::Slider, UiOp::Submit);
                try {
                    c.value = sliderNativeValue(std::stod(r.value), r.minimum, r.maximum, r.step);
                } catch (...) {}
            } else {
                float fw = r.kind == RowKind::File ? fieldWidth - 44 : fieldWidth;
                add(r.id + ":value", r.id, r.value, {fx, y, fw, 20}, WidgetKind::Text, UiOp::Submit);
                if (r.kind == RowKind::File)
                    add(r.id + ":load", r.id, "Load", {fx + fw + 2, y, 42, 20}, WidgetKind::Button, UiOp::Activate);
                if (r.kind == RowKind::Color)
                    for (int channel = 0; channel < 4; ++channel) {
                        auto& c =
                            add(r.id + ":channel" + std::to_string(channel),
                                r.id,
                                std::string(1, "RGBA"[channel]),
                                {fx, y + 23 + channel * 14, fieldWidth, 12},
                                WidgetKind::Slider,
                                UiOp::Color,
                                channel);
                        c.value = r.color[channel];
                    }
            }
        }
        if (!columns) y += h;
    }
    if (id == PageId::MainMenu) {
        if (auto mode = findRow(page, "tool_mode"))
            add(mode->id,
                mode->id,
                mode->label,
                {118, height - 26, std::max(1.f, width - 130), 20},
                WidgetKind::Button,
                UiOp::Submit,
                0,
                true);
    }
    if (f.maxScroll > 0) {
        float thumb = std::max(12.f, f.body.height * f.body.height / f.contentHeight);
        auto& c =
            add("scrollbar",
                {},
                {},
                {width - 14, top + f.scroll / f.maxScroll * (f.body.height - thumb), 6, thumb},
                WidgetKind::Scrollbar,
                UiOp::Scroll,
                0,
                true);
        c.value = f.scroll / f.maxScroll;
    }
    return f;
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
            {"text",       c.text                                 },
            {"textBounds", rect(c.textBounds)                     },
            {"selected",   c.selected                             },
            {"hovered",    c.hovered                              },
            {"enabled",    c.enabled                              },
            {"texture",    c.texture                              },
            {"iconUV",     rect(c.icon)                           }
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
} 
