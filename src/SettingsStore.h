#pragma once

#include "Common.h"

namespace jd {

class SettingsStore {
public:
    SettingsStore();

    Settings Load() const;
    void Save(const Settings& settings) const;
    const std::wstring& Path() const { return path_; }

private:
    std::wstring path_;
};

}  // namespace jd
