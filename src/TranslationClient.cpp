#include "TranslationClient.h"

#include "TextUtil.h"

#include <winhttp.h>

namespace jd {
namespace {

std::string UrlEncode(std::string_view value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string output;
    for (unsigned char ch : value) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            output.push_back(static_cast<char>(ch));
        } else if (ch == ' ') {
            output.push_back('+');
        } else {
            output.push_back('%');
            output.push_back(hex[ch >> 4]);
            output.push_back(hex[ch & 0x0F]);
        }
    }
    return output;
}

int HexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

void AppendUtf8(std::string& output, uint32_t codepoint) {
    if (codepoint <= 0x7F) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

std::optional<std::string> HttpGetUtf8(const std::wstring& path) {
    HINTERNET session = WinHttpOpen(L"JustDuckTranslator/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return std::nullopt;
    }

    HINTERNET connect = WinHttpConnect(session, L"translate.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        return std::nullopt;
    }

    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return std::nullopt;
    }

    DWORD timeoutMs = 10000;
    WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    WinHttpSetOption(request, WINHTTP_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    std::string body;
    if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
            std::string chunk(available, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &read)) {
                body.clear();
                break;
            }
            chunk.resize(read);
            body += chunk;
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    if (body.empty()) {
        return std::nullopt;
    }
    return body;
}

std::string ParseJsonString(std::string_view json, size_t& index) {
    std::string value;
    if (index >= json.size() || json[index] != '"') {
        return value;
    }
    ++index;
    while (index < json.size()) {
        char ch = json[index++];
        if (ch == '"') {
            break;
        }
        if (ch == '\\' && index < json.size()) {
            char escaped = json[index++];
            switch (escaped) {
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case 'u': {
                uint32_t codepoint = 0;
                bool valid = index + 4 <= json.size();
                for (size_t i = 0; valid && i < 4; ++i) {
                    const int digit = HexValue(json[index + i]);
                    if (digit < 0) {
                        valid = false;
                    } else {
                        codepoint = (codepoint << 4) | static_cast<uint32_t>(digit);
                    }
                }
                if (valid) {
                    index += 4;
                    AppendUtf8(value, codepoint);
                }
                break;
            }
            default: break;
            }
        } else {
            value.push_back(ch);
        }
    }
    return value;
}

std::optional<TranslationResult> ParseGoogleTranslateResponse(std::string_view json) {
    TranslationResult result;
    std::string translated;
    int depth = 0;
    int segmentStringIndex = 0;
    bool enteredTranslationList = false;

    const size_t start = json.find("[[[");
    if (start == std::string_view::npos) {
        return std::nullopt;
    }

    for (size_t i = start; i < json.size();) {
        const char ch = json[i];
        if (ch == '[') {
            ++depth;
            if (depth == 2) {
                enteredTranslationList = true;
            }
            if (depth == 3) {
                segmentStringIndex = 0;
            }
            ++i;
            continue;
        }
        if (ch == ']') {
            --depth;
            ++i;
            if (enteredTranslationList && depth == 1) {
                break;
            }
            continue;
        }
        if (ch == '"') {
            std::string value = ParseJsonString(json, i);
            if (depth == 3) {
                if (segmentStringIndex == 0) {
                    translated += value;
                }
                ++segmentStringIndex;
            }
            continue;
        }
        ++i;
    }

    result.translatedText = Utf8ToWide(translated);

    size_t detectedStart = json.rfind(",\"");
    if (detectedStart != std::string_view::npos) {
        detectedStart += 1;
        result.detectedLanguage = Utf8ToWide(ParseJsonString(json, detectedStart));
    }

    if (result.translatedText.empty()) {
        return std::nullopt;
    }
    return result;
}

std::wstring NormalizeForCompare(std::wstring_view value) {
    std::wstring normalized;
    normalized.reserve(value.size());
    for (wchar_t ch : value) {
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') || (ch >= L'0' && ch <= L'9')) {
            normalized.push_back(static_cast<wchar_t>(towlower(ch)));
        }
    }
    return normalized;
}

bool LooksUntranslated(std::wstring_view source, std::wstring_view translated) {
    const std::wstring sourceKey = NormalizeForCompare(source);
    const std::wstring translatedKey = NormalizeForCompare(translated);
    if (sourceKey.empty() || translatedKey.empty()) {
        return false;
    }
    return sourceKey == translatedKey || (sourceKey.size() > 24 && translatedKey.find(sourceKey.substr(0, 24)) != std::wstring::npos);
}

std::optional<TranslationResult> RequestTranslation(const std::wstring& text, const std::wstring& targetLanguage, const std::wstring& sourceLanguage) {
    const std::string query = UrlEncode(WideToUtf8(text));
    const std::string target = UrlEncode(WideToUtf8(targetLanguage));
    const std::string source = UrlEncode(WideToUtf8(sourceLanguage));
    const std::wstring path = Utf8ToWide("/translate_a/single?client=gtx&sl=" + source + "&tl=" + target + "&dt=t&q=" + query);

    auto response = HttpGetUtf8(path);
    if (!response) {
        return std::nullopt;
    }
    return ParseGoogleTranslateResponse(*response);
}

}  // namespace

std::optional<TranslationResult> TranslationClient::Translate(const std::wstring& text, const std::wstring& targetLanguage) {
    const std::wstring key = targetLanguage + L"\n" + text;
    if (auto cached = cache_.find(key); cached != cache_.end()) {
        return cached->second;
    }

    const bool shouldForceEnglish = IsVietnameseTarget(targetLanguage) && LooksMostlyEnglish(text) && !LooksVietnameseOcrText(text);
    auto parsed = RequestTranslation(text, targetLanguage, shouldForceEnglish ? L"en" : L"auto");
    if (parsed && !shouldForceEnglish && IsVietnameseTarget(targetLanguage) && LooksUntranslated(text, parsed->translatedText)) {
        parsed = RequestTranslation(text, targetLanguage, L"en");
    }
    if (parsed) {
        cache_[key] = *parsed;
    }
    return parsed;
}

}  // namespace jd
