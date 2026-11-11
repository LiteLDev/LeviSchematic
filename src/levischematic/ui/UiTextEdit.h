#pragma once
#include <algorithm>
#include <string>
#include <string_view>

namespace levischematic::ui {

struct UiTextEdit {
    std::string text;
    size_t      caret = 0, anchor = 0;
    void        set(std::string value) {
        text  = std::move(value);
        caret = anchor = text.size();
    }
    size_t previous(size_t p) const {
        if (!p) return 0;
        --p;
        while (p && (static_cast<unsigned char>(text[p]) & 0xc0) == 0x80) --p;
        return p;
    }
    size_t next(size_t p) const {
        if (p >= text.size()) return text.size();
        ++p;
        while (p < text.size() && (static_cast<unsigned char>(text[p]) & 0xc0) == 0x80) ++p;
        return p;
    }
    void move(size_t position, bool select) {
        caret = std::min(position, text.size());
        if (!select) anchor = caret;
    }
    void selectAll() {
        anchor = 0;
        caret  = text.size();
    }
    std::string selection() const {
        return text.substr(std::min(caret, anchor), std::max(caret, anchor) - std::min(caret, anchor));
    }
    void insert(std::string_view value) {
        auto first = std::min(caret, anchor), last = std::max(caret, anchor);
        text.replace(first, last - first, value);
        caret = anchor = first + value.size();
    }
    void erase(bool backwards) {
        if (caret == anchor) anchor = backwards ? previous(caret) : next(caret);
        insert({});
    }
};
} 
