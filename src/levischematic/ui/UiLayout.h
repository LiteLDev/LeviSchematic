#pragma once
#include "Ui.h"
#include <functional>
#include <nlohmann/json.hpp>

namespace levischematic::ui {
struct UiRect {
    float x = 0, y = 0, width = 0, height = 0;
    bool  contains(float px, float py) const;
};
UiRect intersect(UiRect a, UiRect b);
enum class WidgetKind { Label, Button, Text, Slider, Swatch, Scrollbar, ListRow, Panel };
enum class UiOp { None, Submit, Activate, Reset, Clear, Choice, Color, Back, Category, Search, Scroll };
struct Widget {
    std::string id, row, text;
    UiRect      rect, clip;
    WidgetKind  kind     = WidgetKind::Label;
    UiOp        op       = UiOp::None;
    int         index    = 0;
    double      value    = 0;
    bool        selected = false;
    UiRect      icon; 
    bool        enabled = true, hovered = false;
    UiRect      textBounds;
    std::string texture;
};
struct UiViewState {
    float       scroll = 0;
    std::string search;
    int         category = 0;
};
struct UiFrame {
    float               width = 0, height = 0, contentHeight = 0, maxScroll = 0, scroll = 0;
    UiRect              body;
    std::vector<Widget> widgets;
    Widget const*       hit(float x, float y) const;
};
using TextWidth = std::function<float(std::string const&)>;
struct UiTextRun {
    std::string text;
    UiRect      rect;
};
std::vector<UiTextRun>
layoutTextRuns(std::string const&, UiRect bounds, UiRect clip, TextWidth const&, bool center = false);

UiFrame                layoutPage(Page const&, PageId, UiViewState const&, float width, float height, TextWidth const&);
int                    configCategory(std::string_view path);
nlohmann::ordered_json frameJson(UiFrame const&, PageId, uint64_t frame, std::string const& language, float scale);
} 
