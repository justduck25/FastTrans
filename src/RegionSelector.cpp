#include "RegionSelector.h"

#include <windowsx.h>

namespace jd {
namespace {

const wchar_t* kRegionSelectorClass = L"JustDuckRegionSelector";

RECT VirtualScreenRect() {
    RECT rect{};
    rect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    rect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    rect.right = rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    rect.bottom = rect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return rect;
}

POINT CurrentCursorPoint() {
    POINT point{};
    GetCursorPos(&point);
    return point;
}

RECT SelectionRect(const RegionSelector::State& state) {
    return NormalizeRect({state.start.x, state.start.y, state.current.x, state.current.y});
}

}  // namespace

std::optional<RECT> RegionSelector::Select(HWND owner) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = RegionSelector::WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = kRegionSelectorClass;
    RegisterClassW(&wc);

    State state;
    state.virtualScreen = VirtualScreenRect();

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kRegionSelectorClass,
        L"Select text region",
        WS_POPUP,
        state.virtualScreen.left,
        state.virtualScreen.top,
        RectWidth(state.virtualScreen),
        RectHeight(state.virtualScreen),
        owner,
        nullptr,
        GetModuleHandleW(nullptr),
        &state);

    if (!hwnd) {
        return std::nullopt;
    }

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 70, LWA_ALPHA);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetCapture(hwnd);

    MSG msg{};
    while (!state.completed && !state.cancelled && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ReleaseCapture();
    DestroyWindow(hwnd);

    if (state.cancelled) {
        return std::nullopt;
    }

    RECT rect = SelectionRect(state);
    if (RectWidth(rect) < 6 || RectHeight(rect) < 6) {
        return std::nullopt;
    }
    return rect;
}

LRESULT CALLBACK RegionSelector::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    State* state = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        state = reinterpret_cast<State*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message) {
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && state) {
            state->cancelled = true;
            PostMessageW(hwnd, WM_NULL, 0, 0);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (state) {
            state->dragging = true;
            state->start = CurrentCursorPoint();
            state->current = state->start;
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (state && state->dragging) {
            state->current = CurrentCursorPoint();
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (state && state->dragging) {
            state->current = CurrentCursorPoint();
            state->dragging = false;
            state->completed = true;
            PostMessageW(hwnd, WM_NULL, 0, 0);
            return 0;
        }
        break;
    case WM_PAINT:
        if (state) {
            Paint(hwnd, *state);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void RegionSelector::Paint(HWND hwnd, const State& state) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);

    RECT client{};
    GetClientRect(hwnd, &client);
    HBRUSH dimBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &client, dimBrush);
    DeleteObject(dimBrush);

    if (state.dragging || state.completed) {
        RECT rect = SelectionRect(state);
        OffsetRect(&rect, -state.virtualScreen.left, -state.virtualScreen.top);

        HBRUSH clearBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(dc, &rect, clearBrush);
        DeleteObject(clearBrush);

        HPEN pen = CreatePen(PS_SOLID, 2, RGB(30, 144, 255));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }

    EndPaint(hwnd, &ps);
}

}  // namespace jd
