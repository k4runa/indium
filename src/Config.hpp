#pragma once

#include <fstream>
#include <string>
#include "../include/nlohmann/json.hpp"

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
            if (!file.is_open())
            {
                TraceLog(LOG_WARNING, "Config: could not open '%s', using defaults instead", path.c_str());
                return { 1280, 720, 60, false, "Indium Engine" };
            }

            nlohmann::json j;
            try { j = nlohmann::json::parse(file); }
            catch (const std::exception& e)
            {
                TraceLog(LOG_WARNING, "Config: malformed JSON ('%s'), using defaults instead", e.what());
                return { 1280, 720, 60, false, "Indium Engine" };
            }

            Config c;
            c.screenWidth  = j.value("screenWidth",  1280);
            c.screenHeight = j.value("screenHeight", 720);
            c.targetFps    = j.value("targetFps",    60);
            c.windowTitle  = j.value("windowTitle",  "Indium Engine");
            c.showFps      = j.value("showFps",      false);
            return c;
        }
    };
}
