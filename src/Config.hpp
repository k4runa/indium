#pragma once

#include "fstream"
#include "string"
#include "json.hpp"

namespace Indium
{
    /**
     * @brief Configuration settings for the engine, loaded from JSON.
     */
    struct Config
    {
        int         screenWidth;
        int         screenHeight;
        int         targetFps;
        bool        showFps = false;
        std::string windowTitle;

        /** @brief Static helper to load config from a JSON file */
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

