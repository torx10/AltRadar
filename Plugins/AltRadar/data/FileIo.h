#pragma once

#include "RadarLog.h"

#include <filesystem>
#include <fstream>
#include <string_view>

namespace RadarData {

// Writes <path>.tmp and renames it over path, so a crash mid-save never leaves a half-written file.
inline bool WriteFileAtomic(const std::filesystem::path& path, std::string_view text) {
    auto tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.close();
        if (!out) return false;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (!ec) return true;
    std::filesystem::remove(tmp, ec);
    return false;
}

// The next save overwrites a file we could not read, so keep the user's copy as <path>.bad.
inline void BackupUnreadableFile(const std::filesystem::path& path) {
    auto bad = path;
    bad += ".bad";
    std::error_code ec;
    std::filesystem::copy_file(path, bad, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
        RadarLog::Instance().Warn("Could not back up unreadable " + path.string() + ": " + ec.message());
    else
        RadarLog::Instance().Warn("Kept a copy of unreadable " + path.string() + " as " + bad.string());
}

} // namespace RadarData
