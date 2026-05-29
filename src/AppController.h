#pragma once

#include "CaptureManager.h"
#include "HistoryStore.h"
#include "HotkeyManager.h"
#include "OcrEngine.h"
#include "OverlayWindow.h"
#include "RegionSelector.h"
#include "SettingsStore.h"
#include "TranslationClient.h"

namespace jd {

class AppController {
public:
    bool Initialize(HWND hwnd);
    void Shutdown(HWND hwnd);
    void TranslateFromScreen(HWND hwnd);
    void ShowSettings(HWND owner);
    void ShowTrayMenu(HWND hwnd);
    void ShowAbout(HWND owner) const;

private:
    SettingsStore settingsStore_;
    Settings settings_;
    HotkeyManager hotkey_;
    RegionSelector selector_;
    CaptureManager capture_;
    OcrEngine ocr_;
    TranslationClient translator_;
    OverlayWindow overlay_;
    HistoryStore history_;
};

}  // namespace jd
