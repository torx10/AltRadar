#pragma once

#include <limits>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RadarData {

inline std::string WideToUtf8(const std::wstring& text) {
#ifdef _WIN32
    if (text.empty() || text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
#else
    return std::string(text.begin(), text.end());  // self-test only; game names arrive on Windows
#endif
}

} // namespace RadarData
