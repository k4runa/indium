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
#include "../include/imgui.h"
#include <string>

#define NO_FONT_AWESOME
#include "../include/rlImGui.h"
#include "../include/extras/IconsFontAwesome6.h"
#include "../include/imgui_impl_raylib.h"
#include "../editor/Editor.hpp"
#include "../core/Logger.hpp"
#include "./Config.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

#include "../include/roboto_regular.h"
#include "../include/fa_solid_900.h"

namespace
{
    void ApplyConfiguredWindowSize(const Indium::Config& config)
    {

        int monitor = GetCurrentMonitor();
        Vector2 dpiScale = GetWindowScaleDPI();

        int maxWidth  = (int)(GetMonitorWidth(monitor)  / dpiScale.x) - 40;
        int maxHeight = (int)(GetMonitorHeight(monitor) / dpiScale.y) - 80;

        int windowWidth  = config.screenWidth;
        int windowHeight = config.screenHeight;

        if (maxWidth > 0)  windowWidth  = std::min(windowWidth,  std::max(1, maxWidth));
        if (maxHeight > 0) windowHeight = std::min(windowHeight, std::max(1, maxHeight));

        SetWindowSize(windowWidth, windowHeight);
    }
}

/**
 * @brief Application Entry Point.
 */
extern "C" void InstallCrashHandler(); // core/CrashHandler.cpp

int main()
{
    InstallCrashHandler(); // capture unhandled crashes to crash.log

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
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_RESIZABLE);
    InitWindow(config.screenWidth, config.screenHeight, config.windowTitle.c_str());
    ApplyConfiguredWindowSize(config);
    ClearWindowState(FLAG_WINDOW_HIDDEN);

    // Mirror all TraceLog output to logs/ from here on (must follow InitWindow so
    // raylib's logging is up). Captures the rest of startup and the whole session.
    Indium::Logger::Init();

    InitAudioDevice();
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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // enable panel docking
    io.Fonts->Clear();

    // Extended glyph range: Basic Latin + Latin-1 Supplement + Latin Extended-A
    // Latin Extended-A (0x0100–0x017F) covers Turkish (ş ğ ı), Polish, Czech, etc.
    static const ImWchar base_ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
        0x0100, 0x017F, // Latin Extended-A
        0,
    };
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    // Embedded fonts — no external files needed.
    // AddFontFromMemoryTTF takes ownership, so we must give it a malloc'd copy.
    void* robotoData = ImGui::MemAlloc(roboto_regular_ttf_len);
    memcpy(robotoData, roboto_regular_ttf, roboto_regular_ttf_len);
    io.Fonts->AddFontFromMemoryTTF(robotoData, roboto_regular_ttf_len, 15.5f, nullptr, base_ranges);

    ImFontConfig fa_cfg;
    fa_cfg.MergeMode  = true;
    fa_cfg.PixelSnapH = true;
    fa_cfg.GlyphMinAdvanceX = 14.0f;
    void* faData = ImGui::MemAlloc(fa_solid_900_ttf_len);
    memcpy(faData, fa_solid_900_ttf, fa_solid_900_ttf_len);
    io.Fonts->AddFontFromMemoryTTF(faData, fa_solid_900_ttf_len, 14.0f, &fa_cfg, icons_ranges);

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
    while (!editor.ShouldClose())
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
    CloseAudioDevice();
    Indium::Logger::Shutdown();
    CloseWindow();

    return 0;
}
