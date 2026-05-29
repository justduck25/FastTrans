#include "HotkeyManager.h"

namespace jd {

bool HotkeyManager::Register(HWND hwnd) {
    return RegisterHotKey(hwnd, kHotkeyTranslateId, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, L'T') != FALSE;
}

void HotkeyManager::Unregister(HWND hwnd) {
    UnregisterHotKey(hwnd, kHotkeyTranslateId);
}

}  // namespace jd
