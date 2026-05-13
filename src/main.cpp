#include "raylib.h"
#include "imgui.h"
#include "../include/rlImGui.h"
#include "../include/imgui_impl_raylib.h"
#include "../Editor/Editor.hpp"
#include "./Config.hpp"


int main()
{
    // Load configuration from JSON
    Indium::Config config = Indium::Config::Load("../config.json");

    // Initialize Raylib Window
    InitWindow(config.screenWidth, config.screenHeight, config.windowTitle.c_str());
    SetTargetFPS(config.targetFps);

    // Setup ImGui integration
    rlImGuiSetup(true);

    // Initialize the Editor (MUST be after InitWindow + rlImGuiSetup)
    // Main Editor Instance
    Indium::Editor editor;
    editor.Init();

    // Main Game Loop
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        editor.Update(dt);
        editor.Run();
    }

    // Cleanup (reverse order of init)
    editor.Shutdown();
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
