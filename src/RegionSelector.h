#pragma once

#include "Common.h"

namespace jd {

class RegionSelector {
public:
    std::optional<RECT> Select(HWND owner);

    struct State {
        bool dragging = false;
        bool completed = false;
        bool cancelled = false;
        POINT start{};
        POINT current{};
        RECT virtualScreen{};
    };

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static void Paint(HWND hwnd, const State& state);
};

}  // namespace jd
