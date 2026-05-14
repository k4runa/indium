/**********************************************************************************************
*
*   Config - Engine settings and initialization parameters
*
*   Handles loading and storage of global engine parameters from external 
*   data sources, ensuring flexible configuration without recompilation.
*
*   Copyright (c) 2026
*
**********************************************************************************************/

#pragma once

#include "fstream"
#include "string"
#include "json.hpp"

namespace Indium
{
    /**
     * @brief Global engine configuration container.
     *
     * This structure holds all essential startup settings, such as display resolution
     * and performance targets. It is designed to be loaded from an external JSON file,
     * allowing users to customize the engine without recompilation.
     */
    struct Config
    {
        /** @brief Window width in pixels. */
        int         screenWidth;

        /** @brief Window height in pixels. */
        int         screenHeight;

        /** @brief Target frame rate for the simulation. */
        int         targetFps;

        /** @brief Toggle to display the FPS counter in the UI. */
        bool        showFps = false;

        /** @brief The text displayed in the operating system's window title bar. */
        std::string windowTitle;

        /**
         * @brief Synchronously loads engine settings from a JSON file.
         *
         * If the file is missing or corrupted, the engine will automatically
         * fallback to safe default values (1280x720, 60fps).
         *
         * @param path The relative or absolute path to the configuration file.
         * @return A Config object populated with file data or defaults.
         */
        static Config Load(const std::string& path)
        {
            std::ifstream file(path);
            if (!file.is_open()) return { 1280, 720, 60, false, "Indium Engine" };

            nlohmann::json j = nlohmann::json::parse(file);

            Config c;
            c.screenHeight = j.value("screenHeight", 720);
            c.screenWidth  = j.value("screenWidth", 1280);
            c.targetFps    = j.value("targetFps", 60);
            c.windowTitle  = j.value("windowTitle", "Indium Engine");
            c.showFps      = j.value("showFps", false);

            return c;
        }
    };
}
