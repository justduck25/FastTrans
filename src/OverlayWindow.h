#pragma once

#include "Common.h"

namespace jd {

class OverlayWindow {
public:
    void ShowMessage(const std::wstring& text, const RECT& anchor);
    void Hide();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    HWND EnsureWindow();
    RECT CloseButtonRect(HWND hwnd) const;
    SIZE MeasureText(int maxWidth) const;
    void LayoutAndPaint(HWND hwnd, HDC dc);

    HWND hwnd_ = nullptr;
    std::wstring text_;
};

}  // namespace jd
