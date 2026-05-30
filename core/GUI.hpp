#pragma once
#include "raylib.h"
#include "Screen.hpp"
#include <string>

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
     *  character-by-character so it never overflows. Returns the total pixel
     *  height drawn (place choices below it). */
    inline float LabelWrapped(const char* text, ::Rectangle area, int size, Color c, int lineSpacing = 4)
    {
        if (!text) return 0.0f;
        const std::string s = text;
        const float lineH = (float)size + (float)lineSpacing;
        float y = area.y;
        std::string line, word;

        auto flush = [&](const std::string& ln) { DrawText(ln.c_str(), (int)area.x, (int)y, size, c); y += lineH; };

        // Character-wrap a word that's wider than area.width — emits as many full
        // lines as needed and returns the remaining tail (which fits on one line).
        auto breakLongWord = [&](const std::string& w) -> std::string
        {
            std::string chunk;
            std::string rest = w;
            while ((float)MeasureText(rest.c_str(), size) > area.width && !rest.empty())
            {
                chunk.clear();
                for (size_t k = 0; k < rest.size(); ++k)
                {
                    std::string cand = chunk + rest[k];
                    if ((float)MeasureText(cand.c_str(), size) > area.width && !chunk.empty()) break;
                    chunk = cand;
                }
                if (chunk.empty()) chunk = rest.substr(0, 1); // even one char overflows — emit it anyway
                flush(chunk);
                rest = rest.substr(chunk.size());
            }
            return rest;
        };

        auto addWord = [&](const std::string& w)
        {
            // If the word alone overflows, break it character-by-character.
            if ((float)MeasureText(w.c_str(), size) > area.width)
            {
                if (!line.empty()) { flush(line); line.clear(); }
                line = breakLongWord(w);
                return;
            }
            std::string cand = line.empty() ? w : line + " " + w;
            if (!line.empty() && (float)MeasureText(cand.c_str(), size) > area.width) { flush(line); line = w; }
            else line = cand;
        };

        for (size_t i = 0; i <= s.size(); ++i)
        {
            char ch = (i < s.size()) ? s[i] : '\0';
            if (ch == ' ' || ch == '\n' || ch == '\0')
            {
                if (!word.empty()) { addWord(word); word.clear(); }
                if (ch == '\n') { flush(line); line.clear(); }
            }
            else word += ch;
        }
        if (!line.empty()) flush(line);
        return y - area.y;
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
