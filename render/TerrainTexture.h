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

        bool operator==(const PackedStyle& other) const {
            return interior == other.interior;
        }

        static PackedStyle FromConfig(const RadarData::RadarConfig& cfg) {
            PackedStyle out;
            out.interior = PackColor(cfg.TextureInteriorColor);
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
        m_pixels.assign(bake.CellCount() * 4, 0);
        for (size_t idx = 0; idx < bake.walkableMask.size(); ++idx) {
            if (bake.walkableMask[idx] == 0) continue;
            const size_t px = idx * 4;
            m_pixels[px + 0] = style.interior.r;
            m_pixels[px + 1] = style.interior.g;
            m_pixels[px + 2] = style.interior.b;
            m_pixels[px + 3] = style.interior.a;
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

        Release();
        m_srv = srv;
        m_device = d3dDevice;
        m_width = bake.width;
        m_height = bake.height;
        m_areaCounter = areaCounter;
        m_walkablePtr = walkablePtr;
        m_style = style;
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
};

} // namespace RadarRender
