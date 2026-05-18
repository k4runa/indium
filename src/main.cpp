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
#include <string>
#include <filesystem>
#define NO_FONT_AWESOME
#include "../include/rlImGui.h"
#include "../include/extras/IconsFontAwesome6.h"
#include "../include/imgui_impl_raylib.h"
#include "../editor/Editor.hpp"
#include "./Config.hpp"
#include <algorithm>
#include <cmath>

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
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_RESIZABLE);
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
    io.Fonts->Clear();

    // Extended glyph range: Basic Latin + Latin-1 Supplement + Latin Extended-A
    // Latin Extended-A (0x0100–0x017F) covers Turkish (ş ğ ı), Polish, Czech, etc.
    static const ImWchar base_ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
        0x0100, 0x017F, // Latin Extended-A
        0,
    };
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    // Returns true if path exists on disk and has at least 100 bytes (ImGui hard requirement).
    auto fontValid = [](const std::string& p) -> bool {
        std::error_code ec;
        auto sz = std::filesystem::file_size(p, ec);
        return !ec && sz >= 100;
    };

    std::string appDir = GetApplicationDirectory();

    // Candidate base fonts: project-local first, then platform system fonts.
    const std::vector<std::string> baseCandidates = {
        appDir + "/../assets/fonts/Inter-Regular.ttf",
        appDir + "/../assets/fonts/Roboto-Regular.ttf",
#if defined(_WIN32)
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/arial.ttf",
#else
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
#endif
    };

    std::string baseFontPath;
    for (const auto& c : baseCandidates) {
        if (fontValid(c)) { baseFontPath = c; break; }
    }

    bool baseFontLoaded = false;
    if (!baseFontPath.empty()) {
        baseFontLoaded = (io.Fonts->AddFontFromFileTTF(baseFontPath.c_str(), 15.5f, nullptr, base_ranges) != nullptr);
        printf("FONT: %s — %s\n", baseFontPath.c_str(), baseFontLoaded ? "OK" : "parse failed");
    } else {
        printf("FONT: No base font found — using ImGui built-in (add a .ttf to assets/fonts/)\n");
    }

    // FontAwesome: merge into base font so icons render inline with text.
    // If base font failed, load as standalone (icons will still work, just on separate atlas entry).
    std::string faPath = appDir + "/../assets/fonts/fa-solid-900.ttf";
    if (fontValid(faPath)) {
        ImFontConfig fa_cfg;
        fa_cfg.MergeMode  = baseFontLoaded;
        fa_cfg.PixelSnapH = true;
        fa_cfg.GlyphMinAdvanceX = 14.0f; // Keep icons mono-width for alignment
        bool faOk = (io.Fonts->AddFontFromFileTTF(faPath.c_str(), 14.0f, &fa_cfg, icons_ranges) != nullptr);
        printf("FONT: FontAwesome — %s\n", faOk ? "OK" : "parse failed");
    } else {
        printf("FONT: fa-solid-900.ttf not found — add it to assets/fonts/ for icons\n");
    }

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
    CloseWindow();

    return 0;
}
