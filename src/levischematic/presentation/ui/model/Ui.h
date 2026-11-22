#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace levischematic::ui {
enum class PageId {
    Projection,
    Settings,
    KeyBindings,
    MainMenu,
    Selection,
    Placement,
    SaveSelection,
    Layers,
    Verifier,
    Session,
    Materials,
    Survival,
    Resources,
    Edit,
    WorldOperations,
    Projects,
    Signs,
    Load,
    Tasks,
    HudLayout,
    SchematicManager,
    AreaSelections,
    SubRegion,
    PlacementGrid
};

inline std::optional<PageId> configurationParent(PageId page) {
    if (page == PageId::Settings) return PageId::MainMenu;
    if (page == PageId::HudLayout) return PageId::Settings;
    return {};
}


enum class RowKind {
    Action,
    Text,
    Toggle,
    Slider,
    Choice,
    Info,
    File,
    Range,
    KeyBinding,
    Color,
    ActionBar,
    Entry,
    TableRow,
    Checkbox,
    Label,
    Adjust,
    IconButton
};
enum class PageLayout { List, Menu, Browser, Form, Config, Table };
enum class Slot { Body, TopBar, TopRight, Status, FooterLeft, FooterRight };
struct IconRef {
    int   atlas = 0;
    float u = 0, v = 0, w = 0, h = 0;
    bool  variants = false;
    bool  valid() const { return atlas != 0 && w > 0 && h > 0; }
};
namespace icons {
inline constexpr IconRef None{};
inline constexpr IconRef Litematic{1, 144, 0, 12, 12};
inline constexpr IconRef Structure{1, 144, 36, 12, 12};
inline constexpr IconRef Json{1, 144, 48, 12, 12};
inline constexpr IconRef Text{1, 144, 60, 12, 12};
inline constexpr IconRef Directory{1, 156, 0, 12, 12};
inline constexpr IconRef DirectoryUp{1, 156, 12, 12, 12};
inline constexpr IconRef DirectoryRoot{1, 156, 24, 12, 12};
inline constexpr IconRef Search{1, 156, 36, 12, 12};
inline constexpr IconRef NewFolder{1, 156, 48, 12, 12};
inline constexpr IconRef Memory{1, 186, 0, 12, 12};
inline constexpr IconRef Info{1, 168, 18, 11, 11};
inline constexpr IconRef Notice{1, 168, 29, 11, 11};
inline constexpr IconRef Lock{1, 168, 51, 11, 11};
inline constexpr IconRef CheckboxOff{1, 198, 0, 11, 11};
inline constexpr IconRef CheckboxOn{1, 198, 11, 11, 11};
inline constexpr IconRef ArrowUp{1, 209, 0, 15, 15};
inline constexpr IconRef ArrowDown{1, 209, 15, 15, 15};
inline constexpr IconRef PlusMinus{1, 0, 128, 16, 16, true};
inline constexpr IconRef BoxOn{1, 0, 144, 16, 16, true};
inline constexpr IconRef BoxOff{1, 0, 160, 16, 16, true};
inline constexpr IconRef menu(float v) { return {1, 116, v, 14, 14}; }
inline constexpr IconRef MenuAreaSelections = menu(0), MenuLoaded = menu(14), MenuLoad = menu(28),
                         MenuPlacements = menu(42), MenuManager = menu(56), MenuAreaEditor = menu(70),
                         MenuConfig = menu(84), MenuTasks = menu(112);
} // namespace icons
namespace colors {
inline constexpr uint32_t Default = 0, White = 0xFFFFFFFF, Gray = 0xFFA0A0A0, Green = 0xFF55FF55, Red = 0xFFFF5555,
                          Gold = 0xFFFFAA00, Yellow = 0xFFFFFF55, Aqua = 0xFF55FFFF;
}
struct SpritePart {
    float u = 0, v = 0, w = 0, h = 0, dx = 0, dy = 0;
};
struct Modifiers {
    bool shift = false, ctrl = false, alt = false;
};
int       adjustAmount(int mode, Modifiers modifiers);
Modifiers currentModifiers();
struct Row {
    std::string                      id;
    std::string                      label;
    std::string                      value;
    bool                             editable = false;
    std::function<void(std::string)> submit;
    RowKind                          kind    = RowKind::Action;
    double                           minimum = 0, maximum = 1, step = 0.01;
    std::vector<std::string>         choices;
    std::string                      value2;
    std::function<void(std::string)> submit2;
    std::function<void()>            activate;
    std::function<void()>            reset;
    std::function<void()>            clear;
    std::function<void(int, double)> colorSubmit;
    std::array<double, 4>            color{0, 0, 0, 1};
    std::vector<Row>                 actions;
    double                           displayScale = 1;
    std::string                      unit;
    bool                             selected = false;
    bool                             enabled  = true;
    std::vector<std::string>         columns;
    std::vector<uint32_t>            columnColors;
    std::string                      tooltip;
    Slot                             slot = Slot::Body;
    IconRef                          icon;
    uint32_t                         textColor = colors::Default;
    std::function<void(int)>         adjust;
    int                              adjustMode = 0;
    bool                             checked    = false;
    bool                             placed     = false;
    float                            x = 0, y = 0, width = 0, height = 20;
    std::function<void()>            secondary;
    bool                             yesNo = false;
    std::vector<SpritePart>          parts;
    std::string                      partsTooltip;
    std::string                      item;
    int                              itemAux = 0;
};
struct InfoLine {
    std::string label, value;
    uint32_t    color = colors::Default;
};
struct Page {
    std::string                         title;
    std::string                         message;
    std::vector<Row>                    rows;
    std::function<void()>               refresh;
    std::function<void(int, int, bool)> rawKey;
    bool                                capturing         = false;
    bool                                dirty             = false;
    bool                                placementSettings = false;
    std::string                         fileName;
    std::function<bool()>               save;
    PageId                              route = PageId::Projection;
    std::function<void()>               dismiss;
    PageLayout                          layout = PageLayout::List;
    std::string                         crumb;
    bool                                fullTitle = false;
    std::string                         path;
    std::vector<InfoLine>               info;
    bool                                sidePanel = false;
    std::vector<std::string>            header;
    int                                 sortColumn     = -1;
    bool                                sortDescending = false;
    std::function<void(int)>            sort;
    float                               listX = 12, listY = 30, listWidth = -12, listHeight = -32;
    bool                                searchable = true;
    std::string                         modalTitle;
    std::function<void()>               captureEscape;
};

void clearPage(Page& page) noexcept;
void beginModal(std::shared_ptr<Page> const& page, std::string message);
void dismissModal(std::shared_ptr<Page> const& page);
enum class RowSection { File, Info, Main, Color, Footer };
RowSection                           rowSection(Row const& row);
Row const*                           sectionRow(Page const& page, RowSection section, int index);
int                                  sectionSize(Page const& page, RowSection section);
std::string                          infoText(Page const& page);
void                                 moveActionsToFooter(Page& page, std::vector<std::string> const& ids);
double                               sliderNativeValue(double value, double minimum, double maximum, double step);
double                               sliderModelValue(double native, double minimum, double maximum, double step);
std::string                          formatColor(std::array<double, 4> const& color);
std::optional<std::array<double, 4>> parseColor(std::string text);
std::string                          formatHexColor(std::array<double, 4> const& color);
std::optional<std::array<double, 4>> parseHexColor(std::string text);
int                                  clampLayerInput(std::string_view text, int previous, int minimum, int maximum);
Row const*                           findRow(Page const& page, std::string_view id);
class PageBuilder {
public:
    explicit PageBuilder(std::shared_ptr<Page> page) : mPage(std::move(page)) {}
    PageBuilder& button(std::string id, std::string label, std::function<void()> action);
    PageBuilder& input(std::string id, std::string label, std::string value, std::function<void(std::string)> action);
    PageBuilder& info(std::string id, std::string text);
    PageBuilder& at(float x, float y, float width, float height = 20);
    PageBuilder& in(Slot slot);
    PageBuilder& tip(std::string text);
    PageBuilder& icon(IconRef icon);
    PageBuilder& color(uint32_t color);
    PageBuilder& kind(RowKind kind);
    PageBuilder& right(std::function<void()> action);
    PageBuilder& enabled(bool value);
    PageBuilder& checkbox(std::string id, std::string label, bool checked, std::function<void()> toggle);
    PageBuilder& toggle(std::string id, std::string label, bool value, std::function<void()> action);
    PageBuilder& label(std::string id, std::string text);
    PageBuilder& adjuster(std::string id, int mode, std::function<void(int)> action);
    Row&         last();

    static std::string validate(Page const& page);

private:
    std::shared_ptr<Page> mPage;
};

std::shared_ptr<Page> buildPage(PageId id);
void                  start();
void                  stop();

void requestOpen(PageId id = PageId::MainMenu);
void requestToggle();
void pump();
bool isOpen();

std::string diagnostics();
std::string exportLayout();
} // namespace levischematic::ui
