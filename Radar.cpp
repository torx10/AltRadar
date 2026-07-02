// Alt Radar plugin — fork of the official Radar overlay.

#include "sdk/PluginSDK.h"

#include "data/Migration.h"
#include "data/DisplayRulesStore.h"
#include "data/RadarDefaults.h"
#include "data/RadarLog.h"
#include "render/RadarOverlay.h"
#include "ui/RadarUi.h"

#include <imgui.h>
#include <cmath>
#include <optional>
#include <string>

namespace {

constexpr float kPickerRadiusPx = 14.f;
constexpr float kPickerRadiusSq = kPickerRadiusPx * kPickerRadiusPx;

std::string PathBaseName(const std::string& path) {
    if (const size_t slash = path.find_last_of('/'); slash != std::string::npos)
        return path.substr(slash + 1);
    return path;
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
        RadarData::RadarLog::Instance().Init(pluginDir);

        const auto hostDir = pluginDir.parent_path().parent_path();
        RadarData::TryMigrateFromHost(pluginDir, hostDir);

        m_overlay.cfg.Load(pluginDir);
        m_overlay.icons.Load(pluginDir);
        RadarData::DisplayRulesStore::Load(pluginDir, m_overlay.icons.displayRules);
        m_overlay.targets.Load(pluginDir);
        if (RadarData::TargetDatabase::SyncBundledTargetsFromHost(pluginDir, hostDir, true,
                                                                &m_overlay.targets))
            m_overlay.cache.InvalidatePoi();
        m_overlay.walkable = ctx()->Terrain.GetWalkableGrid();
        m_overlay.EnsureAtlas(const_cast<PluginSDK::Context*>(ctx()), pluginDir);

        RadarData::RadarLog::Instance().Info("Alt Radar plugin enabled");
        ctx()->Log.Info("Alt Radar plugin enabled — see logs/altradar.log in plugin folder");
    }

    void OnDisable() override {
        EndPickerMode();
        m_overlay.walkable.Reset();
        m_overlay.terrain.Release();
        m_overlay.atlas.Release();
        m_overlay.cache.Clear();
        RadarData::RadarLog::Instance().Info("Alt Radar plugin disabled");
        RadarData::RadarLog::Instance().Shutdown();
        ctx()->Log.Info("Alt Radar plugin disabled");
    }

    void DrawSettings() override {
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        const auto pluginDir = DirectoryPath();
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

        const auto snap = ctx()->Game.GetSnapshot();
        m_overlay.EnsureAtlas(const_cast<PluginSDK::Context*>(ctx()), pluginDir);
        RadarUi::DrawSettings(m_overlay, m_ui, const_cast<PluginSDK::Context*>(ctx()), snap, pluginDir);
    }

    void DrawUI() override {
        if (!m_overlay.cfg.OverlayEnabled) return;
        if (!ctx()->Game.IsInGame()) return;
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        const auto snap = ctx()->Game.GetSnapshot();

        if (m_ui.pickerPoiMode || m_ui.pickerEntityMode) {
            DrawPicker(snap);
            return;
        }

        m_overlay.EnsureAtlas(const_cast<PluginSDK::Context*>(ctx()), DirectoryPath());
        m_overlay.Draw(const_cast<PluginSDK::Context*>(ctx()), snap);
    }

    void SaveSettings() override {
        const auto dir = DirectoryPath();
        m_overlay.cfg.Save(dir);
        m_overlay.icons.Save(dir);
        RadarData::DisplayRulesStore::Save(dir, m_overlay.icons.displayRules);
        m_overlay.targets.SaveUser(dir);
    }

private:
    RadarRender::RadarOverlay m_overlay;
    RadarUi::UiState          m_ui;

    void EndPickerMode() {
        if (!m_ui.pickerPoiMode && !m_ui.pickerEntityMode) return;
        m_ui.pickerPoiMode = false;
        m_ui.pickerEntityMode = false;
        ctx()->Overlay.SetIncludeSleepingEntities(false);
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
            std::optional<PluginSDK::TgtLocation> nearest;
            float nearestDistSq = kPickerRadiusSq;

            ctx()->Terrain.EnumerateTgtLocations([&](const PluginSDK::TgtLocation& loc) {
                const float tz = ctx()->Terrain.GetTerrainHeight(static_cast<int>(loc.X),
                                                                 static_cast<int>(loc.Y));
                const auto scr =
                    RadarRender::ProjectGridToScreen(gameCtx, snap, loc.X, loc.Y, tz);
                if (!scr.valid) return true;
                const float dx = mouse.x - scr.sx;
                const float dy = mouse.y - scr.sy;
                const float distSq = dx * dx + dy * dy;
                if (distSq < nearestDistSq) {
                    nearestDistSq = distSq;
                    nearest = loc;
                }
                return true;
            });

            if (nearest) {
                ctx()->Terrain.EnumerateTgtLocations([&](const PluginSDK::TgtLocation& loc) {
                    const auto scr =
                        RadarRender::ProjectGridToScreen(gameCtx, snap, loc.X, loc.Y, 0.f);
                    if (!scr.valid) return true;
                    const float dx = mouse.x - scr.sx;
                    const float dy = mouse.y - scr.sy;
                    const float distSq = dx * dx + dy * dy;
                    if (distSq >= kPickerRadiusSq) return true;
                    const bool isNearest = loc.Path == nearest->Path && loc.X == nearest->X
                                           && loc.Y == nearest->Y;
                    dl->AddCircleFilled(ImVec2(scr.sx, scr.sy), isNearest ? 9.f : 6.f,
                                        isNearest ? IM_COL32(0, 255, 255, 255)
                                                  : IM_COL32(255, 255, 0, 120));
                    return true;
                });
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
                m_ui.editIsNew = true;
                m_ui.editStorageIndex = SIZE_MAX;
                m_ui.editModalOpen = true;
                EndPickerMode();
            }
        }

        if (m_ui.pickerEntityMode) {
            auto* gameCtx = const_cast<PluginSDK::Context*>(ctx());
            const PluginSDK::Entity* nearestEnt = nullptr;
            float nearestDistSq = kPickerRadiusSq;

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
                if (distSq < nearestDistSq) {
                    nearestDistSq = distSq;
                    nearestEnt = &e;
                }
            }

            if (nearestEnt) {
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
                    if (distSq >= kPickerRadiusSq) continue;
                    const bool isNearest = &e == nearestEnt;
                    dl->AddCircleFilled(ImVec2(scr.sx, scr.sy), isNearest ? 8.f : 5.f,
                                        isNearest ? IM_COL32(255, 128, 0, 255)
                                                  : IM_COL32(200, 200, 200, 120));
                }
                const std::string path(nearestEnt->Path.begin(), nearestEnt->Path.end());
                dl->AddText(ImVec2(12.f, 32.f), IM_COL32(255, 200, 150, 255), path.c_str());
            }

            if (clicked && nearestEnt) {
                m_ui.editTarget = {};
                std::string path(nearestEnt->Path.begin(), nearestEnt->Path.end());
                if (path.find('*') == std::string::npos) path += '*';
                m_ui.editTarget.path = path;
                m_ui.editTarget.name = PathBaseName(path);
                m_ui.editTarget.category = "User";
                m_ui.editTarget.enabled = true;
                m_ui.editTarget.showIcon = true;
                m_ui.editTarget.expectedCount = 1;
                m_ui.editAreaKey = m_overlay.targets.ResolveAreaKey(snap.CurrentAreaHash,
                                                                    snap.CurrentAreaName);
                m_ui.editIsNew = true;
                m_ui.editStorageIndex = SIZE_MAX;
                m_ui.editModalOpen = true;
                EndPickerMode();
            }
        }

        dl->AddText(ImVec2(12, 12), IM_COL32(255, 255, 255, 255),
                    m_ui.pickerPoiMode
                        ? "Click nearest yellow marker (cyan = selected, Esc to cancel)"
                        : "Click nearest entity marker (Esc to cancel)");
    }
};

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new RadarPlugin(); }

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* p) { delete p; }
