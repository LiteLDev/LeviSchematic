#include "Ui.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace levischematic::ui {
void clearPage(Page& page) noexcept {
    page.rows.clear();
    page.rawKey = {};
    page.refresh = {};
    page.save = {};
    page.dismiss = {};
}
void dismissModal(std::shared_ptr<Page> const& page) {
    auto dismiss = page->dismiss;
    if (dismiss) dismiss();
}
void beginModal(std::shared_ptr<Page> const& page, std::string message) {
    dismissModal(page);
    auto rows = std::make_shared<std::vector<Row>>(std::move(page->rows));
    auto previous = std::move(page->message);
    auto refresh = std::move(page->refresh);
    page->dismiss = [weak = std::weak_ptr<Page>(page), rows, previous, refresh] {
        if (auto p = weak.lock()) {
            p->rows = std::move(*rows); p->message = previous; p->refresh = refresh; p->dismiss = {};
        }
    };
    page->rows.clear(); page->message = std::move(message);
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
        if (row.kind == RowKind::Choice && (row.choices.empty() || row.choices.size() > 4))
            return "Invalid choice count: " + row.id;
    }
    return {};
}
} 
