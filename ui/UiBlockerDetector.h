#pragma once

#include "render/MapProjection.h"
#include "sdk/PluginSDK.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace RadarUi {

struct UiBlockerDetector {
    struct NodeStatus {
        std::string path;
        uintptr_t   address = 0;
        uintptr_t   parentAddress = 0;
        float       rectX = 0.f;
        float       rectY = 0.f;
        float       rectW = 0.f;
        float       rectH = 0.f;
        int         childCount = 0;
        uint32_t    flags = 0;
        uint16_t    elementType = 0;
        bool        exists = false;
        bool        valid = false;
        bool        localVisible = false;
        bool        uiVisible = false;
        bool        hasRect = false;
    };

    struct MatchState {
        bool        blocked = false;
        std::string ruleName;
        std::string reason;
        float       rectX = 0.f;
        float       rectY = 0.f;
        float       rectW = 0.f;
        float       rectH = 0.f;
        bool        hasRect = false;
        float       overlapX = 0.f;
        float       overlapY = 0.f;
        float       overlapW = 0.f;
        float       overlapH = 0.f;
        float       overlapArea = 0.f;
        bool        hasOverlap = false;
    };

    bool        active = false;
    bool        enabled = true;
    MatchState  match{};
    NodeStatus  primary{"root/1/22"};
    NodeStatus  optionalChild{"root/1/22/0/6"};
    std::chrono::steady_clock::time_point lastScanTime = std::chrono::steady_clock::time_point{};
    uint64_t    lastAreaCounter = 0;

    static constexpr int kScanIntervalMs = 300;

    static bool LooksHeapLike(uintptr_t address) {
        return address >= 0x10000ull && address <= 0x00007FFFFFFFFFFFull;
    }

    static uintptr_t WalkUpToRoot(PluginSDK::Context* ctx, uintptr_t start) {
        if (!ctx || !LooksHeapLike(start)) return 0;
        uintptr_t current = start;
        uintptr_t lastValid = start;
        for (int i = 0; i < 32; ++i) {
            const PluginSDK::UiElement element = ctx->Ui.Read(current);
            if (!element.Valid) break;
            lastValid = current;
            if (!LooksHeapLike(element.ParentAddress) || element.ParentAddress == current
                || element.ParentAddress == 0)
                break;
            current = element.ParentAddress;
        }
        return lastValid;
    }

    static std::vector<int> ParsePath(std::string_view path) {
        std::vector<int> indices;
        size_t start = 0;
        while (start < path.size()) {
            const size_t slash = path.find('/', start);
            const std::string_view token =
                slash == std::string_view::npos ? path.substr(start) : path.substr(start, slash - start);
            if (!token.empty() && token != "root") {
                int value = 0;
                for (const char ch : token) {
                    if (ch < '0' || ch > '9') return {};
                    value = value * 10 + (ch - '0');
                }
                indices.push_back(value);
            }
            if (slash == std::string_view::npos) break;
            start = slash + 1;
        }
        return indices;
    }

    static NodeStatus ResolveNode(PluginSDK::Context* ctx, uintptr_t root, std::string_view path) {
        NodeStatus out;
        out.path = std::string(path);
        if (!ctx || root == 0) return out;

        const auto indices = ParsePath(path);
        if (path.empty() || (indices.empty() && path != "root")) return out;

        uintptr_t current = root;
        out.exists = true;
        for (const int index : indices) {
            const auto children = ctx->Ui.GetChildren(current);
            if (index < 0 || static_cast<size_t>(index) >= children.size()) {
                out.exists = false;
                return out;
            }
            out.parentAddress = current;
            current = children[static_cast<size_t>(index)];
            if (current == 0) {
                out.exists = false;
                return out;
            }
        }

        out.address = current;
        const PluginSDK::UiElement element = ctx->Ui.Read(current);
        out.valid = element.Valid;
        if (!element.Valid) return out;

        out.localVisible = element.IsVisible;
        out.uiVisible = ctx->Ui.IsVisible(current);
        out.flags = element.Flags;
        out.elementType = element.ElementType;
        out.childCount = element.ChildCount;

        float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
        if (ctx->Ui.ComputeScreenRect(current, x, y, w, h)) {
            out.hasRect = true;
            out.rectX = x;
            out.rectY = y;
            out.rectW = w;
            out.rectH = h;
        }
        return out;
    }

    void ForceRefresh(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap) {
        primary = NodeStatus{"root/1/22"};
        optionalChild = NodeStatus{"root/1/22/0/6"};
        match = {};
        active = false;
        lastAreaCounter = snap.AreaChangeCounter;
        lastScanTime = std::chrono::steady_clock::now();
        if (!ctx || !enabled) return;

        const uintptr_t topUiRoot = WalkUpToRoot(ctx, ctx->Ui.GetUiRoot());
        if (topUiRoot == 0) return;

        primary = ResolveNode(ctx, topUiRoot, "root/1/22");
        optionalChild = ResolveNode(ctx, topUiRoot, "root/1/22/0/6");

        for (const RuleDef& rule : BuiltInRules()) {
            const NodeStatus node = rule.isAtlasCompatibility ? primary : ResolveNode(ctx, topUiRoot, rule.path);
            if (!MatchesRule(rule, node, ctx, snap)) continue;

            const OverlapInfo overlap = ComputeMapOverlap(ctx, snap, node);
            if (!overlap.hasOverlap) continue;

            match.blocked = true;
            match.ruleName = rule.name;
            match.reason = rule.reason;
            match.hasRect = node.hasRect;
            match.rectX = node.rectX;
            match.rectY = node.rectY;
            match.rectW = node.rectW;
            match.rectH = node.rectH;
            match.hasOverlap = overlap.hasOverlap;
            match.overlapX = overlap.x;
            match.overlapY = overlap.y;
            match.overlapW = overlap.w;
            match.overlapH = overlap.h;
            match.overlapArea = overlap.area;
            active = true;
            return;
        }
    }

    bool ShouldBlock(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap) {
        if (!enabled) {
            active = false;
            match = {};
            return false;
        }
        const auto now = std::chrono::steady_clock::now();
        const bool shouldScan = lastScanTime.time_since_epoch().count() == 0
                                || snap.AreaChangeCounter != lastAreaCounter
                                || now - lastScanTime >= std::chrono::milliseconds(kScanIntervalMs);
        if (shouldScan) ForceRefresh(ctx, snap);
        return active;
    }

    long long LastScanAgeMs() const {
        if (lastScanTime.time_since_epoch().count() == 0) return -1;
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - lastScanTime)
            .count();
    }

private:
    enum class Shape {
        Any,
        Fullscreen,
        LeftPanel,
    };

    struct RuleDef {
        const char* name;
        const char* path;
        const char* reason;
        uint16_t    elementType;
        int         childCount;
        Shape       shape;
        bool        isAtlasCompatibility;
    };

    struct OverlapInfo {
        bool  hasOverlap = false;
        float x = 0.f;
        float y = 0.f;
        float w = 0.f;
        float h = 0.f;
        float area = 0.f;
    };

    static const std::array<RuleDef, 6>& BuiltInRules() {
        static const std::array<RuleDef, 6> rules{{
            {"TopUiRootAtlasBlocker", "root/1/22", "Atlas UI visible", 0, -1, Shape::Any, true},
            {"TopUiRootPassiveTreeFullscreen", "root/1/24", "Passive tree fullscreen UI visible", 0x5921, 8, Shape::Fullscreen, false},
            {"TopUiRootAtlasSkillsTreeFullscreen", "root/1/25", "Atlas skills tree fullscreen UI visible", 0x5921, 9, Shape::Fullscreen, false},
            {"TopUiRootMtxStoreFullscreen", "root/1/29", "MTX store fullscreen UI visible", 0x0, 10, Shape::Fullscreen, false},
            {"TopUiRootMarketPanel", "root/1/98", "Market UI visible", 0x0, -1, Shape::LeftPanel, false},
            {"TopUiRootLeftSidePanel", "root/1/38", "Large left-side UI panel visible", 0x0, -1, Shape::LeftPanel, false},
        }};
        return rules;
    }

    static bool IntersectRects(float ax, float ay, float aw, float ah,
                               float bx, float by, float bw, float bh,
                               OverlapInfo& out) {
        const float left = (ax > bx) ? ax : bx;
        const float top = (ay > by) ? ay : by;
        const float right = ((ax + aw) < (bx + bw)) ? (ax + aw) : (bx + bw);
        const float bottom = ((ay + ah) < (by + bh)) ? (ay + ah) : (by + bh);
        if (right <= left || bottom <= top) return false;
        out.hasOverlap = true;
        out.x = left;
        out.y = top;
        out.w = right - left;
        out.h = bottom - top;
        out.area = out.w * out.h;
        return true;
    }

    static OverlapInfo ComputeMapOverlap(PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap,
                                         const NodeStatus& node) {
        OverlapInfo best;
        if (!node.hasRect) return best;

        if (snap.LargeMap.IsVisible && snap.LargeMap.SizeX > 0.f && snap.LargeMap.SizeY > 0.f) {
            const float halfW = snap.LargeMap.SizeX * 0.5f;
            const float halfH = snap.LargeMap.SizeY * 0.5f;
            OverlapInfo overlap;
            if (IntersectRects(node.rectX, node.rectY, node.rectW, node.rectH,
                               snap.LargeMap.CenterX - halfW, snap.LargeMap.CenterY - halfH,
                               snap.LargeMap.SizeX, snap.LargeMap.SizeY, overlap)
                && overlap.area > best.area) {
                best = overlap;
            }
        }

        if (snap.MiniMap.IsVisible && snap.MiniMap.SizeX > 0.f && snap.MiniMap.SizeY > 0.f) {
            float minimapCx = snap.MiniMap.CenterX;
            float minimapCy = snap.MiniMap.CenterY;
            RadarRender::GetMinimapClipOrigin(ctx, snap.MiniMap, minimapCx, minimapCy);
            const float halfW = snap.MiniMap.SizeX * 0.5f;
            const float halfH = snap.MiniMap.SizeY * 0.5f;
            OverlapInfo overlap;
            if (IntersectRects(node.rectX, node.rectY, node.rectW, node.rectH,
                               minimapCx - halfW, minimapCy - halfH,
                               snap.MiniMap.SizeX, snap.MiniMap.SizeY, overlap)
                && overlap.area > best.area) {
                best = overlap;
            }
        }

        return best;
    }

    static bool CoversFullscreen(const NodeStatus& node, const PluginSDK::Snapshot& snap) {
        const float screenW = snap.ScreenWidth > 0 ? static_cast<float>(snap.ScreenWidth) : 0.f;
        const float screenH = snap.ScreenHeight > 0 ? static_cast<float>(snap.ScreenHeight) : 0.f;
        if (screenW <= 0.f || screenH <= 0.f) return false;
        return node.rectX <= screenW * 0.02f && node.rectY <= screenH * 0.02f
               && node.rectW >= screenW * 0.90f && node.rectH >= screenH * 0.90f;
    }

    static bool CoversLeftPanel(const NodeStatus& node, const PluginSDK::Snapshot& snap) {
        const float screenW = snap.ScreenWidth > 0 ? static_cast<float>(snap.ScreenWidth) : 0.f;
        const float screenH = snap.ScreenHeight > 0 ? static_cast<float>(snap.ScreenHeight) : 0.f;
        if (screenW <= 0.f || screenH <= 0.f) return false;
        return node.rectX <= screenW * 0.02f && node.rectY <= screenH * 0.02f
               && node.rectH >= screenH * 0.90f && node.rectW >= screenW * 0.18f;
    }

    static bool MatchesRule(const RuleDef& rule, const NodeStatus& node,
                            PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap) {
        if (!node.exists || !node.valid || !node.localVisible || !node.uiVisible || !node.hasRect)
            return false;
        if (node.path != rule.path) return false;
        if (rule.childCount >= 0 && node.childCount != rule.childCount) return false;
        if (node.path == "root/1/25/1" || node.path == "root/1/25/7" || node.path == "root/1/97/9")
            return false;
        if (!rule.isAtlasCompatibility && node.elementType != rule.elementType) return false;

        const OverlapInfo overlap = ComputeMapOverlap(ctx, snap, node);
        if (!overlap.hasOverlap) return false;

        switch (rule.shape) {
            case Shape::Fullscreen: return CoversFullscreen(node, snap);
            case Shape::LeftPanel: return CoversLeftPanel(node, snap);
            case Shape::Any:
            default: return true;
        }
    }
};

} // namespace RadarUi
