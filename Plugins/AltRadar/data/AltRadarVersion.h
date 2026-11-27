#pragma once

// A macro so POEFIXER_PLUGIN_INFO in Radar.cpp can use it (it needs a string literal).
#define ALTRADAR_VERSION "0.1.0"

namespace AltRadar {

inline constexpr const char* kVersion = ALTRADAR_VERSION;
inline constexpr const char* kVersionLabel = "AltRadar v" ALTRADAR_VERSION;

} // namespace AltRadar
