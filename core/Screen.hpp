#pragma once
#include "raylib.h"

namespace Indium
{
    /**
     * @brief Per-frame game-viewport context for runtime UI (NativeScript::OnGUI).
     *
     * The editor fills this once per frame with the game viewport's pixel size and
     * the mouse position mapped into that same render-texture space. Scripts read it
     * inside OnGUI() to lay out and hit-test screen-space UI.
     *
     * Header-only singleton shared across the script dylib boundary, exactly like
     * InputManager / StoryState: the engine exports its symbols (ENABLE_EXPORTS) and
     * the dlopen'd script library binds to the engine's instance, so both sides read
     * and write the same values.
     *
     * Coordinates are viewport pixels: the UI space is [0, Width()] x [0, Height()],
     * the origin is the top-left, and MousePosition() is in the same space as the
     * coordinates OnGUI draws in.
     */
    class Screen
    {
    public:
        Screen(const Screen&)            = delete;
        Screen& operator=(const Screen&) = delete;
        Screen(Screen&&)                 = delete;
        Screen& operator=(Screen&&)      = delete;

        static Screen& Get() { static Screen instance; return instance; }

        // --- Read API (call from OnGUI) ---

        /** @brief Width of the game viewport in pixels. */
        static int Width()  { return Get().width_; }

        /** @brief Height of the game viewport in pixels. */
        static int Height() { return Get().height_; }

        /** @brief Mouse position in viewport pixels — same space OnGUI draws in. */
        static Vector2 MousePosition() { return Get().mouse_; }

        /** @brief True only on the frame the left button was pressed over the viewport. */
        static bool MousePressed() { return Get().pressed_; }

        /** @brief True while the left button is held over the viewport. */
        static bool MouseDown() { return Get().down_; }

        // --- Write API (editor only) ---

        void Set(int width, int height, Vector2 mouse, bool pressed, bool down)
        {
            width_   = width;
            height_  = height;
            mouse_   = mouse;
            pressed_ = pressed;
            down_    = down;
        }

    private:
        Screen()  = default;
        ~Screen() = default;

        int     width_   = 0;
        int     height_  = 0;
        Vector2 mouse_   = {0, 0};
        bool    pressed_ = false;
        bool    down_    = false;
    };
}
