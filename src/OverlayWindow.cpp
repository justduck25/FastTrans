#include "OverlayWindow.h"

#include <windowsx.h>

namespace jd {
namespace {

const wchar_t* kOverlayClass = L"JustDuckOverlayWindow";
constexpr int kPaddingX = 18;
constexpr int kPaddingY = 14;
constexpr int kTitleHeight = 24;
constexpr int kMinWidth = 280;
constexpr int kMaxWidth = 720;
constexpr int kMaxHeight = 520;
constexpr int kCloseButtonSize = 24;
constexpr COLORREF kOverlayBg = RGB(246, 251, 255);
constexpr COLORREF kOverlayBorder = RGB(132, 190, 255);
constexpr COLORREF kOverlayTitle = RGB(31, 118, 210);
constexpr COLORREF kOverlayText = RGB(36, 49, 68);
constexpr COLORREF kOverlayMuted = RGB(108, 125, 150);
constexpr COLORREF kBlue = RGB(60, 170, 255);
constexpr COLORREF kPink = RGB(255, 118, 174);

HFONT CreateOverlayFont(int pointSize, int weight) {
    HDC screen = GetDC(nullptr);
    const int logicalHeight = -MulDiv(pointSize, GetDeviceCaps(screen, LOGPIXELSY), 72);
    ReleaseDC(nullptr, screen);

    return CreateFontW(logicalHeight, 0, 0, 0, weight, FALSE, FALSE, FALSE, VIETNAMESE_CHARSET,
                       OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY,
                       VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
}

RECT WorkAreaForPoint(POINT point) {
    HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    return info.rcWork;
}

}  // namespace

void OverlayWindow::ShowMessage(const std::wstring& text, const RECT& anchor) {
    text_ = text.empty() ? L"No text found" : text;
    HWND hwnd = EnsureWindow();
    if (!hwnd) {
        return;
    }

    POINT anchorPoint{anchor.left, anchor.bottom};
    RECT work = WorkAreaForPoint(anchorPoint);

    const int availableWidth = std::max(kMinWidth, std::min(kMaxWidth, RectWidth(work) - 32));
    SIZE measured = MeasureText(availableWidth - (kPaddingX * 2));
    const int width = std::clamp(static_cast<int>(measured.cx) + (kPaddingX * 2), kMinWidth, availableWidth);
    const int height = std::clamp(static_cast<int>(measured.cy) + kTitleHeight + (kPaddingY * 2), 96, kMaxHeight);

    int x = anchor.left;
    int y = anchor.bottom + 12;

    if (x + width > work.right) {
        x = work.right - width - 8;
    }
    if (y + height > work.bottom) {
        y = anchor.top - height - 12;
    }
    x = std::max(static_cast<int>(work.left + 8), x);
    y = std::max(static_cast<int>(work.top + 8), y);

    if (hasManualPosition_) {
        x = manualPosition_.x;
        y = manualPosition_.y;
    }

    const bool wasVisible = IsWindowVisible(hwnd) != FALSE;
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, 14, 14);
    SetWindowRgn(hwnd, region, FALSE);
    if (!wasVisible) {
        SetWindowPos(hwnd, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
        AnimateWindow(hwnd, 90, AW_BLEND);
    } else {
        SetWindowPos(hwnd, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    }
    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
}

void OverlayWindow::Hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

HWND OverlayWindow::EnsureWindow() {
    if (hwnd_) {
        return hwnd_;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = OverlayWindow::WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kOverlayClass;
    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kOverlayClass,
        L"Translation",
        WS_POPUP,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        420,
        180,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        this);

    return hwnd_;
}

LRESULT CALLBACK OverlayWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    OverlayWindow* self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = reinterpret_cast<OverlayWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    switch (message) {
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && self) {
            self->Hide();
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (self) {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            RECT closeRect = self->CloseButtonRect(hwnd);
            if (PtInRect(&closeRect, point)) {
                self->Hide();
            } else if (point.y <= kPaddingY + kTitleHeight) {
                self->trackingManualMove_ = true;
                ReleaseCapture();
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
            return 0;
        }
        break;
    case WM_EXITSIZEMOVE:
        if (self && self->trackingManualMove_) {
            RECT windowRect{};
            GetWindowRect(hwnd, &windowRect);
            self->hasManualPosition_ = true;
            self->trackingManualMove_ = false;
            self->manualPosition_ = {windowRect.left, windowRect.top};
        }
        break;
    case WM_PAINT:
        if (self) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            self->LayoutAndPaint(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

RECT OverlayWindow::CloseButtonRect(HWND hwnd) const {
    RECT client{};
    GetClientRect(hwnd, &client);
    return RECT{
        client.right - kPaddingX - kCloseButtonSize + 4,
        kPaddingY - 4,
        client.right - kPaddingX + 4,
        kPaddingY - 4 + kCloseButtonSize,
    };
}

SIZE OverlayWindow::MeasureText(int maxWidth) const {
    HDC dc = GetDC(nullptr);
    HFONT font = CreateOverlayFont(12, FW_NORMAL);
    HGDIOBJ oldFont = SelectObject(dc, font);

    RECT rect{0, 0, maxWidth, 0};
    DrawTextW(dc, text_.c_str(), -1, &rect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);

    SelectObject(dc, oldFont);
    DeleteObject(font);
    ReleaseDC(nullptr, dc);

    return SIZE{RectWidth(rect), RectHeight(rect)};
}

void OverlayWindow::LayoutAndPaint(HWND hwnd, HDC dc) {
    RECT client{};
    GetClientRect(hwnd, &client);

    HBRUSH background = CreateSolidBrush(kOverlayBg);
    HPEN border = CreatePen(PS_SOLID, 1, kOverlayBorder);
    HGDIOBJ oldBrush = SelectObject(dc, background);
    HGDIOBJ oldPen = SelectObject(dc, border);
    RoundRect(dc, client.left, client.top, client.right, client.bottom, 14, 14);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(border);
    DeleteObject(background);

    HPEN accentPen = CreatePen(PS_SOLID, 4, kBlue);
    HGDIOBJ oldAccentPen = SelectObject(dc, accentPen);
    MoveToEx(dc, 18, 1, nullptr);
    LineTo(dc, client.right - 18, 1);
    SelectObject(dc, oldAccentPen);
    DeleteObject(accentPen);

    HBRUSH dotBrush = CreateSolidBrush(kPink);
    HGDIOBJ oldDotBrush = SelectObject(dc, dotBrush);
    HGDIOBJ oldDotPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, 18, 18, 28, 28);
    SelectObject(dc, oldDotPen);
    SelectObject(dc, oldDotBrush);
    DeleteObject(dotBrush);

    SetBkMode(dc, TRANSPARENT);

    HFONT titleFont = CreateOverlayFont(9, FW_SEMIBOLD);
    HGDIOBJ oldFont = SelectObject(dc, titleFont);
    SetTextColor(dc, kOverlayTitle);
    RECT titleRect{kPaddingX + 18, kPaddingY - 2, client.right - kPaddingX, kPaddingY + kTitleHeight};
    DrawTextW(dc, L"Translation", -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, oldFont);
    DeleteObject(titleFont);

    RECT closeRect = CloseButtonRect(hwnd);
    HPEN closePen = CreatePen(PS_SOLID, 2, kOverlayMuted);
    HGDIOBJ oldClosePen = SelectObject(dc, closePen);
    MoveToEx(dc, closeRect.left + 7, closeRect.top + 7, nullptr);
    LineTo(dc, closeRect.right - 7, closeRect.bottom - 7);
    MoveToEx(dc, closeRect.right - 7, closeRect.top + 7, nullptr);
    LineTo(dc, closeRect.left + 7, closeRect.bottom - 7);
    SelectObject(dc, oldClosePen);
    DeleteObject(closePen);

    SetTextColor(dc, kOverlayText);

    HFONT font = CreateOverlayFont(12, FW_NORMAL);
    oldFont = SelectObject(dc, font);

    RECT textRect{kPaddingX, kPaddingY + kTitleHeight, client.right - kPaddingX, client.bottom - kPaddingY};
    DrawTextW(dc, text_.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

    SelectObject(dc, oldFont);
    DeleteObject(font);
}

}  // namespace jd
