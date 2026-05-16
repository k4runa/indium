/**********************************************************************************************
*
*   Indium - A modular 2D engine built on Raylib
*
*   This is the application entry point, responsible for bootstrapping systems,
*   initializing the graphics context, and managing the main execution loop.
*
*   Copyright (c) 2026
*
**********************************************************************************************/

#include "raylib.h"
#include "imgui.h"
#include "../include/rlImGui.h"
#include "../include/imgui_impl_raylib.h"
#include "../editor/Editor.hpp"
#include "./Config.hpp"
#include <algorithm>
#include <cmath>

namespace
{
    int ToWindowCoordinate(int value, float dpiScale)
    {
#if defined(__APPLE__)
        if (dpiScale > 1.0f) return std::max(1, (int)std::round((float)value / dpiScale));
#else
        (void)dpiScale;
#endif
        return value;
    }

    void ApplyConfiguredWindowSize(const Indium::Config& config)
    {
        Vector2 dpiScale = GetWindowScaleDPI();

        int windowWidth  = ToWindowCoordinate(config.screenWidth, dpiScale.x);
        int windowHeight = ToWindowCoordinate(config.screenHeight, dpiScale.y);

        int monitor = GetCurrentMonitor();
        int maxWidth  = ToWindowCoordinate(GetMonitorWidth(monitor), dpiScale.x);
        int maxHeight = ToWindowCoordinate(GetMonitorHeight(monitor), dpiScale.y);

        if (maxWidth > 0) windowWidth = std::min(windowWidth, std::max(1, maxWidth - 40));
        if (maxHeight > 0) windowHeight = std::min(windowHeight, std::max(1, maxHeight - 80));

        SetWindowSize(windowWidth, windowHeight);
    }
}

/**
 * @brief Application Entry Point.
 */
int main()
{
    /**
     * @brief Step 1: Configuration Loading.
     *
     * We load external settings first to determine window dimensions
     * and performance targets before initializing the hardware window.
     */
    Indium::Config config = Indium::Config::Load("../config.json");

    /**
     * @brief Step 2: Graphics Context Initialization.
     *
     * Raylib must be initialized before any other graphical operations occur.
     */
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(config.screenWidth, config.screenHeight, config.windowTitle.c_str());
    ApplyConfiguredWindowSize(config);
    ClearWindowState(FLAG_WINDOW_HIDDEN);

    SetExitKey(KEY_NULL);
    SetTargetFPS(config.targetFps);

    /**
     * @brief Step 3: UI Layer Setup.
     *
     * rlImGui acts as a bridge between ImGui and Raylib. The 'true' flag
     * enables dark mode by default.
     */
    rlImGuiBeginInitImGui();
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = io.Fonts->AddFontFromFileTTF("../assets/fonts/Roboto-Regular.ttf", 16.0f);
    if (!font) TraceLog(LOG_WARNING, "Failed to load font");
    ImGui::StyleColorsDark();
    rlImGuiEndInitImGui();

    /**
     * @brief Step 4: Engine Core Initialization.
     *
     * We create the Editor instance and call Init(). This must happen AFTER
     * the graphics context is ready, as the Editor may create textures or shaders.
     */
    Indium::Editor editor;
    editor.Init(config);

    /**
     * @brief Step 5: The Main Execution Loop.
     *
     * This loop continues until the user closes the window or triggers an exit.
     * It separates 'Update' (logic) from 'Run' (rendering) to maintain
     * clear architecture.
     */
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // Handle input, physics, and editor logic
        editor.Update(dt);

        // Execute the rendering pass
        editor.Run();
    }

    /**
     * @brief Step 6: Graceful Shutdown.
     *
     * Resources are released in the reverse order of their initialization
     * to prevent dangling pointers or memory leaks.
     */
    editor.Shutdown();
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
