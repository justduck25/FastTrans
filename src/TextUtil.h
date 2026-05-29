#pragma once

#include "Common.h"

namespace jd {

std::string WideToUtf8(std::wstring_view value);
std::wstring Utf8ToWide(std::string_view value);
std::wstring Trim(std::wstring value);
std::wstring NormalizeOcrText(std::wstring_view value);
bool LooksVietnamese(std::wstring_view value);
bool LooksMostlyEnglish(std::wstring_view value);
bool LooksVietnameseOcrText(std::wstring_view value);
bool IsVietnameseTarget(std::wstring_view language);

}  // namespace jd
