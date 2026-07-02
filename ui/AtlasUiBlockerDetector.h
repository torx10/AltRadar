#pragma once

#include "sdk/PluginSDK.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace RadarUi {

struct AtlasUiBlockerDetector {
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
        bool        exists = false;
        bool        valid = false;
        bool        localVisible = false;
        bool        uiVisible = false;
        bool        hasRect = false;
    };

    bool active = false;
    bool enabled = true;
    NodeStatus primary{"root/1/22"};
    NodeStatus optionalChild{"root/1/22/0/6"};
    std::chrono::steady_clock::time_point lastScanTime = std::chrono::steady_clock::time_point{};
    uint64_t lastAreaCounter = 0;

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

    void ForceRefresh(PluginSDK::Context* ctx, uint64_t areaCounter) {
        primary = NodeStatus{"root/1/22"};
        optionalChild = NodeStatus{"root/1/22/0/6"};
        active = false;
        lastAreaCounter = areaCounter;
        lastScanTime = std::chrono::steady_clock::now();
        if (!ctx || !enabled) return;

        const uintptr_t topUiRoot = WalkUpToRoot(ctx, ctx->Ui.GetUiRoot());
        if (topUiRoot == 0) return;

        primary = ResolveNode(ctx, topUiRoot, "root/1/22");
        optionalChild = ResolveNode(ctx, topUiRoot, "root/1/22/0/6");
        active = primary.exists && primary.valid && (primary.uiVisible || primary.localVisible);
    }

    bool ShouldBlock(PluginSDK::Context* ctx, uint64_t areaCounter) {
        if (!enabled) {
            active = false;
            return false;
        }
        const auto now = std::chrono::steady_clock::now();
        const bool shouldScan = lastScanTime.time_since_epoch().count() == 0
                                || areaCounter != lastAreaCounter
                                || now - lastScanTime >= std::chrono::milliseconds(kScanIntervalMs);
        if (shouldScan) ForceRefresh(ctx, areaCounter);
        return active;
    }

    long long LastScanAgeMs() const {
        if (lastScanTime.time_since_epoch().count() == 0) return -1;
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - lastScanTime)
            .count();
    }
};

} // namespace RadarUi
