#include "SettingsStore.h"

#include <shlwapi.h>

namespace jd {
namespace {

std::wstring AppDataPath() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        PathRemoveFileSpecW(buffer);
    }
    return buffer;
}

std::wstring ReadIniString(const std::wstring& path, const wchar_t* section, const wchar_t* key, const wchar_t* fallback) {
    wchar_t buffer[256]{};
    GetPrivateProfileStringW(section, key, fallback, buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
    return buffer;
}

}  // namespace

SettingsStore::SettingsStore() {
    std::wstring directory = AppDataPath() + L"\\JustDuckTranslator";
    CreateDirectoryW(directory.c_str(), nullptr);
    path_ = directory + L"\\settings.ini";
}

Settings SettingsStore::Load() const {
    Settings settings;
    settings.targetLanguage = ReadIniString(path_, L"Translation", L"TargetLanguage", L"vi");
    settings.ocrProvider = ReadIniString(path_, L"OCR", L"Provider", L"windows");
    settings.ocrLanguage = ReadIniString(path_, L"OCR", L"Language", L"");
    settings.saveHistory = GetPrivateProfileIntW(L"Privacy", L"SaveHistory", 0, path_.c_str()) != 0;
    return settings;
}

void SettingsStore::Save(const Settings& settings) const {
    WritePrivateProfileStringW(L"Translation", L"TargetLanguage", settings.targetLanguage.c_str(), path_.c_str());
    WritePrivateProfileStringW(L"OCR", L"Provider", settings.ocrProvider.c_str(), path_.c_str());
    WritePrivateProfileStringW(L"OCR", L"Language", settings.ocrLanguage.c_str(), path_.c_str());
    WritePrivateProfileStringW(L"Privacy", L"SaveHistory", settings.saveHistory ? L"1" : L"0", path_.c_str());
}

}  // namespace jd
