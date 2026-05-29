#include "CaptureManager.h"

namespace jd {

CapturedBitmap::CapturedBitmap(CapturedBitmap&& other) noexcept : bitmap(other.bitmap), size(other.size) {
    other.bitmap = nullptr;
    other.size = {};
}

CapturedBitmap& CapturedBitmap::operator=(CapturedBitmap&& other) noexcept {
    if (this != &other) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        bitmap = other.bitmap;
        size = other.size;
        other.bitmap = nullptr;
        other.size = {};
    }
    return *this;
}

CapturedBitmap::~CapturedBitmap() {
    if (bitmap) {
        DeleteObject(bitmap);
    }
}

std::optional<CapturedBitmap> CaptureManager::CaptureRegion(const RECT& region) const {
    RECT rect = NormalizeRect(region);
    if (IsEmptyRect(rect)) {
        return std::nullopt;
    }

    const int width = RectWidth(rect);
    const int height = RectHeight(rect);

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) {
        return std::nullopt;
    }

    HDC memoryDc = CreateCompatibleDC(screenDc);
    if (!memoryDc) {
        ReleaseDC(nullptr, screenDc);
        return std::nullopt;
    }

    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
    if (!bitmap) {
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return std::nullopt;
    }

    HGDIOBJ previous = SelectObject(memoryDc, bitmap);
    const BOOL copied = BitBlt(memoryDc, 0, 0, width, height, screenDc, rect.left, rect.top, SRCCOPY | CAPTUREBLT);
    SelectObject(memoryDc, previous);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    if (!copied) {
        DeleteObject(bitmap);
        return std::nullopt;
    }

    CapturedBitmap captured;
    captured.bitmap = bitmap;
    captured.size = {width, height};
    return captured;
}

}  // namespace jd
