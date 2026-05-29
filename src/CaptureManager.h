#pragma once

#include "Common.h"

namespace jd {

struct CapturedBitmap {
    HBITMAP bitmap = nullptr;
    SIZE size{};

    CapturedBitmap() = default;
    CapturedBitmap(const CapturedBitmap&) = delete;
    CapturedBitmap& operator=(const CapturedBitmap&) = delete;

    CapturedBitmap(CapturedBitmap&& other) noexcept;
    CapturedBitmap& operator=(CapturedBitmap&& other) noexcept;
    ~CapturedBitmap();
};

class CaptureManager {
public:
    std::optional<CapturedBitmap> CaptureRegion(const RECT& region) const;
};

}  // namespace jd
