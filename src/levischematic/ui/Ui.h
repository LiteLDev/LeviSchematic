#pragma once

#include <array>
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
    HudLayout
};


enum class RowKind { Action, Text, Toggle, Slider, Choice, Info, File, Range, KeyBinding, Color, ActionBar, Entry, TableRow };
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
    bool                             enabled = true;
    std::vector<std::string>          columns;
};
struct Page {
    std::string                   title;
    std::string                   message;
    std::vector<Row>              rows;
    std::function<void()>         refresh;
    std::function<void(int, int, bool)> rawKey;
    bool                          capturing = false;
    bool                          dirty     = false;
    bool                                placementSettings = false;
    std::string                   fileName;
    std::function<bool()>          save;
    PageId                        route = PageId::Projection;
    std::function<void()>          dismiss;
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
int                                  clampLayerInput(std::string_view text, int previous, int minimum, int maximum);
Row const*                           findRow(Page const& page, std::string_view id);
class PageBuilder {
public:
    explicit PageBuilder(std::shared_ptr<Page> page) : mPage(std::move(page)) {}
    PageBuilder& button(std::string id, std::string label, std::function<void()> action);
    PageBuilder& input(std::string id, std::string label, std::string value, std::function<void(std::string)> action);
    PageBuilder& info(std::string id, std::string text);
    
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
} 
