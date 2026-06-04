#pragma once
//
// BusyOverlay — a reusable, good-looking "please wait" overlay for any operation
// that blocks or runs in the background (script compile, project load, scene
// switch, save…). One look, used everywhere, so progress feedback is consistent.
//
// Visual: full-screen dim, a centered rounded card, a smooth circular spinner
// (arc sweeping around a faint track), a title line and an optional subtitle.
// Drawn with ImGui's draw list (no textures/assets), animated by raylib time.
//
// Usage — call DrawBusyOverlay every frame while the work is running:
//
//     if (busy)
//         Indium::BusyOverlay::Draw("Compiling scripts", "Running the C++ compiler…");
//
// It opens/owns its own popup, so the caller only decides *whether* to draw it.
#include "imgui.h"
#include "raylib.h"          // GetTime() for animation
#include <cmath>

namespace Indium
{
    namespace BusyOverlay
    {
        // Draw a smooth circular spinner centred at `center` with the given radius.
        // `t` is a time value in seconds; the arc sweeps and rotates based on it.
        inline void DrawSpinner(ImDrawList* dl, ImVec2 center, float radius,
                                float thickness, ImU32 trackCol, ImU32 arcCol)
        {
            const int   segments = 64;
            const float t        = (float)GetTime();

            // Faint full-circle track underneath.
            dl->PathClear();
            for (int i = 0; i <= segments; ++i)
            {
                float a = (float)i / segments * 2.0f * IM_PI;
                dl->PathLineTo(ImVec2(center.x + cosf(a) * radius,
                                      center.y + sinf(a) * radius));
            }
            dl->PathStroke(trackCol, 0, thickness);

            // Moving arc on top. Its length eases in/out so it feels alive rather
            // than a constant-length segment going round at fixed speed.
            float spin   = t * 2.6f;                          // rotation speed
            float head   = spin;
            float sweep  = 1.4f + 1.0f * sinf(t * 2.0f);      // 0.4..2.4 rad span
            float start  = head;
            float end    = head + sweep;

            dl->PathClear();
            int arcSegs = 48;
            for (int i = 0; i <= arcSegs; ++i)
            {
                float a = start + (end - start) * ((float)i / arcSegs);
                dl->PathLineTo(ImVec2(center.x + cosf(a) * radius,
                                      center.y + sinf(a) * radius));
            }
            dl->PathStroke(arcCol, 0, thickness);
        }

        // Full overlay: dim + centered card + spinner + text. Title is required,
        // subtitle may be nullptr.
        inline void Draw(const char* title, const char* subtitle = nullptr)
        {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            // Argumentless overload: the viewport-specific one is only in the
            // docking/internal API. The single foreground list covers the main
            // viewport, which is all we draw into.
            ImDrawList* fg = ImGui::GetForegroundDrawList();

            // 1. Dim the whole screen so the editor visibly recedes behind the card.
            fg->AddRectFilled(vp->Pos,
                              ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                              IM_COL32(0, 0, 0, 150));

            // 2. Card geometry, centred.
            const ImVec2 cardSize(360.0f, 150.0f);
            ImVec2 center(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f);
            ImVec2 cardMin(center.x - cardSize.x * 0.5f, center.y - cardSize.y * 0.5f);
            ImVec2 cardMax(center.x + cardSize.x * 0.5f, center.y + cardSize.y * 0.5f);

            // Card: subtle drop shadow, dark rounded body, hairline border.
            // Pure neutral grays (r==g==b) to match the editor's stealth theme —
            // any blue tint here looked off against it.
            fg->AddRectFilled(ImVec2(cardMin.x, cardMin.y + 6), ImVec2(cardMax.x, cardMax.y + 8),
                              IM_COL32(0, 0, 0, 90), 14.0f);
            fg->AddRectFilled(cardMin, cardMax, IM_COL32(28, 28, 28, 255), 14.0f);
            fg->AddRect(cardMin, cardMax, IM_COL32(70, 70, 70, 255), 14.0f, 0, 1.5f);

            // 3. Spinner, upper-centre of the card.
            ImVec2 spinCenter(center.x, cardMin.y + 52.0f);
            DrawSpinner(fg, spinCenter, 20.0f, 4.0f,
                        IM_COL32(70, 70, 70, 255),     // track
                        IM_COL32(255, 255, 255, 255)); // arc (theme accent = white)

            // 4. Title (centred) + subtitle (centred, dim).
            ImFont* font = ImGui::GetFont();
            float titleSize = ImGui::GetFontSize() + 2.0f;

            ImVec2 tsz = font->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, title);
            fg->AddText(font, titleSize,
                        ImVec2(center.x - tsz.x * 0.5f, cardMin.y + 86.0f),
                        IM_COL32(240, 240, 240, 255), title);

            if (subtitle)
            {
                float subSize = ImGui::GetFontSize() - 1.0f;
                ImVec2 ssz = font->CalcTextSizeA(subSize, FLT_MAX, 0.0f, subtitle);
                fg->AddText(font, subSize,
                            ImVec2(center.x - ssz.x * 0.5f, cardMin.y + 110.0f),
                            IM_COL32(150, 150, 150, 255), subtitle);
            }
        }
    }
}
