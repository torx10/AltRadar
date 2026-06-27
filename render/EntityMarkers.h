#pragma once

#include "data/RadarTypes.h"

#include <imgui.h>

#include <algorithm>

namespace RadarRender {

inline ImVec2 MarkerPoint(const ImVec2& center, float radius, float nx, float ny) {
    return ImVec2(center.x + radius * nx, center.y + radius * ny);
}

template <size_t N>
inline void FillMarkerPoly(ImDrawList* dl, const ImVec2 (&pts)[N], ImU32 color) {
    dl->AddConvexPolyFilled(pts, static_cast<int>(N), color);
}

inline void DrawMarkerCircle(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddCircleFilled(center, radius, color, 18);
}

inline void DrawMarkerSquare(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddRectFilled(ImVec2(center.x - radius * 0.9f, center.y - radius * 0.9f),
                      ImVec2(center.x + radius * 0.9f, center.y + radius * 0.9f), color,
                      radius * 0.12f);
}

inline void DrawMarkerTriangle(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, 0.f, -1.f),
        MarkerPoint(center, radius, 0.92f, 0.70f),
        MarkerPoint(center, radius, -0.92f, 0.70f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerDiamond(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, 0.f, -1.f),
        MarkerPoint(center, radius, 1.f, 0.f),
        MarkerPoint(center, radius, 0.f, 1.f),
        MarkerPoint(center, radius, -1.f, 0.f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerPlus(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const float arm = radius * 0.34f;
    const float ext = radius * 0.95f;
    const ImVec2 pts[] = {
        {center.x - arm, center.y - ext}, {center.x + arm, center.y - ext},
        {center.x + arm, center.y - arm}, {center.x + ext, center.y - arm},
        {center.x + ext, center.y + arm}, {center.x + arm, center.y + arm},
        {center.x + arm, center.y + ext}, {center.x - arm, center.y + ext},
        {center.x - arm, center.y + arm}, {center.x - ext, center.y + arm},
        {center.x - ext, center.y - arm}, {center.x - arm, center.y - arm},
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerStar(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    constexpr float kPi = 3.14159265358979323846f;
    ImVec2 pts[10];
    for (int i = 0; i < 10; ++i) {
        const float angle = -kPi * 0.5f + (kPi * 2.f * static_cast<float>(i) / 10.f);
        const float r = (i % 2 == 0) ? radius : radius * 0.45f;
        pts[i] = ImVec2(center.x + cosf(angle) * r, center.y + sinf(angle) * r);
    }
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerHexagon(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, -0.86f, -0.50f),
        MarkerPoint(center, radius, 0.00f, -1.00f),
        MarkerPoint(center, radius, 0.86f, -0.50f),
        MarkerPoint(center, radius, 0.86f, 0.50f),
        MarkerPoint(center, radius, 0.00f, 1.00f),
        MarkerPoint(center, radius, -0.86f, 0.50f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerPentagon(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, 0.00f, -1.00f),
        MarkerPoint(center, radius, 0.95f, -0.26f),
        MarkerPoint(center, radius, 0.60f, 0.96f),
        MarkerPoint(center, radius, -0.60f, 0.96f),
        MarkerPoint(center, radius, -0.95f, -0.26f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerTriangleDown(ImDrawList* dl, const ImVec2& center, float radius,
                                   ImU32 color) {
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, 0.f, 1.f),
        MarkerPoint(center, radius, 0.92f, -0.70f),
        MarkerPoint(center, radius, -0.92f, -0.70f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerArrowUp(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, 0.00f, -1.00f),
        MarkerPoint(center, radius, 1.00f, 0.05f),
        MarkerPoint(center, radius, 0.40f, 0.05f),
        MarkerPoint(center, radius, 0.40f, 1.00f),
        MarkerPoint(center, radius, -0.40f, 1.00f),
        MarkerPoint(center, radius, -0.40f, 0.05f),
        MarkerPoint(center, radius, -1.00f, 0.05f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerCross(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const float w = radius * 0.28f;
    const float ext = radius * 0.95f;
    const ImVec2 a[] = {
        {center.x - ext, center.y - ext + w}, {center.x - ext + w, center.y - ext},
        {center.x + ext, center.y + ext - w}, {center.x + ext - w, center.y + ext},
    };
    const ImVec2 b[] = {
        {center.x + ext - w, center.y - ext}, {center.x + ext, center.y - ext + w},
        {center.x - ext + w, center.y + ext}, {center.x - ext, center.y + ext - w},
    };
    FillMarkerPoly(dl, a, color);
    FillMarkerPoly(dl, b, color);
}

inline void DrawMarkerHeart(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddCircleFilled(ImVec2(center.x - radius * 0.34f, center.y - radius * 0.26f),
                        radius * 0.42f, color, 14);
    dl->AddCircleFilled(ImVec2(center.x + radius * 0.34f, center.y - radius * 0.26f),
                        radius * 0.42f, color, 14);
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, -0.92f, -0.06f),
        MarkerPoint(center, radius, 0.92f, -0.06f),
        MarkerPoint(center, radius, 0.00f, 1.00f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerDroplet(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddCircleFilled(ImVec2(center.x, center.y + radius * 0.18f), radius * 0.58f, color, 18);
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, 0.00f, -1.00f),
        MarkerPoint(center, radius, 0.60f, -0.05f),
        MarkerPoint(center, radius, -0.60f, -0.05f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerGem(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, 0.00f, -1.00f),
        MarkerPoint(center, radius, 0.88f, -0.20f),
        MarkerPoint(center, radius, 0.00f, 1.00f),
        MarkerPoint(center, radius, -0.88f, -0.20f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerRing(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddCircle(center, radius, color, 24, std::max(1.25f, radius * 0.28f));
    dl->AddCircleFilled(center, std::max(1.35f, radius * 0.24f), color, 12);
}

inline void DrawMarkerFang(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, 0.00f, -1.00f),
        MarkerPoint(center, radius, 0.58f, -0.55f),
        MarkerPoint(center, radius, 0.38f, 0.18f),
        MarkerPoint(center, radius, 0.00f, 1.00f),
        MarkerPoint(center, radius, -0.38f, 0.18f),
        MarkerPoint(center, radius, -0.58f, -0.55f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerClaw(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 left[] = {
        MarkerPoint(center, radius, -0.95f, -0.95f),
        MarkerPoint(center, radius, -0.62f, 1.00f),
        MarkerPoint(center, radius, -0.22f, 0.52f),
        MarkerPoint(center, radius, -0.47f, -0.28f),
    };
    const ImVec2 mid[] = {
        MarkerPoint(center, radius, -0.22f, -1.05f),
        MarkerPoint(center, radius, 0.02f, 1.00f),
        MarkerPoint(center, radius, 0.35f, 0.52f),
        MarkerPoint(center, radius, 0.08f, -0.28f),
    };
    const ImVec2 right[] = {
        MarkerPoint(center, radius, 0.52f, -0.95f),
        MarkerPoint(center, radius, 0.78f, 1.00f),
        MarkerPoint(center, radius, 1.00f, 0.52f),
        MarkerPoint(center, radius, 0.77f, -0.28f),
    };
    FillMarkerPoly(dl, left, color);
    FillMarkerPoly(dl, mid, color);
    FillMarkerPoly(dl, right, color);
}

inline void DrawMarkerShield(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, 0.00f, -1.00f),
        MarkerPoint(center, radius, 0.76f, -0.72f),
        MarkerPoint(center, radius, 0.76f, 0.10f),
        MarkerPoint(center, radius, 0.48f, 0.72f),
        MarkerPoint(center, radius, 0.00f, 1.00f),
        MarkerPoint(center, radius, -0.48f, 0.72f),
        MarkerPoint(center, radius, -0.76f, 0.10f),
        MarkerPoint(center, radius, -0.76f, -0.72f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerFlask(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddRectFilled(ImVec2(center.x - radius * 0.28f, center.y - radius * 1.00f),
                      ImVec2(center.x + radius * 0.28f, center.y - radius * 0.48f), color,
                      radius * 0.06f);
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, -0.55f, -0.42f),
        MarkerPoint(center, radius, 0.55f, -0.42f),
        MarkerPoint(center, radius, 0.86f, 0.28f),
        MarkerPoint(center, radius, 0.38f, 1.00f),
        MarkerPoint(center, radius, -0.38f, 1.00f),
        MarkerPoint(center, radius, -0.86f, 0.28f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerEye(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 eye[] = {
        MarkerPoint(center, radius, -1.00f, 0.00f),
        MarkerPoint(center, radius, -0.42f, -0.62f),
        MarkerPoint(center, radius, 0.42f, -0.62f),
        MarkerPoint(center, radius, 1.00f, 0.00f),
        MarkerPoint(center, radius, 0.42f, 0.62f),
        MarkerPoint(center, radius, -0.42f, 0.62f),
    };
    FillMarkerPoly(dl, eye, color);
    dl->AddCircleFilled(center, radius * 0.28f, IM_COL32(0, 0, 0, 96), 12);
}

inline void DrawMarkerCoin(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddCircleFilled(center, radius, color, 22);
    dl->AddCircle(center, radius * 0.62f, IM_COL32(0, 0, 0, 92), 18, std::max(1.0f, radius * 0.14f));
}

inline void DrawMarkerSword(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddRectFilled(ImVec2(center.x - radius * 0.14f, center.y - radius * 1.00f),
                      ImVec2(center.x + radius * 0.14f, center.y + radius * 0.28f), color,
                      radius * 0.05f);
    dl->AddRectFilled(ImVec2(center.x - radius * 0.72f, center.y + radius * 0.18f),
                      ImVec2(center.x + radius * 0.72f, center.y + radius * 0.46f), color,
                      radius * 0.04f);
    dl->AddRectFilled(ImVec2(center.x - radius * 0.20f, center.y + radius * 0.46f),
                      ImVec2(center.x + radius * 0.20f, center.y + radius * 0.92f), color,
                      radius * 0.04f);
}

inline void DrawMarkerSkull(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 head(center.x, center.y - radius * 0.14f);
    dl->AddCircleFilled(head, radius * 0.72f, color, 18);
    const ImVec2 jaw[] = {
        MarkerPoint(center, radius, -0.52f, 0.10f),
        MarkerPoint(center, radius, 0.52f, 0.10f),
        MarkerPoint(center, radius, 0.46f, 0.90f),
        MarkerPoint(center, radius, -0.46f, 0.90f),
    };
    FillMarkerPoly(dl, jaw, color);
}

inline void DrawMarkerPerson(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddCircleFilled(ImVec2(center.x, center.y - radius * 0.48f), radius * 0.30f, color, 16);
    const ImVec2 body[] = {
        MarkerPoint(center, radius, -0.62f, 0.95f),
        MarkerPoint(center, radius, -0.48f, 0.18f),
        MarkerPoint(center, radius, -0.18f, -0.06f),
        MarkerPoint(center, radius, 0.18f, -0.06f),
        MarkerPoint(center, radius, 0.48f, 0.18f),
        MarkerPoint(center, radius, 0.62f, 0.95f),
    };
    FillMarkerPoly(dl, body, color);
}

inline void DrawMarkerChat(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 bubbleMin(center.x - radius * 0.92f, center.y - radius * 0.62f);
    const ImVec2 bubbleMax(center.x + radius * 0.92f, center.y + radius * 0.32f);
    dl->AddRectFilled(bubbleMin, bubbleMax, color, radius * 0.22f);
    const ImVec2 tail[] = {
        MarkerPoint(center, radius, -0.18f, 0.32f),
        MarkerPoint(center, radius, -0.66f, 1.00f),
        MarkerPoint(center, radius, 0.02f, 0.45f),
    };
    FillMarkerPoly(dl, tail, color);
}

inline void DrawMarkerChest(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddRectFilled(ImVec2(center.x - radius * 0.84f, center.y - radius * 0.08f),
                      ImVec2(center.x + radius * 0.84f, center.y + radius * 0.78f), color,
                      radius * 0.12f);
    dl->AddRectFilled(ImVec2(center.x - radius * 0.74f, center.y - radius * 0.72f),
                      ImVec2(center.x + radius * 0.74f, center.y - radius * 0.08f), color,
                      radius * 0.16f);
    dl->AddRectFilled(ImVec2(center.x - radius * 0.11f, center.y - radius * 0.03f),
                      ImVec2(center.x + radius * 0.11f, center.y + radius * 0.36f),
                      IM_COL32(0, 0, 0, 72), radius * 0.06f);
}

inline void DrawMarkerCrown(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, -0.98f, 0.42f),
        MarkerPoint(center, radius, -0.82f, -0.68f),
        MarkerPoint(center, radius, -0.34f, -0.06f),
        MarkerPoint(center, radius, 0.00f, -0.96f),
        MarkerPoint(center, radius, 0.34f, -0.06f),
        MarkerPoint(center, radius, 0.82f, -0.68f),
        MarkerPoint(center, radius, 0.98f, 0.42f),
        MarkerPoint(center, radius, 0.72f, 0.96f),
        MarkerPoint(center, radius, -0.72f, 0.96f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerMapPin(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    DrawMarkerCircle(dl, ImVec2(center.x, center.y - radius * 0.25f), radius * 0.65f, color);
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, -0.42f, -0.05f),
        MarkerPoint(center, radius, 0.42f, -0.05f),
        MarkerPoint(center, radius, 0.00f, 1.00f),
    };
    FillMarkerPoly(dl, pts, color);
    dl->AddCircleFilled(ImVec2(center.x, center.y - radius * 0.25f), radius * 0.22f,
                        IM_COL32(0, 0, 0, 72), 12);
}

inline void DrawMarkerFlag(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddRectFilled(ImVec2(center.x - radius * 0.78f, center.y - radius * 1.0f),
                      ImVec2(center.x - radius * 0.56f, center.y + radius * 1.0f), color,
                      radius * 0.05f);
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, -0.52f, -0.92f),
        MarkerPoint(center, radius, 0.95f, -0.92f),
        MarkerPoint(center, radius, 0.48f, -0.22f),
        MarkerPoint(center, radius, 0.95f, 0.38f),
        MarkerPoint(center, radius, -0.52f, 0.38f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerStairs(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    const ImVec2 pts[] = {
        MarkerPoint(center, radius, -0.95f, 0.95f),
        MarkerPoint(center, radius, -0.95f, 0.45f),
        MarkerPoint(center, radius, -0.40f, 0.45f),
        MarkerPoint(center, radius, -0.40f, -0.05f),
        MarkerPoint(center, radius, 0.15f, -0.05f),
        MarkerPoint(center, radius, 0.15f, -0.55f),
        MarkerPoint(center, radius, 0.72f, -0.55f),
        MarkerPoint(center, radius, 0.72f, -0.95f),
        MarkerPoint(center, radius, 0.95f, -0.95f),
        MarkerPoint(center, radius, 0.95f, 0.95f),
    };
    FillMarkerPoly(dl, pts, color);
}

inline void DrawMarkerPortal(ImDrawList* dl, const ImVec2& center, float radius, ImU32 color) {
    dl->AddCircle(center, radius, color, 24, std::max(1.1f, radius * 0.24f));
    dl->AddCircle(center, radius * 0.55f, color, 20, std::max(1.0f, radius * 0.18f));
}

inline void DrawMarkerExclamation(ImDrawList* dl, const ImVec2& center, float radius,
                                  ImU32 color) {
    dl->AddRectFilled(ImVec2(center.x - radius * 0.16f, center.y - radius * 0.95f),
                      ImVec2(center.x + radius * 0.16f, center.y + radius * 0.35f), color,
                      radius * 0.08f);
    dl->AddCircleFilled(ImVec2(center.x, center.y + radius * 0.78f), radius * 0.16f, color, 10);
}

inline void DrawMarkerShape(ImDrawList* dl, RadarData::MarkerShape shape, const ImVec2& center,
                            float radius, ImU32 color) {
    switch (shape) {
        case RadarData::MarkerShape::Circle:
            DrawMarkerCircle(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Square:
            DrawMarkerSquare(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Triangle:
            DrawMarkerTriangle(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Diamond:
            DrawMarkerDiamond(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Plus:
            DrawMarkerPlus(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Star:
            DrawMarkerStar(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Hexagon:
            DrawMarkerHexagon(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Pentagon:
            DrawMarkerPentagon(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::TriangleDown:
            DrawMarkerTriangleDown(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::ArrowUp:
            DrawMarkerArrowUp(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Cross:
            DrawMarkerCross(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Heart:
            DrawMarkerHeart(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Droplet:
            DrawMarkerDroplet(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Gem:
            DrawMarkerGem(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Fang:
            DrawMarkerFang(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Claw:
            DrawMarkerClaw(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Shield:
            DrawMarkerShield(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Flask:
            DrawMarkerFlask(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Eye:
            DrawMarkerEye(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Coin:
            DrawMarkerCoin(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Sword:
            DrawMarkerSword(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Skull:
            DrawMarkerSkull(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Person:
            DrawMarkerPerson(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Chat:
            DrawMarkerChat(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Chest:
            DrawMarkerChest(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Crown:
            DrawMarkerCrown(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Ring:
            DrawMarkerRing(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::MapPin:
            DrawMarkerMapPin(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Flag:
            DrawMarkerFlag(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Stairs:
            DrawMarkerStairs(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Portal:
            DrawMarkerPortal(dl, center, radius, color);
            break;
        case RadarData::MarkerShape::Exclamation:
            DrawMarkerExclamation(dl, center, radius, color);
            break;
        default:
            break;
    }
}

inline void DrawEntityMarker(ImDrawList* dl, RadarData::MarkerShape shape, float screenX,
                             float screenY, float radius, ImU32 color) {
    if (!dl || shape == RadarData::MarkerShape::None || radius <= 0.f) return;

    const int alpha = static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xFF);
    const int shadowAlpha = std::clamp(alpha * 3 / 4, 72, 180);
    const ImVec2 center(screenX, screenY);
    DrawMarkerShape(dl, shape, center, radius + std::max(1.0f, radius * 0.18f),
                    IM_COL32(0, 0, 0, shadowAlpha));
    DrawMarkerShape(dl, shape, center, radius, color);
}

} // namespace RadarRender
