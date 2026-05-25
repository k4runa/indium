#pragma once
#include "raylib.h"
#include "Screen.hpp"

namespace Indium::GUI
{
    /**
     * @brief Immediate-mode screen-space widgets for NativeScript::OnGUI().
     *
     * All coordinates are viewport pixels (see Screen::Width/Height). These are
     * deliberately raylib-only (no Dear ImGui) so in-game UI never depends on the
     * editor's UI toolkit. Text uses raylib's default font; switch to DrawTextEx
     * with a loaded Font when a typeface is needed.
     */

    /** @brief Filled rectangle with an optional border. */
    inline void Box(::Rectangle r, Color bg, Color border = BLANK, float borderPx = 0.0f)
    {
        if (bg.a > 0)                          DrawRectangleRec(r, bg);
        if (borderPx > 0.0f && border.a > 0)   DrawRectangleLinesEx(r, borderPx, border);
    }

    /** @brief Left-aligned text at (x, y). */
    inline void Label(const char* text, float x, float y, int size, Color c)
    {
        DrawText(text, (int)x, (int)y, size, c);
    }

    /** @brief Text centered on both axes within a rectangle. */
    inline void LabelCentered(const char* text, ::Rectangle within, int size, Color c)
    {
        int tw = MeasureText(text, size);
        int x  = (int)(within.x + (within.width  - (float)tw)   * 0.5f);
        int y  = (int)(within.y + (within.height - (float)size) * 0.5f);
        DrawText(text, x, y, size, c);
    }

    /** @brief Texture stretched into a destination rectangle. No-op if unset. */
    inline void Image(Texture2D tex, ::Rectangle dst, Color tint = WHITE)
    {
        if (tex.id == 0) return;
        ::Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
        DrawTexturePro(tex, src, dst, {0.0f, 0.0f}, 0.0f, tint);
    }

    /**
     * @brief Clickable button. Returns true on the frame it is pressed.
     *
     * Hover/press visuals come from Screen's mouse state, so the button only
     * reacts while the cursor is over the game viewport.
     */
    inline bool Button(::Rectangle r, const char* label, int size = 20)
    {
        Vector2 m       = Screen::MousePosition();
        bool    hovered = CheckCollisionPointRec(m, r);

        Color base   = { 45, 45, 48, 230 };
        Color hot    = { 70, 70, 75, 240 };
        Color active = { 30, 30, 33, 240 };
        Color border = { 120, 120, 130, 255 };

        Color bg = base;
        if (hovered) bg = (Screen::MouseDown()) ? active : hot;

        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1.0f, border);
        LabelCentered(label, r, size, RAYWHITE);

        return hovered && Screen::MousePressed();
    }
}
