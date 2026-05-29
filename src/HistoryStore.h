#pragma once

#include "Common.h"

namespace jd {

class HistoryStore {
public:
    void Remember(const std::wstring& source, const TranslationResult& translation, const Settings& settings);

private:
    std::vector<std::pair<std::wstring, TranslationResult>> entries_;
};

}  // namespace jd
