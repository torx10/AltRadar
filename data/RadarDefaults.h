#pragma once

#include "IconTables.h"
#include "RadarConfig.h"
#include "RadarLog.h"
#include "TargetDatabase.h"

#include <filesystem>

namespace RadarData {

inline void ApplyDefaultSettings(RadarConfig& cfg) {
    cfg = RadarConfig{};
}

inline bool ResetSettingsToDefaults(const std::filesystem::path& pluginDir, RadarConfig& cfg) {
    ApplyDefaultSettings(cfg);
    cfg.Save(pluginDir);
    RadarLog::Instance().Info("Reset to defaults: settings reloaded");
    return true;
}

inline bool ResetCustomTargets(const std::filesystem::path& pluginDir, TargetDatabase& targets) {
    std::error_code ec;
    std::filesystem::remove(pluginDir / "config" / "targets" / "user.json", ec);
    targets.Load(pluginDir);
    RadarLog::Instance().Info("Reset to defaults: custom landmarks removed");
    return true;
}

} // namespace RadarData
