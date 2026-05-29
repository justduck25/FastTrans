#include "AppController.h"

#include <winrt/base.h>

namespace {

const wchar_t* kMainWindowClass = L"JustDuckTranslatorMainWindow";

void EnableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto setContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setContext && setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }

    SetProcessDPIAware();
}

jd::AppController* Controller(HWND hwnd) {
    return reinterpret_cast<jd::AppController*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    jd::AppController* controller = Controller(hwnd);
    if (message == WM_NCCREATE) {
        const CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        controller = reinterpret_cast<jd::AppController*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(controller));
    }

    switch (message) {
    case WM_CREATE:
        if (controller && !controller->Initialize(hwnd)) {
            MessageBoxW(hwnd, L"Could not register Ctrl + Shift + T. The app will keep running from the tray.", L"Hotkey unavailable",
                        MB_OK | MB_ICONWARNING);
        }
        return 0;
    case WM_HOTKEY:
        if (wparam == jd::kHotkeyTranslateId && controller) {
            controller->TranslateFromScreen(hwnd);
            return 0;
        }
        break;
    case jd::kTrayMessage:
        if ((lparam == WM_RBUTTONUP || lparam == WM_LBUTTONDBLCLK) && controller) {
            controller->ShowTrayMenu(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        if (controller) {
            controller->Shutdown(hwnd);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    EnableDpiAwareness();
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    WNDCLASSW wc{};
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kMainWindowClass;
    RegisterClassW(&wc);

    jd::AppController controller;
    HWND hwnd = CreateWindowExW(0, kMainWindowClass, L"JustDuck Translator", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, instance, &controller);
    if (!hwnd) {
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    winrt::uninit_apartment();
    return static_cast<int>(msg.wParam);
}
