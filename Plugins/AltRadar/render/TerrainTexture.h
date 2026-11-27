#pragma once

#include "WalkableBake.h"
#include "data/RadarConfig.h"

#include <d3d11.h>
#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace RadarRender {

class TerrainTexture {
public:
    struct DebugInfo {
        bool buildCalled = false;
        int  width = 0;
        int  height = 0;
        int  pixelsWritten = 0;
        int  nonTransparentPixels = 0;
        bool uploadCalled = false;
        bool uploadSucceeded = false;
    };

    ~TerrainTexture() { Release(); }

    void Release() {
        if (m_srv) {
            m_srv->Release();
            m_srv = nullptr;
        }
        m_device = nullptr;
        m_width = 0;
        m_height = 0;
        m_areaCounter = 0;
        m_walkablePtr = nullptr;
        m_style = {};
    }

    bool Valid() const { return m_srv != nullptr; }
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    ImTextureRef TexRef() const { return ImTextureRef(reinterpret_cast<void*>(m_srv)); }
    const DebugInfo& LastDebug() const { return m_debug; }

    bool IsCurrent(void* d3dDevice, const WalkableBake& bake, const RadarData::RadarConfig& cfg,
                   uint64_t areaCounter, const uint8_t* walkablePtr) const {
        if (!m_srv || !d3dDevice || !bake.valid || bake.width <= 0 || bake.height <= 0
            || bake.walkableMask.empty())
            return false;
        const PackedStyle style = PackedStyle::FromConfig(cfg);
        return m_device == d3dDevice && m_width == bake.width && m_height == bake.height
               && m_areaCounter == areaCounter && m_walkablePtr == walkablePtr && m_style == style;
    }

    bool EnsureBuilt(void* d3dDevice, const WalkableBake& bake, const RadarData::RadarConfig& cfg,
                     uint64_t areaCounter, const uint8_t* walkablePtr) {
        if (!d3dDevice || !bake.valid || bake.width <= 0 || bake.height <= 0
            || bake.walkableMask.empty()) {
            Release();
            return false;
        }

        const PackedStyle style = PackedStyle::FromConfig(cfg);
        if (IsCurrent(d3dDevice, bake, cfg, areaCounter, walkablePtr)) {
            return true;
        }

        return Build(d3dDevice, bake, style, areaCounter, walkablePtr);
    }

private:
    struct PackedColor {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t a = 0;

        bool operator==(const PackedColor& other) const {
            return r == other.r && g == other.g && b == other.b && a == other.a;
        }
    };

    struct PackedStyle {
        PackedColor interior;
        PackedColor edge;
        PackedColor dot;
        uint8_t     edgeThickness = 0;
        uint8_t     dotStep = 1;
        uint8_t     dotSizeTimesTwo = 3;
        uint8_t     terrainStyle = 0;
        bool        includeBoundaryEdges = false;
        bool        includeRasterizedDots = false;

        bool operator==(const PackedStyle& other) const {
            return interior == other.interior && edge == other.edge
                   && dot == other.dot
                   && edgeThickness == other.edgeThickness
                   && dotStep == other.dotStep
                   && dotSizeTimesTwo == other.dotSizeTimesTwo
                   && terrainStyle == other.terrainStyle
                   && includeBoundaryEdges == other.includeBoundaryEdges
                   && includeRasterizedDots == other.includeRasterizedDots;
        }

        static PackedStyle FromConfig(const RadarData::RadarConfig& cfg) {
            PackedStyle out;
            out.interior = PackColor(cfg.TextureInteriorColor);
            out.edge = PackColor(cfg.TextureWallEdgeColor);
            out.dot = PackColor(cfg.DotMatrixFillColor);
            out.edgeThickness = static_cast<uint8_t>(RadarData::SanitizeBoundaryEdgeThickness(
                cfg.WalkableMapBorderThickness));
            out.dotStep = static_cast<uint8_t>(std::clamp(cfg.DotCellStep, 1, 16));
            out.dotSizeTimesTwo = static_cast<uint8_t>(std::clamp(static_cast<int>(cfg.DotSize * 2.0f + 0.5f), 1, 12));
            out.terrainStyle = static_cast<uint8_t>(cfg.TerrainStyle);
            const auto effectiveDotMode = RadarData::EffectiveDotMatrixRenderMode(cfg.EnableDrawDebug,
                                                                                  cfg.DotRenderMode);
            out.includeBoundaryEdges = cfg.ShowBoundaryEdges
                                       && cfg.BoundaryRenderMode == RadarData::TerrainBoundaryRenderMode::CachedTexture
                                       && out.edgeThickness > 0;
            out.includeRasterizedDots = effectiveDotMode == RadarData::DotMatrixRenderMode::CachedTexture
                                        && (cfg.TerrainStyle == RadarData::TerrainRenderStyle::DotMatrix
                                            || cfg.TerrainStyle == RadarData::TerrainRenderStyle::TextureAndDotMatrix);
            return out;
        }

        static PackedColor PackColor(const ImVec4& c) {
            return PackedColor{
                ToByte(c.x),
                ToByte(c.y),
                ToByte(c.z),
                ToByte(c.w),
            };
        }

        static uint8_t ToByte(float v) {
            const int scaled = static_cast<int>(v * 255.0f + 0.5f);
            return static_cast<uint8_t>(std::clamp(scaled, 0, 255));
        }
    };

    bool Build(void* d3dDevice, const WalkableBake& bake, const PackedStyle& style,
               uint64_t areaCounter, const uint8_t* walkablePtr) {
        m_debug = {};
        m_debug.buildCalled = true;
        m_pixels.assign(bake.CellCount() * 4, 0);
        m_width = bake.width;
        m_height = bake.height;
        m_debug.width = m_width;
        m_debug.height = m_height;
        const bool fillInterior = style.terrainStyle != static_cast<uint8_t>(RadarData::TerrainRenderStyle::DotMatrix);
        if (fillInterior) {
            for (size_t idx = 0; idx < bake.walkableMask.size(); ++idx) {
                if (bake.walkableMask[idx] == 0) continue;
                const size_t px = idx * 4;
                m_pixels[px + 0] = style.interior.r;
                m_pixels[px + 1] = style.interior.g;
                m_pixels[px + 2] = style.interior.b;
                m_pixels[px + 3] = style.interior.a;
                ++m_debug.pixelsWritten;
            }
        }
        if (style.includeRasterizedDots)
            RasterizeDots(bake, style);
        if (style.includeBoundaryEdges && !bake.boundarySegments.empty())
            RasterizeBoundaryEdges(bake, style);

        for (size_t i = 3; i < m_pixels.size(); i += 4) {
            if (m_pixels[i] != 0) ++m_debug.nonTransparentPixels;
        }

        auto* dev = static_cast<ID3D11Device*>(d3dDevice);
        if (!dev) {
            Release();
            return false;
        }

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(bake.width);
        desc.Height = static_cast<UINT>(bake.height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sub{};
        sub.pSysMem = m_pixels.data();
        sub.SysMemPitch = static_cast<UINT>(bake.width * 4);

        ID3D11Texture2D* tex = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        m_debug.uploadCalled = true;
        HRESULT hr = dev->CreateTexture2D(&desc, &sub, &tex);
        if (FAILED(hr) || !tex) {
            Release();
            return false;
        }

        hr = dev->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
        if (FAILED(hr) || !srv) {
            Release();
            return false;
        }

        const int width = m_width;
        const int height = m_height;
        Release();
        m_srv = srv;
        m_device = d3dDevice;
        m_width = width;
        m_height = height;
        m_areaCounter = areaCounter;
        m_walkablePtr = walkablePtr;
        m_style = style;
        m_debug.width = m_width;
        m_debug.height = m_height;
        m_debug.uploadSucceeded = true;
        return true;
    }

    ID3D11ShaderResourceView* m_srv = nullptr;
    void*                     m_device = nullptr;
    int                       m_width = 0;
    int                       m_height = 0;
    uint64_t                  m_areaCounter = 0;
    const uint8_t*            m_walkablePtr = nullptr;
    PackedStyle               m_style{};
    std::vector<uint8_t>      m_pixels;
    DebugInfo                 m_debug{};

    void WritePixel(int x, int y, const PackedColor& color) {
        if (x < 0 || y < 0 || x >= m_width || y >= m_height) return;
        const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(m_width)
                           + static_cast<size_t>(x))
                           * 4;
        if (idx + 3 >= m_pixels.size()) return;
        m_pixels[idx + 0] = color.r;
        m_pixels[idx + 1] = color.g;
        m_pixels[idx + 2] = color.b;
        m_pixels[idx + 3] = color.a;
        ++m_debug.pixelsWritten;
    }

    void DrawThickPoint(int x, int y, int thickness, const PackedColor& color) {
        const int radius = std::max(0, thickness - 1);
        for (int dy = -radius; dy <= radius; ++dy)
            for (int dx = -radius; dx <= radius; ++dx)
                WritePixel(x + dx, y + dy, color);
    }

    void RasterizeBoundaryLine(int x0, int y0, int x1, int y1, int thickness,
                               const PackedColor& color) {
        int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true) {
            DrawThickPoint(x0, y0, thickness, color);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = err * 2;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    void RasterizeBoundaryEdges(const WalkableBake& bake, const PackedStyle& style) {
        const int thickness = std::max(1, static_cast<int>(style.edgeThickness));
        for (const auto& segment : bake.boundarySegments) {
            const int x0 = std::clamp(static_cast<int>(segment.x0), 0, m_width - 1);
            const int y0 = std::clamp(static_cast<int>(segment.y0), 0, m_height - 1);
            const int x1 = std::clamp(static_cast<int>(segment.x1), 0, m_width - 1);
            const int y1 = std::clamp(static_cast<int>(segment.y1), 0, m_height - 1);
            RasterizeBoundaryLine(x0, y0, x1, y1, thickness, style.edge);
        }
    }

    void RasterizeDots(const WalkableBake& bake, const PackedStyle& style) {
        const int step = std::max(1, static_cast<int>(style.dotStep));
        const int radius = std::max(0, (static_cast<int>(style.dotSizeTimesTwo) + 1) / 2 - 1);
        for (int gy = 0; gy < bake.height; gy += step) {
            for (int gx = 0; gx < bake.width; gx += step) {
                const size_t idx = static_cast<size_t>(gy) * static_cast<size_t>(bake.width)
                                 + static_cast<size_t>(gx);
                if (idx >= bake.walkableMask.size() || bake.walkableMask[idx] == 0) continue;
                for (int dy = -radius; dy <= radius; ++dy) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        WritePixel(gx + dx, gy + dy, style.dot);
                    }
                }
            }
        }
    }
};

} // namespace RadarRender
