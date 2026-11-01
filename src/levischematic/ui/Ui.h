#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace levischematic::ui {
enum class PageId { Projection, Settings, KeyBindings };



struct Row {
    std::string                      id;
    std::string                      label;
    std::string                      value;
    bool                             editable = false;
    std::function<void(std::string)> submit;
};
struct Page {
    std::string                   title;
    std::string                   message;
    std::vector<Row>              rows;
    std::function<void()>         refresh;
    std::function<void(int, int)> rawKey;
    bool                          capturing = false;
    bool                          dirty     = false;
};

struct PageTabs {
    std::array<std::shared_ptr<Page>, 3> pages;
    PageId                               active = PageId::Projection;
    void                                 clear() noexcept;
    ~PageTabs() { clear(); }
};
Row const* findRow(Page const& page, std::string_view id);
class PageBuilder {
public:
    explicit PageBuilder(std::shared_ptr<Page> page) : mPage(std::move(page)) {}
    PageBuilder& button(std::string id, std::string label, std::function<void()> action);
    PageBuilder& input(std::string id, std::string label, std::string value, std::function<void(std::string)> action);
    
    static std::string validate(Page const& page);

private:
    std::shared_ptr<Page> mPage;
};

std::shared_ptr<Page> buildPage(PageId id);
void                  start();
void                  stop();

void requestOpen(PageId id = PageId::Projection);
void requestToggle();
void pump();
bool isOpen();

std::string diagnostics();
} 
