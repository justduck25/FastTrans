#pragma once

#include "Common.h"

namespace jd {

class TranslationClient {
public:
    std::optional<TranslationResult> Translate(const std::wstring& text, const std::wstring& targetLanguage);

private:
    std::map<std::wstring, TranslationResult> cache_;
};

}  // namespace jd
