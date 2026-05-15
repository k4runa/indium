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
    InitWindow(config.screenWidth, config.screenHeight, config.windowTitle.c_str());
    SetExitKey(KEY_NULL); // Disable ESC key as exit trigger
    SetTargetFPS(config.targetFps);

    /**
     * @brief Step 3: UI Layer Setup.
     *
     * rlImGui acts as a bridge between ImGui and Raylib. The 'true' flag
     * enables dark mode by default.
     */
    rlImGuiBeginInitImGui();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("../assets/fonts/Roboto-Regular.ttf", 16.0f);
    ImGui::StyleColorsDark();
    rlImGuiEndInitImGui();

    /**
     * @brief Step 4: Engine Core Initialization.
     *
     * We create the Editor instance and call Init(). This must happen AFTER
     * the graphics context is ready, as the Editor may create textures or shaders.
     */
    Indium::Editor editor;
    editor.Init();

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
