#include "levischematic/presentation/ui/model/Ui.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace levischematic::ui {
void clearPage(Page& page) noexcept {
    page.rows.clear();
    page.rawKey        = {};
    page.captureEscape = {};
    page.refresh       = {};
    page.save          = {};
    page.dismiss       = {};
}
void dismissModal(std::shared_ptr<Page> const& page) {
    auto dismiss = page->dismiss;
    if (dismiss) dismiss();
}
void beginModal(std::shared_ptr<Page> const& page, std::string message) {
    dismissModal(page);
    auto rows     = std::make_shared<std::vector<Row>>(std::move(page->rows));
    auto previous = std::move(page->message);
    auto refresh  = std::move(page->refresh);
    page->dismiss = [weak = std::weak_ptr<Page>(page), rows, previous, refresh] {
        if (auto p = weak.lock()) {
            p->rows    = std::move(*rows);
            p->message = previous;
            p->refresh = refresh;
            p->dismiss = {};
            p->modalTitle.clear();
        }
    };
    page->rows.clear();
    page->message = std::move(message);
}
Row const* findRow(Page const& page, std::string_view id) {
    for (auto const& row : page.rows)
        if (row.id == id) return &row;
    return nullptr;
}
PageBuilder& PageBuilder::button(std::string id, std::string label, std::function<void()> action) {
    mPage->rows.push_back({std::move(id), std::move(label), {}, false, [action = std::move(action)](std::string) {
                               action();
                           }});
    return *this;
}
PageBuilder&
PageBuilder::input(std::string id, std::string label, std::string value, std::function<void(std::string)> action) {
    mPage->rows.push_back({std::move(id), std::move(label), std::move(value), true, std::move(action), RowKind::Text});
    return *this;
}
PageBuilder& PageBuilder::info(std::string id, std::string text) {
    button(std::move(id), std::move(text), [] {});
    mPage->rows.back().kind = RowKind::Info;
    return *this;
}
Row&         PageBuilder::last() { return mPage->rows.back(); }
PageBuilder& PageBuilder::at(float x, float y, float width, float height) {
    auto& r  = last();
    r.placed = true;
    r.x      = x;
    r.y      = y;
    r.width  = width;
    r.height = height;
    return *this;
}
PageBuilder& PageBuilder::in(Slot slot) {
    last().slot = slot;
    return *this;
}
PageBuilder& PageBuilder::tip(std::string text) {
    last().tooltip = std::move(text);
    return *this;
}
PageBuilder& PageBuilder::icon(IconRef value) {
    last().icon = value;
    return *this;
}
PageBuilder& PageBuilder::color(uint32_t value) {
    last().textColor = value;
    return *this;
}
PageBuilder& PageBuilder::kind(RowKind value) {
    last().kind = value;
    return *this;
}
PageBuilder& PageBuilder::right(std::function<void()> action) {
    last().secondary = std::move(action);
    return *this;
}
PageBuilder& PageBuilder::enabled(bool value) {
    last().enabled = value;
    return *this;
}
PageBuilder& PageBuilder::checkbox(std::string id, std::string text, bool checked, std::function<void()> toggle) {
    button(std::move(id), std::move(text), std::move(toggle));
    last().kind    = RowKind::Checkbox;
    last().checked = checked;
    return *this;
}
PageBuilder& PageBuilder::toggle(std::string id, std::string text, bool value, std::function<void()> action) {
    button(std::move(id), std::move(text), std::move(action));
    last().kind  = RowKind::Toggle;
    last().value = value ? "true" : "false";
    return *this;
}
PageBuilder& PageBuilder::label(std::string id, std::string text) {
    info(std::move(id), std::move(text));
    last().kind = RowKind::Label;
    return *this;
}
PageBuilder& PageBuilder::adjuster(std::string id, int mode, std::function<void(int)> action) {
    button(std::move(id), {}, [] {});
    last().kind       = RowKind::Adjust;
    last().adjust     = std::move(action);
    last().adjustMode = mode;
    last().icon       = icons::PlusMinus;
    return *this;
}
int adjustAmount(int mode, Modifiers m) {
    int amount = 1;
    if (mode == 1) {
        if (m.ctrl) amount *= 100;
        if (m.shift) amount *= 10;
        if (m.alt) amount *= 5;
    } else {
        if (m.shift) amount *= 8;
        if (m.alt) amount *= 4;
    }
    return amount;
}
RowSection rowSection(Row const& row) {
    if (row.kind == RowKind::File) return RowSection::File;
    if (row.kind == RowKind::Info) return RowSection::Info;
    if (row.kind == RowKind::Color) return RowSection::Color;
    if (row.kind == RowKind::ActionBar) return RowSection::Footer;
    return RowSection::Main;
}
Row const* sectionRow(Page const& page, RowSection section, int index) {
    if (index < 0) return nullptr;
    for (auto const& row : page.rows)
        if (rowSection(row) == section && index-- == 0) return &row;
    return nullptr;
}
int sectionSize(Page const& page, RowSection section) {
    return static_cast<int>(std::count_if(page.rows.begin(), page.rows.end(), [section](auto const& r) {
        return rowSection(r) == section;
    }));
}
std::string infoText(Page const& page) {
    std::string result;
    for (auto const& row : page.rows) {
        if (row.kind != RowKind::Info) continue;
        if (!result.empty()) result += '\n';
        result += row.label;
    }
    return result;
}
void moveActionsToFooter(Page& page, std::vector<std::string> const& ids) {
    Row bar;
    bar.id     = "footer";
    bar.kind   = RowKind::ActionBar;
    bar.submit = [](std::string) {};
    for (auto const& id : ids) {
        auto it = std::find_if(page.rows.begin(), page.rows.end(), [&](auto const& r) { return r.id == id; });
        if (it != page.rows.end()) {
            bar.actions.push_back(std::move(*it));
            page.rows.erase(it);
        }
    }
    if (!bar.actions.empty()) page.rows.push_back(std::move(bar));
}
double sliderNativeValue(double value, double minimum, double maximum, double step) {
    (void)step;
    return (std::clamp(value, minimum, maximum) - minimum) / (maximum - minimum);
}
double sliderModelValue(double native, double minimum, double maximum, double step) {
    return std::clamp(
        minimum + std::round(std::clamp(native, 0., 1.) * (maximum - minimum) / step) * step,
        minimum,
        maximum
    );
}
std::string formatColor(std::array<double, 4> const& color) {
    return std::to_string(static_cast<int>(std::round(color[0] * 255))) + ", "
         + std::to_string(static_cast<int>(std::round(color[1] * 255))) + ", "
         + std::to_string(static_cast<int>(std::round(color[2] * 255))) + ", "
         + std::to_string(static_cast<int>(std::round(color[3] * 100))) + "%";
}
std::optional<std::array<double, 4>> parseColor(std::string text) {
    for (auto& c : text)
        if (c == ',') c = ' ';
    std::istringstream    input(text);
    std::array<double, 4> result;
    for (int c = 0; c < 4; ++c) {
        double value;
        if (!(input >> value) || !std::isfinite(value) || value < 0 || value > (c == 3 ? 100 : 255)
            || (c != 3 && value != std::round(value)))
            return {};
        result[c] = value / (c == 3 ? 100 : 255);
    }
    input >> std::ws;
    if (input.peek() == '%') {
        input.get();
        input >> std::ws;
    }
    if (!input.eof()) return {};
    return result;
}
std::string formatHexColor(std::array<double, 4> const& color) {
    auto byte = [](double v) { return static_cast<unsigned>(std::lround(std::clamp(v, 0., 1.) * 255)); };
    char text[16];
    std::snprintf(
        text,
        sizeof(text),
        "#%02X%02X%02X%02X",
        byte(color[3]),
        byte(color[0]),
        byte(color[1]),
        byte(color[2])
    );
    return text;
}
std::optional<std::array<double, 4>> parseHexColor(std::string text) {
    while (!text.empty() && text.front() == ' ') text.erase(text.begin());
    while (!text.empty() && text.back() == ' ') text.pop_back();
    if (!text.empty() && text.front() == '#') text.erase(text.begin());
    else if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text.erase(0, 2);
    if (text.size() == 6) text = "FF" + text;
    if (text.size() != 8) return parseColor(text);
    unsigned value    = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (error != std::errc{} || end != text.data() + text.size()) return {};
    return std::array<double, 4>{
        ((value >> 16) & 255) / 255.,
        ((value >> 8) & 255) / 255.,
        (value & 255) / 255.,
        ((value >> 24) & 255) / 255.
    };
}
int clampLayerInput(std::string_view text, int previous, int minimum, int maximum) {
    long long value;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error == std::errc::result_out_of_range) {
        bool negative = !text.empty() && text.front() == '-';
        auto digits   = text.substr(negative ? 1 : 0);
        if (!digits.empty() && std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; }))
            return negative ? minimum : maximum;
    }
    if (error != std::errc{} || end != text.data() + text.size()) return std::clamp(previous, minimum, maximum);
    return static_cast<int>(std::clamp(value, static_cast<long long>(minimum), static_cast<long long>(maximum)));
}
std::string PageBuilder::validate(Page const& page) {
    if (page.title.empty()) return "Page title is empty.";
    std::unordered_set<std::string> ids;
    for (auto const& row : page.rows) {
        if (row.id.empty() || !ids.insert(row.id).second) return "Empty or duplicate control ID.";
        if (!row.submit) return "Missing action: " + row.id;
        if (row.kind == RowKind::Slider
            && (!std::isfinite(row.minimum) || !std::isfinite(row.maximum) || !std::isfinite(row.step)
                || row.minimum >= row.maximum || row.step <= 0))
            return "Invalid slider range: " + row.id;
        if (row.kind == RowKind::Color
            && (!row.colorSubmit || std::any_of(row.color.begin(), row.color.end(), [](double v) {
                   return !std::isfinite(v) || v < 0 || v > 1;
               })))
            return "Invalid color: " + row.id;
        if (row.kind == RowKind::ActionBar && (row.actions.empty() || row.actions.size() > 3))
            return "Invalid action bar: " + row.id;
        if (row.kind == RowKind::Range && !row.submit2) return "Missing range callback: " + row.id;
        if (row.kind == RowKind::File && !row.activate) return "Missing file action: " + row.id;
        if (row.kind == RowKind::KeyBinding && (!row.reset || !row.clear)) return "Missing key action: " + row.id;
        if (row.kind == RowKind::Choice && (row.choices.empty() || row.choices.size() > 8))
            return "Invalid choice count: " + row.id;
    }
    return {};
}
} // namespace levischematic::ui
