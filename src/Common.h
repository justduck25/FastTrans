#pragma once

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace jd {

constexpr UINT kHotkeyTranslateId = 1001;
constexpr UINT kTrayIconId = 2001;
constexpr UINT kTrayMessage = WM_APP + 1;

struct Settings {
    std::wstring targetLanguage = L"vi";
    std::wstring ocrProvider = L"windows";
    std::wstring ocrLanguage;
    bool saveHistory = false;
};

struct TextBox {
    std::wstring text;
    RECT bounds{};
};

struct OcrResult {
    std::wstring text;
    std::vector<TextBox> lines;
    std::vector<TextBox> words;
};

struct TranslationResult {
    std::wstring translatedText;
    std::wstring detectedLanguage;
};

inline int RectWidth(const RECT& rect) {
    return rect.right - rect.left;
}

inline int RectHeight(const RECT& rect) {
    return rect.bottom - rect.top;
}

inline bool IsEmptyRect(const RECT& rect) {
    return RectWidth(rect) <= 0 || RectHeight(rect) <= 0;
}

inline RECT NormalizeRect(RECT rect) {
    if (rect.left > rect.right) {
        std::swap(rect.left, rect.right);
    }
    if (rect.top > rect.bottom) {
        std::swap(rect.top, rect.bottom);
    }
    return rect;
}

}  // namespace jd
