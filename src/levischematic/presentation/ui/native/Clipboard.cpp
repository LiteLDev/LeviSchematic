#include "Clipboard.h"
#include <Windows.h>
namespace levischematic::ui {
std::string clipboardRead() {
    std::string result;
    if (!OpenClipboard(nullptr)) return result;
    if (auto h = GetClipboardData(CF_UNICODETEXT)) {
        if (auto p = static_cast<wchar_t const*>(GlobalLock(h))) {
            int n = WideCharToMultiByte(CP_UTF8, 0, p, -1, nullptr, 0, nullptr, nullptr);
            if (n > 1) {
                result.resize(n);
                WideCharToMultiByte(CP_UTF8, 0, p, -1, result.data(), n, nullptr, nullptr);
                result.pop_back();
            }
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return result;
}
void clipboardWrite(std::string const& value) {
    int  n = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    auto h = GlobalAlloc(GMEM_MOVEABLE, (n + 1) * sizeof(wchar_t));
    if (!h) return;
    auto p = static_cast<wchar_t*>(GlobalLock(h));
    if (!p) {
        GlobalFree(h);
        return;
    }
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), p, n);
    p[n] = 0;
    GlobalUnlock(h);
    if (!OpenClipboard(nullptr)) {
        GlobalFree(h);
        return;
    }
    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h);
    CloseClipboard();
}
} // namespace levischematic::ui
