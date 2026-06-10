#pragma once
#include "raylib.h"
#include "Screen.hpp"
#include <string>
#include <vector>
#include <climits>
#include <algorithm>

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

    /** @brief Word-wrapped text within area.width, starting at (area.x, area.y).
     *  Honors explicit '\n'. A single word wider than area.width is broken
     *  character-by-character so it never overflows.
     *
     *  Layout (which lines hold which words) is computed from the FULL text and is
     *  independent of maxChars, so a typewriter reveal never reflows. `maxChars` caps
     *  how many glyphs are actually drawn (-1 = all); the return value is always the
     *  full-text height so callers can place choices at a fixed position regardless of
     *  how much has been revealed. `outRevealable` (optional) receives the number of glyphs
     *  at full reveal — what a typewriter caller should compare its counter against. It is
     *  fewer than strlen(text): wrap-point spaces, '\n' and collapsed runs aren't drawn. */
    inline float LabelWrapped(const char* text, ::Rectangle area, int size, Color c, int lineSpacing = 4, int maxChars = -1, int* outRevealable = nullptr)
    {
        if (!text) { if (outRevealable) *outRevealable = 0; return 0.0f; }
        const std::string s = text;
        const float lineH = (float)size + (float)lineSpacing;

        // --- Layout pass: wrap the full text into lines (stable; ignores maxChars) ---
        std::vector<std::string> lines;
        std::string line, word;

        // Character-wrap a word wider than area.width — pushes as many full lines as
        // needed and returns the remaining tail (which fits on one line).
        auto breakLongWord = [&](const std::string& w) -> std::string
        {
            std::string rest = w;
            while ((float)MeasureText(rest.c_str(), size) > area.width && !rest.empty())
            {
                std::string chunk;
                for (size_t k = 0; k < rest.size(); ++k)
                {
                    std::string cand = chunk + rest[k];
                    if ((float)MeasureText(cand.c_str(), size) > area.width && !chunk.empty()) break;
                    chunk = cand;
                }
                if (chunk.empty()) chunk = rest.substr(0, 1); // even one char overflows — emit it anyway
                lines.push_back(chunk);
                rest = rest.substr(chunk.size());
            }
            return rest;
        };

        auto addWord = [&](const std::string& w)
        {
            if ((float)MeasureText(w.c_str(), size) > area.width)
            {
                if (!line.empty()) { lines.push_back(line); line.clear(); }
                line = breakLongWord(w);
                return;
            }
            std::string cand = line.empty() ? w : line + " " + w;
            if (!line.empty() && (float)MeasureText(cand.c_str(), size) > area.width) { lines.push_back(line); line = w; }
            else line = cand;
        };

        for (size_t i = 0; i <= s.size(); ++i)
        {
            char ch = (i < s.size()) ? s[i] : '\0';
            if (ch == ' ' || ch == '\n' || ch == '\0')
            {
                if (!word.empty()) { addWord(word); word.clear(); }
                if (ch == '\n') { lines.push_back(line); line.clear(); }
            }
            else word += ch;
        }
        if (!line.empty()) lines.push_back(line);

        // Glyphs across the laid-out lines = the count at which the text is fully shown. This is
        // NOT strlen(text): wrap-point spaces, '\n' and collapsed runs aren't stored in the lines,
        // so a typewriter must reveal against this or choices stay hidden for the gap after the
        // text has finished appearing.
        if (outRevealable)
        {
            int t = 0;
            for (const auto& ln : lines) t += (int)ln.size();
            *outRevealable = t;
        }

        // --- Draw pass: reveal up to maxChars glyphs across the laid-out lines ---
        int   budget = (maxChars < 0) ? INT_MAX : maxChars;
        float y      = area.y;
        for (const auto& ln : lines)
        {
            if (budget >= (int)ln.size())          { DrawText(ln.c_str(), (int)area.x, (int)y, size, c); budget -= (int)ln.size(); }
            else                                   { if (budget > 0) DrawText(ln.substr(0, budget).c_str(), (int)area.x, (int)y, size, c); budget = 0; }
            y += lineH;
        }
        return (float)lines.size() * lineH;   // full-text height (independent of maxChars)
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

    /**
     * @brief Horizontal slider; returns the (possibly updated) value in [min, max].
     *
     * The grab is sticky: once a drag starts inside the track it follows the mouse
     * until the button is released, even if the cursor leaves the rectangle — a thin
     * volume track would otherwise drop the handle mid-drag. Only one slider is active
     * at a time, remembered by its track position (sliders at the same x,y alias).
     */
    inline float Slider(::Rectangle r, float value, float min = 0.0f, float max = 1.0f)
    {
        static bool    dragging  = false;
        static Vector2 activeKey = { 0.0f, 0.0f };

        Vector2 m       = Screen::MousePosition();
        bool    hovered = CheckCollisionPointRec(m, r);

        if (hovered && Screen::MousePressed()) { dragging = true; activeKey = { r.x, r.y }; }
        if (!Screen::MouseDown())              dragging = false;
        const bool active = dragging && activeKey.x == r.x && activeKey.y == r.y;

        if (active && max > min && r.width > 1.0f)
        {
            float t = std::clamp((m.x - r.x) / r.width, 0.0f, 1.0f);
            value   = min + t * (max - min);
        }
        value = std::clamp(value, min, max);

        const float t      = (max > min) ? (value - min) / (max - min) : 0.0f;
        const float trackH = 6.0f;
        ::Rectangle track  = { r.x, r.y + (r.height - trackH) * 0.5f, r.width, trackH };
        DrawRectangleRec(track, Color{ 30, 30, 33, 240 });
        DrawRectangleRec(::Rectangle{ track.x, track.y, track.width * t, track.height },
                         Color{ 110, 140, 200, 255 });
        DrawRectangleLinesEx(track, 1.0f, Color{ 120, 120, 130, 255 });

        const float hw = 10.0f, hh = 18.0f;
        ::Rectangle handle = { r.x + r.width * t - hw * 0.5f, r.y + (r.height - hh) * 0.5f, hw, hh };
        DrawRectangleRec(handle, (active || hovered) ? Color{ 200, 200, 210, 255 }
                                                     : Color{ 160, 160, 170, 255 });
        DrawRectangleLinesEx(handle, 1.0f, Color{ 120, 120, 130, 255 });

        return value;
    }

    /** @brief Checkbox with a label to its right; the whole rectangle is clickable.
     *  Returns the (possibly toggled) value. */
    inline bool Toggle(::Rectangle r, const char* label, bool value, int size = 18)
    {
        Vector2 m       = Screen::MousePosition();
        bool    hovered = CheckCollisionPointRec(m, r);
        if (hovered && Screen::MousePressed()) value = !value;

        const float box = (r.height < 18.0f) ? r.height : 18.0f;
        ::Rectangle b   = { r.x, r.y + (r.height - box) * 0.5f, box, box };
        DrawRectangleRec(b, hovered ? Color{ 70, 70, 75, 240 } : Color{ 45, 45, 48, 230 });
        DrawRectangleLinesEx(b, 1.0f, Color{ 120, 120, 130, 255 });
        if (value)
            DrawRectangleRec(::Rectangle{ b.x + 4.0f, b.y + 4.0f, b.width - 8.0f, b.height - 8.0f },
                             Color{ 110, 140, 200, 255 });
        if (label && label[0])
            Label(label, b.x + box + 8.0f, r.y + (r.height - (float)size) * 0.5f, size, RAYWHITE);
        return value;
    }
}
