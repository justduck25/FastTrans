#pragma once

#include "Common.h"

namespace jd {

class HotkeyManager {
public:
    bool Register(HWND hwnd);
    void Unregister(HWND hwnd);
};

}  // namespace jd
