#pragma once

#include "CaptureManager.h"

namespace jd {

class OcrEngine {
public:
    OcrResult Recognize(const CapturedBitmap& capture, const Settings& settings) const;
};

}  // namespace jd
