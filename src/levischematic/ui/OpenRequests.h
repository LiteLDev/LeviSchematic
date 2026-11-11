#pragma once
#include "Ui.h"
#include <optional>

namespace levischematic::ui {


struct OpenRequests {
    std::optional<PageId> page;
    bool                  close = false;
    void                  open(PageId id) {
        page  = id;
        close = false;
    }
    void toggle(bool visible) {
        if (page) {
            page.reset();
            close = visible;
        } else if (visible && !close) close = true;
        else {
            page  = PageId::MainMenu;
            close = false;
        }
    }
    void didClose() { close = false; }
    void clear() {
        page.reset();
        close = false;
    }
};
} 
