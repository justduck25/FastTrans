#include "HistoryStore.h"

namespace jd {

void HistoryStore::Remember(const std::wstring& source, const TranslationResult& translation, const Settings& settings) {
    if (!settings.saveHistory || source.empty() || translation.translatedText.empty()) {
        return;
    }
    entries_.push_back({source, translation});
    if (entries_.size() > 100) {
        entries_.erase(entries_.begin());
    }
}

}  // namespace jd
