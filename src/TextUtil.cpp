#include "TextUtil.h"

namespace jd {

std::string WideToUtf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::wstring Trim(std::wstring value) {
    const auto first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(L" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::wstring NormalizeOcrText(std::wstring_view value) {
    std::wstring normalized;
    normalized.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i) {
        const wchar_t ch = value[i];
        if ((ch == L'-' || ch == 0x2010 || ch == 0x2011 || ch == 0x2013) && i + 1 < value.size()) {
            size_t next = i + 1;
            while (next < value.size() && iswspace(value[next])) {
                ++next;
            }
            if (next < value.size() && iswlower(value[next])) {
                i = next - 1;
                continue;
            }
        }

        if (iswspace(ch)) {
            if (!normalized.empty() && normalized.back() != L' ') {
                normalized.push_back(L' ');
            }
            continue;
        }
        normalized.push_back(ch);
    }

    return Trim(normalized);
}

bool LooksVietnamese(std::wstring_view value) {
    return std::any_of(value.begin(), value.end(), [](wchar_t ch) {
        return ch == 0x0110 || ch == 0x0111 ||
               ch == 0x0102 || ch == 0x0103 ||
               ch == 0x00C2 || ch == 0x00E2 ||
               ch == 0x00CA || ch == 0x00EA ||
               ch == 0x00D4 || ch == 0x00F4 ||
               ch == 0x01A0 || ch == 0x01A1 ||
               ch == 0x01AF || ch == 0x01B0 ||
               (ch >= 0x1EA0 && ch <= 0x1EF9);
    });
}

bool LooksMostlyEnglish(std::wstring_view value) {
    int asciiLetters = 0;
    int letters = 0;
    for (wchar_t ch : value) {
        if (iswalpha(ch)) {
            ++letters;
            if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z')) {
                ++asciiLetters;
            }
        }
    }
    return letters >= 8 && asciiLetters * 100 / letters >= 85 && !LooksVietnamese(value);
}

bool LooksVietnameseOcrText(std::wstring_view value) {
    if (LooksVietnamese(value)) {
        return true;
    }

    std::wstring lower;
    lower.reserve(value.size());
    for (wchar_t ch : value) {
        if (iswalnum(ch)) {
            lower.push_back(static_cast<wchar_t>(towlower(ch)));
        } else if (!lower.empty() && lower.back() != L' ') {
            lower.push_back(L' ');
        }
    }
    lower = Trim(lower);

    int hits = 0;
    constexpr std::wstring_view phrases[] = {
        L"viet nam", L"du lich", L"am thuc", L"van hoa", L"hang thang",
        L"trang bao", L"lich su", L"chuyen trang", L"tong hop", L"xuat ban",
        L"cong dong", L"nguoi nhat", L"bi kip", L"trai nghiem", L"thuc te",
        L"ghi lai", L"mon an", L"nha bien tap", L"chu y"
    };

    for (std::wstring_view phrase : phrases) {
        if (lower.find(phrase) != std::wstring::npos) {
            ++hits;
        }
    }

    constexpr std::wstring_view noisyTokens[] = {
        L"duqrc", L"ngudi", L"nhung", L"nhung", L"ddi", L"ndi", L"thvc", L"lich"
    };
    for (std::wstring_view token : noisyTokens) {
        if (lower.find(token) != std::wstring::npos) {
            ++hits;
        }
    }

    return hits >= 2;
}

bool IsVietnameseTarget(std::wstring_view language) {
    std::wstring normalized;
    normalized.reserve(language.size());
    for (wchar_t ch : language) {
        if (!iswspace(ch)) {
            normalized.push_back(static_cast<wchar_t>(towlower(ch)));
        }
    }
    return normalized == L"vi" || normalized == L"vi-vn" || normalized == L"vietnamese";
}

}  // namespace jd
