#include "raylib.h"
#include "imgui.h"
#include "../include/rlImGui.h"
#include "../include/imgui_impl_raylib.h"
#include "../include/imgui.h"
#include "../Entity/Circle.hpp"
#include "../Entity/Rectangle.hpp"
#include "../Scene/Scene.hpp"
#include <iostream>
#include "vector"
#include "memory"
#include <map>
#include <string>
#include "./Config.hpp"
#include "../Entity/EntityFactory.hpp"
#include "../Editor/Editor.hpp"


int main()
{
    // Initialize the Editor
    Editor editor;

    // Load configuration from JSON
    Indium::Config config = Indium::Config::Load("../config.json");


    // Initialize Raylib Window
    InitWindow(config.screenWidth, config.screenHeight, config.windowTitle.c_str());
    SetTargetFPS(config.targetFps);

    // Setup ImGui integration
    rlImGuiSetup(true);

    // Main Game Loop
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // 1. Update logic
        editor.Update(dt);

        // 2. Render everything
        editor.Run();
    }

    // Cleanup
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}

