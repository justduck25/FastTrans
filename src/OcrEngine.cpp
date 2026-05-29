#include "OcrEngine.h"

#include "TextUtil.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>

#include <cstdint>
#include <cstring>

namespace jd {
namespace {

#pragma pack(push, 1)
struct BitmapFileHeader {
    WORD type = 0x4D42;
    DWORD size = 0;
    WORD reserved1 = 0;
    WORD reserved2 = 0;
    DWORD offBits = sizeof(BitmapFileHeader) + sizeof(BITMAPINFOHEADER);
};
#pragma pack(pop)

std::vector<uint8_t> HBitmapToBmpBytes(HBITMAP bitmap, SIZE size) {
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = size.cx;
    info.biHeight = size.cy;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;

    const DWORD pixelBytes = static_cast<DWORD>(size.cx * size.cy * 4);
    std::vector<uint8_t> pixels(pixelBytes);

    HDC dc = GetDC(nullptr);
    const int rows = GetDIBits(dc, bitmap, 0, static_cast<UINT>(size.cy), pixels.data(), reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (rows != size.cy) {
        return {};
    }

    BitmapFileHeader fileHeader;
    fileHeader.size = static_cast<DWORD>(sizeof(fileHeader) + sizeof(info) + pixels.size());

    std::vector<uint8_t> bytes(fileHeader.size);
    size_t offset = 0;
    std::memcpy(bytes.data() + offset, &fileHeader, sizeof(fileHeader));
    offset += sizeof(fileHeader);
    std::memcpy(bytes.data() + offset, &info, sizeof(info));
    offset += sizeof(info);
    std::memcpy(bytes.data() + offset, pixels.data(), pixels.size());
    return bytes;
}

std::wstring ModuleDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring directory = path;
    const size_t slash = directory.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        directory.resize(slash);
    }
    return directory;
}

std::wstring Quote(std::wstring_view value) {
    std::wstring quoted = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') {
            quoted += L"\\\"";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted += L"\"";
    return quoted;
}

bool FileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool DirectoryExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring ParentDirectory(std::wstring path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash);
    }
    return path;
}

struct TesseractRuntime {
    std::wstring executable;
    std::wstring tessdata;
};

std::optional<TesseractRuntime> FindBundledTesseractRuntime() {
    const std::wstring moduleDir = ModuleDirectory();
    const std::wstring exeRelative = L"\\third_party\\tesseract\\tesseract.exe";
    const std::wstring dataRelative = L"\\third_party\\tesseract\\tessdata";

    std::vector<std::wstring> bases;
    bases.push_back(moduleDir);
    bases.push_back(ParentDirectory(moduleDir));
    bases.push_back(ParentDirectory(ParentDirectory(moduleDir)));

    for (const std::wstring& base : bases) {
        TesseractRuntime runtime{base + exeRelative, base + dataRelative};
        if (FileExists(runtime.executable) && DirectoryExists(runtime.tessdata)) {
            return runtime;
        }
    }
    return std::nullopt;
}

std::wstring TesseractSearchHint() {
    const std::wstring moduleDir = ModuleDirectory();
    const std::wstring rootDir = ParentDirectory(ParentDirectory(moduleDir));
    return L"Searched:\n" +
           moduleDir + L"\\third_party\\tesseract\n" +
           rootDir + L"\\third_party\\tesseract";
}

std::optional<std::wstring> WriteTempBmp(const std::vector<uint8_t>& bytes) {
    wchar_t tempDirectory[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, tempDirectory) == 0) {
        return std::nullopt;
    }

    wchar_t tempPath[MAX_PATH]{};
    if (GetTempFileNameW(tempDirectory, L"jdt", 0, tempPath) == 0) {
        return std::nullopt;
    }

    std::wstring bmpPath = tempPath;
    bmpPath += L".bmp";
    MoveFileExW(tempPath, bmpPath.c_str(), MOVEFILE_REPLACE_EXISTING);

    HANDLE file = CreateFileW(bmpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        DeleteFileW(bmpPath.c_str());
        return std::nullopt;
    }

    DWORD written = 0;
    const BOOL ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(file);
    if (!ok || written != bytes.size()) {
        DeleteFileW(bmpPath.c_str());
        return std::nullopt;
    }
    return bmpPath;
}

std::wstring TesseractLanguageFor(const std::wstring& language) {
    if (language.empty()) return L"eng+vie";
    if (language.rfind(L"en", 0) == 0) return L"eng";
    if (language.rfind(L"vi", 0) == 0) return L"vie";
    if (language.rfind(L"ja", 0) == 0) return L"jpn";
    if (language.rfind(L"ko", 0) == 0) return L"kor";
    if (language == L"zh-Hans") return L"chi_sim";
    if (language == L"zh-Hant") return L"chi_tra";
    if (language.rfind(L"fr", 0) == 0) return L"fra";
    if (language.rfind(L"de", 0) == 0) return L"deu";
    if (language.rfind(L"es", 0) == 0) return L"spa";
    if (language.rfind(L"it", 0) == 0) return L"ita";
    if (language.rfind(L"pt", 0) == 0) return L"por";
    if (language.rfind(L"ru", 0) == 0) return L"rus";
    if (language.rfind(L"th", 0) == 0) return L"tha";
    return L"eng";
}

struct ProcessOutput {
    DWORD exitCode = 1;
    std::string output;
};

std::optional<ProcessOutput> RunProcessCaptureStdout(std::wstring commandLine) {
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
        return std::nullopt;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;

    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                        nullptr, nullptr, &startup, &process);
    CloseHandle(writePipe);
    if (!created) {
        CloseHandle(readPipe);
        return std::nullopt;
    }

    ProcessOutput result;
    char buffer[4096]{};
    DWORD read = 0;
    while (ReadFile(readPipe, buffer, static_cast<DWORD>(sizeof(buffer)), &read, nullptr) && read > 0) {
        result.output.append(buffer, buffer + read);
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &result.exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(readPipe);

    return result;
}

OcrResult RecognizeWithBundledTesseract(HBITMAP bitmap, SIZE size, const Settings& settings) {
    OcrResult output;
    const std::vector<uint8_t> bytes = HBitmapToBmpBytes(bitmap, size);
    if (bytes.empty()) {
        output.errorMessage = L"Bundled Tesseract could not prepare the screenshot for OCR.";
        return output;
    }

    auto runtime = FindBundledTesseractRuntime();
    if (!runtime) {
        output.errorMessage = L"Bundled Tesseract runtime not found.\n\n" + TesseractSearchHint();
        return output;
    }
    const std::wstring& tesseractExe = runtime->executable;
    const std::wstring& tessdataDir = runtime->tessdata;

    auto imagePath = WriteTempBmp(bytes);
    if (!imagePath) {
        output.errorMessage = L"Bundled Tesseract could not create a temporary OCR image.";
        return output;
    }

    const std::wstring language = TesseractLanguageFor(settings.ocrLanguage);
    size_t languageStart = 0;
    while (languageStart <= language.size()) {
        const size_t separator = language.find(L'+', languageStart);
        const std::wstring token = language.substr(languageStart, separator == std::wstring::npos ? std::wstring::npos : separator - languageStart);
        if (!token.empty()) {
            const std::wstring trainedData = tessdataDir + L"\\" + token + L".traineddata";
            if (!FileExists(trainedData)) {
                output.errorMessage = L"Bundled Tesseract language data not found.\n\nMissing:\n" + trainedData;
                DeleteFileW(imagePath->c_str());
                return output;
            }
        }
        if (separator == std::wstring::npos) {
            break;
        }
        languageStart = separator + 1;
    }

    const std::wstring command = Quote(tesseractExe) + L" " + Quote(*imagePath) +
                                 L" stdout -l " + language + L" --tessdata-dir " +
                                 Quote(tessdataDir) + L" --psm 6";

    auto processOutput = RunProcessCaptureStdout(command);
    DeleteFileW(imagePath->c_str());
    if (!processOutput) {
        output.errorMessage = L"Bundled Tesseract could not start.";
        return output;
    }
    if (processOutput->exitCode != 0) {
        output.errorMessage = L"Bundled Tesseract failed.";
        const std::wstring details = Trim(Utf8ToWide(processOutput->output));
        if (!details.empty()) {
            output.errorMessage += L"\n\n" + details;
        }
        return output;
    }

    output.text = Trim(Utf8ToWide(processOutput->output));
    if (output.text.empty()) {
        output.errorMessage = L"Bundled Tesseract ran, but did not find text.";
    }
    return output;
}

std::optional<CapturedBitmap> ScaleBitmapForOcr(HBITMAP sourceBitmap, SIZE sourceSize) {
    const int largestSide = std::max(sourceSize.cx, sourceSize.cy);
    const int scale = largestSide < 1800 ? 2 : 1;
    if (scale == 1) {
        return std::nullopt;
    }

    HDC screenDc = GetDC(nullptr);
    HDC sourceDc = CreateCompatibleDC(screenDc);
    HDC scaledDc = CreateCompatibleDC(screenDc);
    HBITMAP scaledBitmap = CreateCompatibleBitmap(screenDc, sourceSize.cx * scale, sourceSize.cy * scale);
    if (!sourceDc || !scaledDc || !scaledBitmap) {
        if (scaledBitmap) {
            DeleteObject(scaledBitmap);
        }
        if (scaledDc) {
            DeleteDC(scaledDc);
        }
        if (sourceDc) {
            DeleteDC(sourceDc);
        }
        ReleaseDC(nullptr, screenDc);
        return std::nullopt;
    }

    HGDIOBJ oldSource = SelectObject(sourceDc, sourceBitmap);
    HGDIOBJ oldScaled = SelectObject(scaledDc, scaledBitmap);
    SetStretchBltMode(scaledDc, HALFTONE);
    SetBrushOrgEx(scaledDc, 0, 0, nullptr);

    const BOOL ok = StretchBlt(scaledDc, 0, 0, sourceSize.cx * scale, sourceSize.cy * scale,
                               sourceDc, 0, 0, sourceSize.cx, sourceSize.cy, SRCCOPY);

    SelectObject(sourceDc, oldSource);
    SelectObject(scaledDc, oldScaled);
    DeleteDC(sourceDc);
    DeleteDC(scaledDc);
    ReleaseDC(nullptr, screenDc);

    if (!ok) {
        DeleteObject(scaledBitmap);
        return std::nullopt;
    }

    CapturedBitmap scaled;
    scaled.bitmap = scaledBitmap;
    scaled.size = {sourceSize.cx * scale, sourceSize.cy * scale};
    return scaled;
}

winrt::Windows::Media::Ocr::OcrEngine CreateWindowsOcrEngine(const Settings& settings) {
    using winrt::Windows::Globalization::Language;
    using winrt::Windows::Media::Ocr::OcrEngine;

    if (!settings.ocrLanguage.empty()) {
        Language language(settings.ocrLanguage);
        if (OcrEngine::IsLanguageSupported(language)) {
            return OcrEngine::TryCreateFromLanguage(language);
        }
    }

    auto profileEngine = OcrEngine::TryCreateFromUserProfileLanguages();
    if (profileEngine) {
        return profileEngine;
    }

    Language english(L"en-US");
    if (OcrEngine::IsLanguageSupported(english)) {
        return OcrEngine::TryCreateFromLanguage(english);
    }

    return nullptr;
}

RECT ToRect(const winrt::Windows::Foundation::Rect& rect) {
    return RECT{
        static_cast<LONG>(rect.X),
        static_cast<LONG>(rect.Y),
        static_cast<LONG>(rect.X + rect.Width),
        static_cast<LONG>(rect.Y + rect.Height),
    };
}

OcrResult BuildResult(const winrt::Windows::Media::Ocr::OcrResult& result) {
    OcrResult output;
    output.text = std::wstring(result.Text());

    for (const auto& line : result.Lines()) {
        TextBox lineBox;
        lineBox.text = std::wstring(line.Text());

        bool hasBounds = false;
        RECT bounds{};
        for (const auto& word : line.Words()) {
            TextBox wordBox;
            wordBox.text = std::wstring(word.Text());
            wordBox.bounds = ToRect(word.BoundingRect());
            output.words.push_back(wordBox);

            if (!hasBounds) {
                bounds = wordBox.bounds;
                hasBounds = true;
            } else {
                UnionRect(&bounds, &bounds, &wordBox.bounds);
            }
        }

        lineBox.bounds = bounds;
        output.lines.push_back(lineBox);
    }

    return output;
}

enum class OcrScript {
    Latin,
    Vietnamese,
    Japanese,
    Korean,
    Chinese,
    Cyrillic,
    Thai,
    Unknown,
};

struct CandidateOcrEngine {
    std::wstring tag;
    OcrScript script = OcrScript::Unknown;
    winrt::Windows::Media::Ocr::OcrEngine engine{nullptr};
};

bool IsHan(wchar_t ch) {
    return (ch >= 0x4E00 && ch <= 0x9FFF) || (ch >= 0x3400 && ch <= 0x4DBF);
}

bool IsKana(wchar_t ch) {
    return (ch >= 0x3040 && ch <= 0x30FF);
}

bool IsHangul(wchar_t ch) {
    return (ch >= 0xAC00 && ch <= 0xD7AF) || (ch >= 0x1100 && ch <= 0x11FF);
}

bool IsCyrillic(wchar_t ch) {
    return ch >= 0x0400 && ch <= 0x04FF;
}

bool IsThai(wchar_t ch) {
    return ch >= 0x0E00 && ch <= 0x0E7F;
}

bool IsLatinLetter(wchar_t ch) {
    return (ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') ||
           (ch >= 0x00C0 && ch <= 0x024F) || (ch >= 0x1E00 && ch <= 0x1EFF);
}

OcrScript ScriptForTag(std::wstring_view tag) {
    if (tag.find(L"ja") == 0) {
        return OcrScript::Japanese;
    }
    if (tag.find(L"ko") == 0) {
        return OcrScript::Korean;
    }
    if (tag.find(L"zh") == 0) {
        return OcrScript::Chinese;
    }
    if (tag.find(L"ru") == 0) {
        return OcrScript::Cyrillic;
    }
    if (tag.find(L"th") == 0) {
        return OcrScript::Thai;
    }
    if (tag.find(L"vi") == 0) {
        return OcrScript::Vietnamese;
    }
    if (tag.find(L"en") == 0 || tag.find(L"fr") == 0 || tag.find(L"de") == 0 ||
        tag.find(L"es") == 0 || tag.find(L"it") == 0 || tag.find(L"pt") == 0) {
        return OcrScript::Latin;
    }
    return OcrScript::Unknown;
}

int MatchingScriptChars(std::wstring_view text, OcrScript script) {
    int matches = 0;
    for (wchar_t ch : text) {
        switch (script) {
        case OcrScript::Japanese:
            if (IsKana(ch) || IsHan(ch)) {
                ++matches;
            }
            break;
        case OcrScript::Korean:
            if (IsHangul(ch)) {
                ++matches;
            }
            break;
        case OcrScript::Chinese:
            if (IsHan(ch)) {
                ++matches;
            }
            break;
        case OcrScript::Cyrillic:
            if (IsCyrillic(ch)) {
                ++matches;
            }
            break;
        case OcrScript::Thai:
            if (IsThai(ch)) {
                ++matches;
            }
            break;
        case OcrScript::Latin:
        case OcrScript::Vietnamese:
            if (IsLatinLetter(ch)) {
                ++matches;
            }
            break;
        case OcrScript::Unknown:
            break;
        }
    }
    return matches;
}

int ScoreOcrResult(const OcrResult& result, OcrScript script) {
    int score = static_cast<int>(result.text.size());
    score += static_cast<int>(result.words.size()) * 8;
    for (wchar_t ch : result.text) {
        if (ch == 0xFFFD || ch == L'?' || ch == L'□') {
            score -= 12;
        }
    }

    const int scriptChars = MatchingScriptChars(result.text, script);
    switch (script) {
    case OcrScript::Japanese:
    case OcrScript::Korean:
    case OcrScript::Chinese:
    case OcrScript::Cyrillic:
    case OcrScript::Thai:
        score += scriptChars * 25;
        if (!result.text.empty() && scriptChars == 0) {
            score -= 500;
        }
        break;
    case OcrScript::Vietnamese:
        score += MatchingScriptChars(result.text, OcrScript::Vietnamese) * 4;
        break;
    case OcrScript::Latin:
    case OcrScript::Unknown:
        break;
    }
    return score;
}

std::vector<CandidateOcrEngine> CreateCandidateOcrEngines(const Settings& settings) {
    using winrt::Windows::Globalization::Language;
    using winrt::Windows::Media::Ocr::OcrEngine;

    std::vector<CandidateOcrEngine> engines;
    auto addLanguage = [&](const wchar_t* tag) {
        Language language(tag);
        if (OcrEngine::IsLanguageSupported(language)) {
            auto engine = OcrEngine::TryCreateFromLanguage(language);
            if (engine) {
                engines.push_back({tag, ScriptForTag(tag), engine});
            }
        }
    };

    if (!settings.ocrLanguage.empty()) {
        addLanguage(settings.ocrLanguage.c_str());
        return engines;
    }

    auto profileEngine = OcrEngine::TryCreateFromUserProfileLanguages();
    if (profileEngine) {
        engines.push_back({L"user-profile", OcrScript::Unknown, profileEngine});
    }

    const wchar_t* commonLanguages[] = {
        L"ja-JP", L"ko-KR", L"zh-Hans", L"zh-Hant", L"ru-RU", L"th-TH",
        L"en-US", L"vi-VN", L"fr-FR", L"de-DE", L"es-ES", L"it-IT", L"pt-BR"
    };
    for (const wchar_t* language : commonLanguages) {
        addLanguage(language);
    }

    return engines;
}

}  // namespace

OcrResult OcrEngine::Recognize(const CapturedBitmap& capture, const Settings& settings) const {
    OcrResult output;
    if (!capture.bitmap || capture.size.cx <= 0 || capture.size.cy <= 0) {
        return output;
    }

    auto scaled = ScaleBitmapForOcr(capture.bitmap, capture.size);
    HBITMAP ocrBitmap = scaled ? scaled->bitmap : capture.bitmap;
    SIZE ocrSize = scaled ? scaled->size : capture.size;

    if (settings.ocrProvider == L"tesseract") {
        return RecognizeWithBundledTesseract(ocrBitmap, ocrSize, settings);
    }

    auto bytes = HBitmapToBmpBytes(ocrBitmap, ocrSize);
    if (bytes.empty()) {
        return output;
    }

    using winrt::Windows::Graphics::Imaging::BitmapAlphaMode;
    using winrt::Windows::Graphics::Imaging::BitmapDecoder;
    using winrt::Windows::Graphics::Imaging::BitmapPixelFormat;
    using winrt::Windows::Storage::Streams::DataWriter;
    using winrt::Windows::Storage::Streams::InMemoryRandomAccessStream;

    InMemoryRandomAccessStream stream;
    DataWriter writer(stream.GetOutputStreamAt(0));
    writer.WriteBytes(winrt::array_view<const uint8_t>(bytes.data(), bytes.data() + bytes.size()));
    writer.StoreAsync().get();
    writer.FlushAsync().get();
    writer.DetachStream();
    stream.Seek(0);

    auto decoder = BitmapDecoder::CreateAsync(stream).get();
    auto bitmap = decoder.GetSoftwareBitmapAsync(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied).get();
    auto engines = CreateCandidateOcrEngines(settings);
    if (engines.empty()) {
        return output;
    }

    int bestScore = -1;
    for (const auto& candidateEngine : engines) {
        OcrResult candidate = BuildResult(candidateEngine.engine.RecognizeAsync(bitmap).get());
        const int score = ScoreOcrResult(candidate, candidateEngine.script);
        if (score > bestScore) {
            bestScore = score;
            output = std::move(candidate);
        }
    }

    return output;
}

}  // namespace jd
