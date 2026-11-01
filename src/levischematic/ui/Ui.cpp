#include "Ui.h"
#include <unordered_set>

namespace levischematic::ui {
void PageTabs::clear() noexcept {
    for (auto& p : pages)
        if (p) {
            p->rows.clear();
            p->rawKey  = {};
            p->refresh = {};
            p.reset();
        }
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
    mPage->rows.push_back({std::move(id), std::move(label), std::move(value), true, std::move(action)});
    return *this;
}
std::string PageBuilder::validate(Page const& page) {
    if (page.title.empty()) return "Page title is empty.";
    std::unordered_set<std::string> ids;
    for (auto const& row : page.rows) {
        if (row.id.empty() || !ids.insert(row.id).second) return "Empty or duplicate control ID.";
        if (!row.submit) return "Missing action: " + row.id;
    }
    return {};
}
} 
