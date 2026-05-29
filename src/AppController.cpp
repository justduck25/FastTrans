#include "AppController.h"

#include "TextUtil.h"

#include <shellapi.h>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Media.Ocr.h>

namespace jd {
namespace {

constexpr UINT kCommandTranslate = 3001;
constexpr UINT kCommandSettings = 3002;
constexpr UINT kCommandAbout = 3003;
constexpr UINT kCommandExit = 3004;
constexpr UINT kSettingsSave = 4001;
constexpr UINT kSettingsCancel = 4002;
constexpr UINT kSettingsTargetCombo = 4003;
constexpr UINT kSettingsHistoryCheck = 4004;
constexpr UINT kSettingsOcrCombo = 4005;
constexpr UINT kSettingsProviderCombo = 4006;
constexpr COLORREF kSettingsBg = RGB(246, 251, 255);
constexpr COLORREF kSettingsText = RGB(45, 54, 74);
constexpr COLORREF kSettingsMuted = RGB(98, 116, 145);
constexpr COLORREF kSettingsAccent = RGB(32, 122, 218);
constexpr COLORREF kSettingsBlue = RGB(64, 170, 255);
constexpr COLORREF kSettingsPink = RGB(255, 115, 174);

void AddTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid{sizeof(nid)};
    nid.hWnd = hwnd;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kTrayMessage;
    nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"JustDuck Translator");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid{sizeof(nid)};
    nid.hWnd = hwnd;
    nid.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

struct SettingsDialogState {
    Settings settings;
    std::vector<std::wstring> ocrCodes;
    HBRUSH backgroundBrush = nullptr;
    HFONT titleFont = nullptr;
    bool saved = false;
};

const wchar_t* kSettingsClass = L"JustDuckSettingsWindow";

struct LanguageOption {
    const wchar_t* label;
    const wchar_t* code;
};

constexpr LanguageOption kTargetLanguages[] = {
    {L"Vietnamese", L"vi"},
};

constexpr LanguageOption kOcrProviders[] = {
    {L"Bundled Tesseract", L"tesseract"},
    {L"Windows OCR", L"windows"},
};

constexpr LanguageOption kTesseractLanguages[] = {
    {L"Auto from bundled language data", L""},
    {L"English", L"en-US"},
    {L"Vietnamese", L"vi-VN"},
    {L"Japanese", L"ja-JP"},
    {L"Korean", L"ko-KR"},
    {L"Chinese Simplified", L"zh-Hans"},
    {L"Chinese Traditional", L"zh-Hant"},
    {L"French", L"fr-FR"},
    {L"German", L"de-DE"},
    {L"Spanish", L"es-ES"},
    {L"Italian", L"it-IT"},
    {L"Portuguese", L"pt-BR"},
    {L"Russian", L"ru-RU"},
    {L"Thai", L"th-TH"},
};

HFONT DialogFont() {
    return reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

HFONT CreateDialogFont(int pointSize, int weight) {
    HDC screen = GetDC(nullptr);
    const int logicalHeight = -MulDiv(pointSize, GetDeviceCaps(screen, LOGPIXELSY), 72);
    ReleaseDC(nullptr, screen);
    return CreateFontW(logicalHeight, 0, 0, 0, weight, FALSE, FALSE, FALSE, VIETNAMESE_CHARSET,
                       OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY,
                       VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
}

HWND CreateLabel(HWND hwnd, const wchar_t* text, int x, int y, int width, int height = 22) {
    HWND label = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, width, height, hwnd, nullptr, nullptr, nullptr);
    SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(DialogFont()), TRUE);
    return label;
}

std::wstring LanguageItemText(const LanguageOption& option) {
    if (option.code[0] == L'\0') {
        return option.label;
    }
    return std::wstring(option.label) + L" (" + option.code + L")";
}

void FillLanguageCombo(HWND combo, const LanguageOption* options, size_t count, const std::wstring& selectedCode) {
    int selected = 0;
    const std::wstring trimmedSelected = Trim(selectedCode);
    for (size_t i = 0; i < count; ++i) {
        const std::wstring text = LanguageItemText(options[i]);
        const LRESULT index = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        SendMessageW(combo, CB_SETITEMDATA, index, reinterpret_cast<LPARAM>(options[i].code));
        if (trimmedSelected == options[i].code) {
            selected = static_cast<int>(index);
        }
    }
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

std::wstring SelectedLanguageCode(HWND hwnd, UINT controlId) {
    HWND combo = GetDlgItem(hwnd, controlId);
    const LRESULT index = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR) {
        return {};
    }
    auto code = reinterpret_cast<const wchar_t*>(SendMessageW(combo, CB_GETITEMDATA, index, 0));
    return code ? std::wstring(code) : std::wstring();
}

void FillTesseractLanguages(HWND combo, SettingsDialogState& state) {
    state.ocrCodes.clear();
    int selected = 0;
    const std::wstring selectedCode = Trim(state.settings.ocrLanguage);

    for (size_t i = 0; i < std::size(kTesseractLanguages); ++i) {
        state.ocrCodes.push_back(kTesseractLanguages[i].code);
        const std::wstring text = LanguageItemText(kTesseractLanguages[i]);
        const LRESULT index = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        SendMessageW(combo, CB_SETITEMDATA, index, static_cast<LPARAM>(i));
        if (selectedCode == kTesseractLanguages[i].code) {
            selected = static_cast<int>(index);
        }
    }
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

void FillInstalledOcrLanguages(HWND combo, SettingsDialogState& state) {
    state.ocrCodes.clear();
    state.ocrCodes.push_back(L"");

    const LRESULT autoIndex = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Auto detect from Windows profile"));
    SendMessageW(combo, CB_SETITEMDATA, autoIndex, 0);

    int selected = 0;
    for (const auto& language : winrt::Windows::Media::Ocr::OcrEngine::AvailableRecognizerLanguages()) {
        const std::wstring code = std::wstring(language.LanguageTag());
        const std::wstring label = std::wstring(language.DisplayName()) + L" (" + code + L")";

        state.ocrCodes.push_back(code);
        const LRESULT codeIndex = static_cast<LRESULT>(state.ocrCodes.size() - 1);
        const LRESULT index = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        SendMessageW(combo, CB_SETITEMDATA, index, codeIndex);
        if (Trim(state.settings.ocrLanguage) == code) {
            selected = static_cast<int>(index);
        }
    }

    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

std::wstring SelectedOcrLanguageCode(HWND hwnd, const SettingsDialogState& state) {
    HWND combo = GetDlgItem(hwnd, kSettingsOcrCombo);
    const LRESULT index = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR) {
        return {};
    }
    const LRESULT codeIndex = SendMessageW(combo, CB_GETITEMDATA, index, 0);
    if (codeIndex < 0 || static_cast<size_t>(codeIndex) >= state.ocrCodes.size()) {
        return {};
    }
    return state.ocrCodes[codeIndex];
}

void RefreshOcrLanguageCombo(HWND hwnd, SettingsDialogState& state) {
    HWND combo = GetDlgItem(hwnd, kSettingsOcrCombo);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    state.settings.ocrProvider = SelectedLanguageCode(hwnd, kSettingsProviderCombo);
    if (state.settings.ocrProvider == L"tesseract") {
        FillTesseractLanguages(combo, state);
    } else {
        FillInstalledOcrLanguages(combo, state);
    }
}

LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    SettingsDialogState* state = reinterpret_cast<SettingsDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        state = reinterpret_cast<SettingsDialogState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message) {
    case WM_CREATE: {
        if (state) {
            state->backgroundBrush = CreateSolidBrush(kSettingsBg);
        }

        state->titleFont = CreateDialogFont(14, FW_SEMIBOLD);
        HWND title = CreateLabel(hwnd, L"FastTrans Settings", 22, 18, 340, 26);
        SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(state->titleFont), TRUE);
        SetPropW(title, L"TitleLabel", reinterpret_cast<HANDLE>(1));
        CreateLabel(hwnd, L"Choose how text is read before it becomes Vietnamese.", 22, 46, 340, 22);

        CreateLabel(hwnd, L"Translate to", 22, 86, 340);
        HWND targetCombo = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                         22, 110, 340, 180, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsTargetCombo)), nullptr, nullptr);
        SendMessageW(targetCombo, WM_SETFONT, reinterpret_cast<WPARAM>(DialogFont()), TRUE);
        FillLanguageCombo(targetCombo, kTargetLanguages, std::size(kTargetLanguages), state->settings.targetLanguage);

        CreateLabel(hwnd, L"OCR engine", 22, 150, 340);
        HWND providerCombo = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                           22, 174, 340, 140, hwnd,
                                           reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsProviderCombo)), nullptr, nullptr);
        SendMessageW(providerCombo, WM_SETFONT, reinterpret_cast<WPARAM>(DialogFont()), TRUE);
        FillLanguageCombo(providerCombo, kOcrProviders, std::size(kOcrProviders), state->settings.ocrProvider);

        CreateLabel(hwnd, L"Text language", 22, 214, 340);
        HWND ocrCombo = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                      22, 238, 340, 260, hwnd,
                                      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsOcrCombo)), nullptr, nullptr);
        SendMessageW(ocrCombo, WM_SETFONT, reinterpret_cast<WPARAM>(DialogFont()), TRUE);
        RefreshOcrLanguageCombo(hwnd, *state);

        HWND check = CreateWindowW(L"BUTTON", L"Save local history", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                   22, 288, 250, 24, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsHistoryCheck)), nullptr, nullptr);
        SendMessageW(check, BM_SETCHECK, state->settings.saveHistory ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(check, WM_SETFONT, reinterpret_cast<WPARAM>(DialogFont()), TRUE);

        CreateLabel(hwnd, L"Default uses bundled OCR with auto language scoring.", 22, 324, 340);

        HWND save = CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 192, 358, 80, 30, hwnd,
                      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsSave)), nullptr, nullptr);
        HWND cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 282, 358, 80, 30, hwnd,
                      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kSettingsCancel)), nullptr, nullptr);
        SendMessageW(save, WM_SETFONT, reinterpret_cast<WPARAM>(DialogFont()), TRUE);
        SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(DialogFont()), TRUE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        HBRUSH bg = CreateSolidBrush(kSettingsBg);
        FillRect(dc, &client, bg);
        DeleteObject(bg);

        HPEN accent = CreatePen(PS_SOLID, 4, kSettingsBlue);
        HGDIOBJ oldPen = SelectObject(dc, accent);
        MoveToEx(dc, 22, 8, nullptr);
        LineTo(dc, client.right - 22, 8);
        SelectObject(dc, oldPen);
        DeleteObject(accent);

        HBRUSH dot = CreateSolidBrush(kSettingsPink);
        HGDIOBJ oldBrush = SelectObject(dc, dot);
        HGDIOBJ oldNullPen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, client.right - 48, 28, client.right - 32, 44);
        SelectObject(dc, oldNullPen);
        SelectObject(dc, oldBrush);
        DeleteObject(dot);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == kSettingsProviderCombo && HIWORD(wparam) == CBN_SELCHANGE && state) {
            RefreshOcrLanguageCombo(hwnd, *state);
            return 0;
        }
        if (LOWORD(wparam) == kSettingsSave && state) {
            state->settings.targetLanguage = SelectedLanguageCode(hwnd, kSettingsTargetCombo);
            if (state->settings.targetLanguage.empty()) {
                state->settings.targetLanguage = L"vi";
            }
            state->settings.ocrProvider = SelectedLanguageCode(hwnd, kSettingsProviderCombo);
            state->settings.ocrLanguage = SelectedOcrLanguageCode(hwnd, *state);
            state->settings.saveHistory = SendDlgItemMessageW(hwnd, kSettingsHistoryCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            state->saved = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wparam) == kSettingsCancel) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_CTLCOLORDLG:
        if (state && state->backgroundBrush) {
            return reinterpret_cast<LRESULT>(state->backgroundBrush);
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkMode(dc, TRANSPARENT);
        HWND control = reinterpret_cast<HWND>(lparam);
        if (GetPropW(control, L"TitleLabel")) {
            SetTextColor(dc, kSettingsAccent);
        } else {
            SetTextColor(dc, kSettingsMuted);
        }
        if (state && state->backgroundBrush) {
            return reinterpret_cast<LRESULT>(state->backgroundBrush);
        }
        break;
    }
    case WM_DESTROY:
        if (state && state->backgroundBrush) {
            DeleteObject(state->backgroundBrush);
            state->backgroundBrush = nullptr;
        }
        if (state && state->titleFont) {
            DeleteObject(state->titleFont);
            state->titleFont = nullptr;
        }
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

std::optional<Settings> ShowSettingsDialog(HWND owner, Settings settings) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = SettingsWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kSettingsClass;
    RegisterClassW(&wc);

    SettingsDialogState state{settings};
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW, kSettingsClass, L"JustDuck Settings",
                                WS_CAPTION | WS_SYSMENU | WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 405, 450,
                                owner, nullptr, GetModuleHandleW(nullptr), &state);
    if (!hwnd) {
        return std::nullopt;
    }

    EnableWindow(owner, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    MSG msg{};
    while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);

    if (state.saved) {
        return state.settings;
    }
    return std::nullopt;
}

}  // namespace

bool AppController::Initialize(HWND hwnd) {
    settings_ = settingsStore_.Load();
    AddTrayIcon(hwnd);
    return hotkey_.Register(hwnd);
}

void AppController::Shutdown(HWND hwnd) {
    hotkey_.Unregister(hwnd);
    RemoveTrayIcon(hwnd);
}

void AppController::TranslateFromScreen(HWND hwnd) {
    auto region = selector_.Select(hwnd);
    if (!region) {
        return;
    }

    auto captured = capture_.CaptureRegion(*region);
    if (!captured) {
        overlay_.ShowMessage(L"Could not capture that screen region.", *region);
        return;
    }

    overlay_.ShowMessage(L"Reading text...", *region);
    OcrResult ocr = ocr_.Recognize(*captured, settings_);
    const std::wstring source = NormalizeOcrText(ocr.text);
    if (source.empty()) {
        overlay_.ShowMessage(ocr.errorMessage.empty() ? L"No text found." : ocr.errorMessage, *region);
        return;
    }

    if (IsVietnameseTarget(settings_.targetLanguage) && LooksVietnameseOcrText(source)) {
        overlay_.ShowMessage(L"Text is already Vietnamese.", *region);
        return;
    }

    overlay_.ShowMessage(L"Translating...", *region);
    auto translated = translator_.Translate(source, settings_.targetLanguage);
    if (!translated) {
        overlay_.ShowMessage(L"Translation failed. Check your network connection or translation endpoint.", *region);
        return;
    }

    history_.Remember(source, *translated, settings_);
    overlay_.ShowMessage(translated->translatedText, *region);
}

void AppController::ShowSettings(HWND owner) {
    auto updated = ShowSettingsDialog(owner, settings_);
    if (updated) {
        settings_ = *updated;
        settingsStore_.Save(settings_);
    }
}

void AppController::ShowTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kCommandTranslate, L"Translate now");
    AppendMenuW(menu, MF_STRING, kCommandSettings, L"Settings");
    AppendMenuW(menu, MF_STRING, kCommandAbout, L"About");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandExit, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(hwnd);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd, nullptr);
    DestroyMenu(menu);

    switch (command) {
    case kCommandTranslate:
        TranslateFromScreen(hwnd);
        break;
    case kCommandSettings:
        ShowSettings(hwnd);
        break;
    case kCommandAbout:
        ShowAbout(hwnd);
        break;
    case kCommandExit:
        PostQuitMessage(0);
        break;
    }
}

void AppController::ShowAbout(HWND owner) const {
    MessageBoxW(owner,
                L"JustDuck Translator\n\nCtrl + Shift + T to translate a selected screen region.\nOCR is local; only extracted text is sent for translation.",
                L"About JustDuck Translator",
                MB_OK | MB_ICONINFORMATION);
}

}  // namespace jd
