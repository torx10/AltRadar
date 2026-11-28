// Alt Radar plugin — fork of the official Radar overlay.

#include "plugin_sdk/PluginSDK.h"

#include "data/DisplayRulesStore.h"
#include "data/RadarDefaults.h"
#include "data/RadarLog.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
#include "render/RadarOverlay.h"
#include "ui/RadarUi.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr float kPickerRadiusPx = 14.f;
constexpr float kPickerRadiusSq = kPickerRadiusPx * kPickerRadiusPx;

std::string PathBaseName(const std::string& path) {
    if (const size_t slash = path.find_last_of('/'); slash != std::string::npos)
        return path.substr(slash + 1);
    return path;
}

// A room's short name: its file name without folder or extension.
std::string RoomShortName(const std::string& name) {
    std::string base = PathBaseName(name);
    if (const size_t dot = base.find_last_of('.'); dot != std::string::npos) base.resize(dot);
    return base;
}

// q is convex and its corners go round in one direction.
bool PointInQuad(const ImVec2& p, const ImVec2 (&q)[4]) {
    int sign = 0;
    for (int i = 0; i < 4; ++i) {
        const ImVec2& a = q[i];
        const ImVec2& b = q[(i + 1) % 4];
        const float cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        const int s = cross > 0.f ? 1 : (cross < 0.f ? -1 : 0);
        if (s == 0) continue;
        if (sign == 0) sign = s;
        else if (s != sign) return false;
    }
    return true;
}

void EnsureRuntimeDirectories(const std::filesystem::path& pluginDir) {
    std::error_code ec;
    std::filesystem::create_directories(pluginDir / "logs", ec);
    ec.clear();
    std::filesystem::create_directories(pluginDir / "config", ec);
    ec.clear();
    std::filesystem::create_directories(pluginDir / "config" / "targets", ec);
}

} // namespace

class RadarPlugin : public PluginSDK::Plugin {
public:
    const char* GetName() const override { return "Alt Radar"; }

    bool WantsOverlay() const override { return m_overlay.cfg.OverlayEnabled; }

    void OnEnable(bool /*isGameAttached*/) override {
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        const auto pluginDir = DirectoryPath();
        EnsureRuntimeDirectories(pluginDir);
        m_overlay.pluginDir = pluginDir;
        m_overlay.runeIcons.SetPluginDir(pluginDir);
        RadarData::RadarLog::Instance().Init(pluginDir);
        const auto generatedTargets = RadarData::TargetDatabase::EnsureEmbeddedDefaultsWritten(pluginDir);
        RadarData::RadarLog::Instance().Info("Alt Radar pluginDir: " + pluginDir.string());
        RadarData::RadarLog::Instance().Info("Alt Radar targetDir: " + generatedTargets.targetDir.string());
        RadarData::RadarLog::Instance().Info(std::string("Target defaults generated: acts=")
                                             + (generatedTargets.actsGenerated ? "yes" : "no")
                                             + " endgame="
                                             + (generatedTargets.endgameGenerated ? "yes" : "no")
                                             + " ignore="
                                             + (generatedTargets.ignoreGenerated ? "yes" : "no"));

        std::error_code ec;
        const bool hadSettings = std::filesystem::exists(pluginDir / "config" / "settings.json", ec);
        m_overlay.cfg.Load(pluginDir);
        if (!hadSettings) m_overlay.cfg.Save(pluginDir);
        m_overlay.icons.Load(pluginDir);
        try {
            RadarData::DisplayRulesStore::Load(pluginDir, m_overlay.icons.displayRules);
        } catch (const std::exception& ex) {
            const auto path = RadarData::DisplayRulesStore::PathFor(pluginDir);
            RadarData::RadarLog::Instance().Warn("display_rules load failed for " + path.string() + ": " + ex.what());
        } catch (...) {
            const auto path = RadarData::DisplayRulesStore::PathFor(pluginDir);
            RadarData::RadarLog::Instance().Warn("display_rules load failed for " + path.string() + ": unknown error");
        }
        m_overlay.targets.Load(pluginDir);
        m_overlay.roomTargets.Load(pluginDir);
        if (const size_t moved = m_overlay.roomTargets.Adopt(m_overlay.targets)) {
            m_overlay.roomTargets.Save(pluginDir);
            m_overlay.targets.SaveUser(pluginDir);
            RadarData::RadarLog::Instance().Info("Moved " + std::to_string(moved)
                                                 + " room targets from user.json to rooms.json");
        }
        m_overlay.cache.InvalidatePoi();
        m_overlay.walkable = ctx()->Terrain.GetWalkableGrid();

        RadarData::RadarLog::Instance().Info("Alt Radar plugin enabled");
        ctx()->Log.Info("Alt Radar plugin enabled — see logs/altradar.log in plugin folder");
    }

    void OnDisable() override {
        const auto pluginDir = DirectoryPath();
        if (m_ui.displayRulesDirty) {
            RadarData::DisplayRulesStore::EnsureStableIds(m_overlay.icons.displayRules);
            std::string note;
            if (RadarData::DisplayRulesStore::Save(pluginDir, m_overlay.icons.displayRules, note)) {
                RadarData::RadarLog::Instance().Info(note);
                m_ui.displayRulesDirty = false;
            } else {
                RadarData::RadarLog::Instance().Warn(note);
            }
        }
        EndPickerMode();
        SetSleepingScan(false);
        m_overlay.paths.Release();  // stops the path worker
        m_overlay.walkable.Reset();
        m_overlay.terrain.Release();
        m_overlay.runeIcons.Release();
        m_overlay.cache.Clear();
        RadarData::RadarLog::Instance().Info("Alt Radar plugin disabled");
        RadarData::RadarLog::Instance().Shutdown();
        ctx()->Log.Info("Alt Radar plugin disabled");
    }

    void DrawSettings() override {
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        const auto pluginDir = DirectoryPath();
        m_overlay.pluginDir = pluginDir;
        m_overlay.runeIcons.SetPluginDir(pluginDir);
        if (m_ui.requestResetSettings) {
            m_ui.requestResetSettings = false;
            RadarData::ResetSettingsToDefaults(pluginDir, m_overlay.cfg);
            m_overlay.cache.Clear();
            m_overlay.cache.InvalidatePoi();
            RadarData::RadarLog::Instance().Info("General settings reset to defaults");
            ctx()->Log.Info("Alt Radar general settings reset to defaults");
        }
        if (m_ui.requestResetCustomLandmarks) {
            m_ui.requestResetCustomLandmarks = false;
            RadarData::ResetCustomTargets(pluginDir, m_overlay.targets);
            m_overlay.cache.Clear();
            m_overlay.cache.InvalidatePoi();
            RadarData::RadarLog::Instance().Info("Custom landmarks reset");
            ctx()->Log.Info("Alt Radar custom landmarks reset");
        }
        if (m_ui.requestResetLandmarkOverrides) {
            m_ui.requestResetLandmarkOverrides = false;
            RadarData::ResetTargetOverrides(pluginDir, m_overlay.targets);
            m_overlay.cache.Clear();
            m_overlay.cache.InvalidatePoi();
            RadarData::RadarLog::Instance().Info("Landmark overrides reset");
            ctx()->Log.Info("Alt Radar landmark overrides reset");
        }

        const auto snap = m_ui.rulePickerOpen ? ctx()->Game.GetSnapshot()
                                              : ctx()->Game.GetSnapshot(static_cast<PluginSDK::EntityType>(0x7FFFFFFF));
        const bool perfCaptureEnabled =
            m_overlay.cfg.EnablePerformanceDebug && m_overlay.cfg.EnablePerfTimingCapture;
        const auto settingsStart = perfCaptureEnabled ? std::optional(std::chrono::steady_clock::now())
                                                      : std::nullopt;
        RadarUi::DrawSettings(m_overlay, m_ui, const_cast<PluginSDK::Context*>(ctx()), snap, pluginDir);
        if (settingsStart) {
            m_overlay.perf.RecordExternal(
                RadarPerf::OverlayPerfTiming::Section::SettingsDebugUi,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - *settingsStart)
                    .count(),
                perfCaptureEnabled, true);
        }
    }

    void DrawUI() override {
        if (!m_overlay.cfg.OverlayEnabled || !ctx()->Game.IsInGame()) {
            SetSleepingScan(false);
            return;
        }
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        const bool picking = m_ui.pickerPoiMode || m_ui.pickerEntityMode || m_ui.pickerRoomMode;
        const auto& snap = FrameSnapshot(picking);
        m_overlay.runeIcons.SetPluginDir(DirectoryPath());

        if (picking) {
            DrawPicker(snap);  // the picker reads sleeping entities itself
            return;
        }
        UpdateWispArea(snap);
        SetSleepingScan(m_overlay.cfg.ScanFarWisps && m_wispArea);
        m_overlay.Draw(const_cast<PluginSDK::Context*>(ctx()), snap);
        if (m_overlay.cfg.ShowWispCounter) RadarUi::DrawWispCounter(m_overlay.cache.entities.wispCounts);
    }

    void SaveSettings() override {
        const auto dir = DirectoryPath();
        m_overlay.cfg.Save(dir);
        m_overlay.icons.Save(dir);
        RadarData::DisplayRulesStore::EnsureStableIds(m_overlay.icons.displayRules);
        std::string displayRulesNote;
        if (RadarData::DisplayRulesStore::Save(dir, m_overlay.icons.displayRules, displayRulesNote)) {
            RadarData::RadarLog::Instance().Info(displayRulesNote);
            m_ui.displayRulesDirty = false;
        } else {
            RadarData::RadarLog::Instance().Warn(displayRulesNote);
        }
        m_overlay.targets.SaveUser(dir);
        m_overlay.roomTargets.Save(dir);
    }

private:
    RadarRender::RadarOverlay m_overlay;
    RadarUi::UiState          m_ui;
    // Far wisp deposits are sleeping entities. Like the FixerWiki Radar, scan them in the Wildwood
    // and Flotsam, and on any other map once a deposit has been seen up close.
    bool     m_wispArea = false;
    uint64_t m_wispAreaCounter = UINT64_MAX;
    bool     m_sleepingScan = false;

    // GetSnapshot() materializes every entity's strings (host calls + allocations), which DrawUI
    // cannot afford each frame (the host's watchdog disables a plugin whose DrawUI is consistently
    // over 50 ms). Fetch the entity list at most every EntityUpdateIntervalMs, or when forced; other
    // frames take a light snapshot (player, maps, area) and keep the last entity list with its
    // LastUpdateTime, so the entity cache sees nothing new until the next full fetch.
    PluginSDK::Snapshot                   m_snap;
    std::chrono::steady_clock::time_point m_lastFullSnapshot{};

    const PluginSDK::Snapshot& FrameSnapshot(bool forceEntities) {
        const auto start = std::chrono::steady_clock::now();
        // No entity has this type, and the SDK filters before materializing strings.
        auto light = ctx()->Game.GetSnapshot(static_cast<PluginSDK::EntityType>(0x7FFFFFFF));
        const bool due = forceEntities || light.AreaChangeCounter != m_snap.AreaChangeCounter
                         || (light.LastUpdateTime != m_snap.LastUpdateTime
                             && start - m_lastFullSnapshot
                                    >= std::chrono::milliseconds(m_overlay.cfg.EntityUpdateIntervalMs));
        if (due) {
            m_snap = ctx()->Game.GetSnapshot();
            m_lastFullSnapshot = start;
        } else {
            auto entities = std::move(m_snap.Entities);
            const auto entitiesTime = m_snap.LastUpdateTime;
            m_snap = std::move(light);
            m_snap.Entities = std::move(entities);
            m_snap.LastUpdateTime = entitiesTime;
        }
        const bool capture = m_overlay.cfg.EnablePerformanceDebug && m_overlay.cfg.EnablePerfTimingCapture;
        m_overlay.perf.RecordExternal(
            RadarPerf::OverlayPerfTiming::Section::SnapshotFetch,
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(), capture,
            true);
        return m_snap;
    }

    void UpdateWispArea(const PluginSDK::Snapshot& snap) {
        if (snap.AreaChangeCounter != m_wispAreaCounter) {
            m_wispAreaCounter = snap.AreaChangeCounter;
            const std::string area = m_overlay.targets.ResolveAreaKey(snap.CurrentAreaHash, snap.CurrentAreaName);
            m_wispArea = area == "MAPUNIQUEWILDWOOD" || area == "MAPFLOTSAM";  // WorldAreas ids, 0.5.5
        }
        if (!m_wispArea)
            m_wispArea = std::any_of(snap.Entities.begin(), snap.Entities.end(), [](const PluginSDK::Entity& e) {
                return e.EntityType == PluginSDK::EntityType::AzmeriWisp;
            });
    }

    void SetSleepingScan(bool on) {
        if (on == m_sleepingScan) return;
        ctx()->Overlay.SetIncludeSleepingEntities(on);
        m_sleepingScan = on;
    }

    void EndPickerMode() {
        if (!m_ui.pickerPoiMode && !m_ui.pickerEntityMode && !m_ui.pickerRoomMode) return;
        m_ui.pickerPoiMode = false;
        m_ui.pickerEntityMode = false;
        m_ui.pickerRoomMode = false;
        m_ui.pickerForIgnore = false;
        ctx()->Overlay.SetIncludeSleepingEntities(false);
        m_sleepingScan = false;  // the next frame turns the wisp scan back on if wanted
        ctx()->Overlay.SetWantsOverlayInput(false);
    }

    void DrawPicker(const PluginSDK::Snapshot& snap) {
        ctx()->Overlay.SetIncludeSleepingEntities(true);
        ctx()->Overlay.SetWantsOverlayInput(true);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            EndPickerMode();
            return;
        }

        ImVec2 screenSize(static_cast<float>(snap.ScreenWidth),
                          static_cast<float>(snap.ScreenHeight));
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(screenSize);
        ImGui::Begin("##radar_picker", nullptr,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                          | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground
                          | ImGuiWindowFlags_NoScrollbar);
        ImGui::InvisibleButton("##hit", screenSize);
        const bool clicked = ImGui::IsItemClicked();
        const ImVec2 mouse = ImGui::GetMousePos();
        ImGui::End();

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl) return;

        if (m_ui.pickerPoiMode) {
            auto* gameCtx = const_cast<PluginSDK::Context*>(ctx());
            struct Hit { PluginSDK::TgtLocation loc; ImVec2 pos; float distSq; };
            std::vector<Hit> hits;

            ctx()->Terrain.EnumerateTgtLocations([&](const PluginSDK::TgtLocation& loc) {
                const float tz = ctx()->Terrain.GetTerrainHeight(static_cast<int>(loc.X),
                                                                 static_cast<int>(loc.Y));
                const auto scr =
                    RadarRender::ProjectGridToScreen(gameCtx, snap, loc.X, loc.Y, tz);
                if (!scr.valid) return true;
                const float dx = mouse.x - scr.sx;
                const float dy = mouse.y - scr.sy;
                const float distSq = dx * dx + dy * dy;
                if (distSq < kPickerRadiusSq) hits.push_back({loc, ImVec2(scr.sx, scr.sy), distSq});
                return true;
            });

            const Hit* nearestHit = nullptr;
            for (const auto& h : hits)
                if (!nearestHit || h.distSq < nearestHit->distSq) nearestHit = &h;
            const PluginSDK::TgtLocation* nearest = nearestHit ? &nearestHit->loc : nullptr;

            if (nearest) {
                for (const auto& h : hits) {
                    const bool isNearest = &h == nearestHit;
                    dl->AddCircleFilled(h.pos, isNearest ? 9.f : 6.f,
                                        isNearest ? IM_COL32(0, 255, 255, 255)
                                                  : IM_COL32(255, 255, 0, 120));
                }
                dl->AddText(ImVec2(12.f, 32.f), IM_COL32(200, 255, 200, 255),
                            nearest->Path.c_str());
            }

            if (clicked && nearest) {
                m_ui.editTarget = {};
                m_ui.editTarget.path = nearest->Path;
                m_ui.editTarget.name = PathBaseName(nearest->Path);
                m_ui.editTarget.category = "User";
                m_ui.editTarget.enabled = true;
                m_ui.editTarget.showIcon = false;
                m_ui.editTarget.hasAnchor = true;
                m_ui.editTarget.anchorGridX = nearest->X;
                m_ui.editTarget.anchorGridY = nearest->Y;
                m_ui.editTarget.anchorTileX = nearest->TileX;
                m_ui.editTarget.anchorTileY = nearest->TileY;
                m_ui.editAreaKey = m_overlay.targets.ResolveAreaKey(snap.CurrentAreaHash,
                                                                    snap.CurrentAreaName);
                m_ui.editGlobal = false;
                m_ui.editIsNew = true;
                m_ui.editStorageIndex = SIZE_MAX;
                m_ui.editModalOpen = true;
                EndPickerMode();
            }
        }

        if (m_ui.pickerEntityMode) {
            auto* gameCtx = const_cast<PluginSDK::Context*>(ctx());
            struct Hit { const PluginSDK::Entity* ent; ImVec2 pos; float distSq; };
            std::vector<Hit> hits;

            for (const auto& e : snap.Entities) {
                if (!e.IsValid) continue;
                if (e.EntityState == PluginSDK::EntityState::Useless) continue;
                if (e.EntityType != PluginSDK::EntityType::Monster
                    && e.EntityType != PluginSDK::EntityType::Chest
                    && e.EntityType != PluginSDK::EntityType::NPC)
                    continue;
                const auto scr = RadarRender::ProjectEntityToScreen(gameCtx, snap, e);
                if (!scr.valid) continue;
                const float dx = mouse.x - scr.sx;
                const float dy = mouse.y - scr.sy;
                const float distSq = dx * dx + dy * dy;
                if (distSq < kPickerRadiusSq) hits.push_back({&e, ImVec2(scr.sx, scr.sy), distSq});
            }

            const Hit* nearestHit = nullptr;
            for (const auto& h : hits)
                if (!nearestHit || h.distSq < nearestHit->distSq) nearestHit = &h;
            const PluginSDK::Entity* nearestEnt = nearestHit ? nearestHit->ent : nullptr;

            if (nearestEnt) {
                for (const auto& h : hits) {
                    const bool isNearest = &h == nearestHit;
                    dl->AddCircleFilled(h.pos, isNearest ? 8.f : 5.f,
                                        isNearest ? IM_COL32(255, 128, 0, 255)
                                                  : IM_COL32(200, 200, 200, 120));
                }
                const std::string path = RadarUi::WideToUtf8(nearestEnt->Path);
                dl->AddText(ImVec2(12.f, 32.f), IM_COL32(255, 200, 150, 255), path.c_str());
            }

            if (clicked && nearestEnt && m_ui.pickerForIgnore) {
                const std::string path = RadarUi::WideToUtf8(nearestEnt->Path);  // exact; widen it in the Ignore tab
                m_overlay.targets.AddIgnore(PathBaseName(path), path);
                m_overlay.targets.SaveIgnore(DirectoryPath());
                m_overlay.cache.InvalidatePoi();
                EndPickerMode();
                return;
            }
            if (clicked && nearestEnt) {
                m_ui.editTarget = {};
                std::string path = RadarUi::WideToUtf8(nearestEnt->Path);
                if (path.find('*') == std::string::npos) path += '*';
                m_ui.editTarget.path = path;
                m_ui.editTarget.name = PathBaseName(path);
                m_ui.editTarget.category = "User";
                m_ui.editTarget.enabled = true;
                m_ui.editTarget.showIcon = true;
                m_ui.editTarget.expectedCount = 1;
                m_ui.editAreaKey = m_overlay.targets.ResolveAreaKey(snap.CurrentAreaHash,
                                                                    snap.CurrentAreaName);
                m_ui.editGlobal = false;
                m_ui.editIsNew = true;
                m_ui.editStorageIndex = SIZE_MAX;
                m_ui.editModalOpen = true;
                EndPickerMode();
            }
        }

        if (m_ui.pickerRoomMode) DrawRoomPicker(snap, dl, mouse, clicked);

        dl->AddText(ImVec2(12, 12), IM_COL32(255, 255, 255, 255),
                    m_ui.pickerPoiMode
                        ? "Click nearest yellow marker (cyan = selected, Esc to cancel)"
                        : m_ui.pickerRoomMode ? "Click a room to mark rooms like it (Esc to cancel)"
                        : m_ui.pickerForIgnore ? "Click an entity to ignore it (Esc to cancel)"
                                              : "Click nearest entity marker (Esc to cancel)");
    }

    // Outlines every room of the area on the map; clicking one opens the editor for a room target.
    void DrawRoomPicker(const PluginSDK::Snapshot& snap, ImDrawList* dl, const ImVec2& mouse,
                        bool clicked) {
        const auto rooms = ctx()->Terrain.GetAreaRooms();
        const char* status = !rooms.Supported      ? "This POE2Fixer version does not report rooms."
                             : !rooms.Ready         ? "Rooms are still loading..."
                             : !snap.LargeMap.IsVisible ? "Open the large map to pick a room."
                                                         : nullptr;
        if (status) {
            dl->AddText(ImVec2(12.f, 32.f), IM_COL32(255, 200, 120, 255), status);
            return;
        }
        constexpr float kTileCells = 23.f;  // one tile spans 23 grid cells (PluginSDK::AreaRoom)
        auto* gameCtx = const_cast<PluginSDK::Context*>(ctx());
        const PluginSDK::AreaRoom* hovered = nullptr;
        ImVec2 hoveredQuad[4];
        float hoveredDistSq = 0.f;
        for (const auto& room : rooms.Rooms) {
            const float z = ctx()->Terrain.GetTerrainHeight(static_cast<int>(room.GridX),
                                                           static_cast<int>(room.GridY));
            const float x0 = room.MinTileX * kTileCells, y0 = room.MinTileY * kTileCells;
            const float x1 = room.MaxTileX * kTileCells, y1 = room.MaxTileY * kTileCells;
            const float corners[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
            ImVec2 quad[4];
            bool onScreen = true;
            for (int i = 0; i < 4 && onScreen; ++i) {
                const auto s = RadarRender::ProjectGridToScreen(gameCtx, snap, corners[i][0], corners[i][1], z);
                onScreen = s.valid;
                quad[i] = ImVec2(s.sx, s.sy);
            }
            const auto centre = RadarRender::ProjectGridToScreen(gameCtx, snap, room.GridX, room.GridY, z);
            if (!onScreen || !centre.valid) continue;
            dl->AddPolyline(quad, 4, IM_COL32(255, 255, 0, 110), ImDrawFlags_Closed, 1.f);
            const std::string label = RoomShortName(room.Name);
            const ImVec2 size = ImGui::CalcTextSize(label.c_str());
            dl->AddText(ImVec2(centre.sx - size.x * 0.5f, centre.sy - size.y * 0.5f),
                        IM_COL32(255, 255, 160, 200), label.c_str());
            if (!PointInQuad(mouse, quad)) continue;
            const float dx = mouse.x - centre.sx, dy = mouse.y - centre.sy;
            if (hovered && dx * dx + dy * dy >= hoveredDistSq) continue;  // nested rooms: nearest centre wins
            hovered = &room;
            hoveredDistSq = dx * dx + dy * dy;
            std::copy(std::begin(quad), std::end(quad), hoveredQuad);
        }
        if (!hovered) return;

        dl->AddPolyline(hoveredQuad, 4, IM_COL32(0, 255, 255, 255), ImDrawFlags_Closed, 2.5f);
        ImGui::SetTooltip("%s\ntiles %d,%d - %d,%d  centre %.0f, %.0f", hovered->Name.c_str(),
                          hovered->MinTileX, hovered->MinTileY, hovered->MaxTileX, hovered->MaxTileY,
                          hovered->GridX, hovered->GridY);
        if (!clicked) return;
        const std::string area = m_overlay.targets.ResolveAreaKey(snap.CurrentAreaHash, snap.CurrentAreaName);
        RadarData::RoomTarget room;
        room.pattern = hovered->Name;  // the exact room; widen it with * in the editor
        room.category = m_overlay.targets.IsEndgameArea(area, snap.CurrentAreaLevel) ? "Endgame" : "Acts";
        room.area = area;
        room.look.name = RoomShortName(hovered->Name);
        room.look.category = "User";
        room.look.showIcon = true;
        room.look.expectedCount = 0;
        m_ui.editRoom = room;
        m_ui.editRoomIndex = SIZE_MAX;
        m_ui.editRoomGlobal = false;
        m_ui.editRoomOpen = true;
        EndPickerMode();
    }
};

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new RadarPlugin(); }

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* p) { delete p; }

// Required by POE2Fixer since 2026-10-22: the host checks this before CreatePlugin.
// String literals only: the POEFixer publish tool reads this block as text.
// Keep the version equal to kVersion in data/AltRadarVersion.h.
POEFIXER_PLUGIN_INFO(
    "AltRadar",
    "Alt Radar",
    "0.1.0",
    "torx10",
    "Radar overlay for the in-game large map: monsters, NPCs, chests, POI landmarks and the walkable map.")
