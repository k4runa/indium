#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include "raylib.h"
#include "StoryState.hpp"
#include "../include/nlohmann/json.hpp"

namespace fs = std::filesystem;

namespace Indium
{
    /**
     * @brief Persists StoryState to disk so players can resume their progress.
     *
     * Slots are stored as JSON files next to the project:
     *   <projectPath>/saves/slot_0.json
     *   <projectPath>/saves/slot_1.json  ...
     *
     * Format: { "storyState": { "key": { "type": "bool", "value": true }, ... } }
     *
     * Usage (from a script):
     *   SaveManager::Save(projectPath, 0);   // save to slot 0
     *   SaveManager::Load(projectPath, 0);   // restore from slot 0
     */
    class SaveManager
    {
    public:
        SaveManager() = delete;

        static bool Save(const std::string& projectPath, int slot = 0)
        {
            if (projectPath.empty()) return false;
            fs::path savesDir = fs::path(projectPath) / "saves";

            try
            {
                fs::create_directories(savesDir);
                fs::path savePath = savesDir / ("slot_" + std::to_string(slot) + ".json");
                fs::path tmpPath  = savePath; tmpPath += ".tmp";

                nlohmann::json j;
                j["storyState"] = StoryState::Get().serialize();

                // Write to a temp file then rename so a crash mid-write never corrupts
                // an existing save slot (mirrors ProjectManager::SaveCurrentProject).
                {
                    std::ofstream out(tmpPath);
                    if (!out.is_open())
                    {
                        TraceLog(LOG_ERROR, "SAVE: cannot open '%s' for writing", tmpPath.c_str());
                        return false;
                    }
                    out << std::setw(4) << j << std::endl;
                    out.flush();
                    if (!out.good())
                    {
                        TraceLog(LOG_ERROR, "SAVE: write error on '%s'", tmpPath.c_str());
                        fs::remove(tmpPath);
                        return false;
                    }
                }
                fs::rename(tmpPath, savePath);
                TraceLog(LOG_INFO, "SAVE: Slot %d saved to '%s'", slot, savePath.c_str());
                return true;
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "SAVE: Failed to save slot %d: %s", slot, e.what());
                return false;
            }
        }

        static bool Load(const std::string& projectPath, int slot = 0)
        {
            if (projectPath.empty()) return false;
            fs::path savePath = fs::path(projectPath) / "saves" / ("slot_" + std::to_string(slot) + ".json");

            if (!fs::exists(savePath))
            {
                TraceLog(LOG_WARNING, "SAVE: Slot %d not found at '%s'", slot, savePath.c_str());
                return false;
            }

            try
            {
                std::ifstream in(savePath);
                nlohmann::json j;
                in >> j;

                if (j.contains("storyState"))
                {
                    StoryState::Get().Clear();
                    StoryState::Get().deserialize(j["storyState"]);
                }

                TraceLog(LOG_INFO, "SAVE: Slot %d loaded from '%s'", slot, savePath.c_str());
                return true;
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "SAVE: Failed to load slot %d: %s", slot, e.what());
                return false;
            }
        }

        static bool SlotExists(const std::string& projectPath, int slot = 0)
        {
            if (projectPath.empty()) return false;
            return fs::exists(fs::path(projectPath) / "saves" / ("slot_" + std::to_string(slot) + ".json"));
        }

        static bool DeleteSlot(const std::string& projectPath, int slot = 0)
        {
            if (projectPath.empty()) return false;
            fs::path savePath = fs::path(projectPath) / "saves" / ("slot_" + std::to_string(slot) + ".json");
            if (!fs::exists(savePath)) return false;
            fs::remove(savePath);
            TraceLog(LOG_INFO, "SAVE: Slot %d deleted.", slot);
            return true;
        }
    };
}
