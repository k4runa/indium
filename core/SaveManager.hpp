#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include "raylib.h"
#include "StoryState.hpp"
#include "scene/Scene.hpp"
#include "../include/nlohmann/json.hpp"

namespace fs = std::filesystem;

namespace Indium
{
    /**
     * @brief Persists a runnable game state to disk so players can resume progress.
     *
     * A slot records three things:
     *   - the current scene (so Load returns the player to the right level),
     *   - the global StoryState blackboard (flags / counters / variables),
     *   - the position of every entity tagged "Player".
     *
     * Slots live next to the project:
     *   <projectPath>/saves/slot_0.json
     *   <projectPath>/saves/slot_1.json  ...
     *
     * Format:
     *   {
     *     "scene": "level2",
     *     "storyState": { "key": { "type": "bool", "value": true }, ... },
     *     "players": [ { "id": 3, "x": 100.0, "y": 50.0 }, ... ]
     *   }
     *
     * The project path is set once by the editor (SetProjectPath) and shared across the
     * script dylib boundary like the other engine singletons, so scripts only pass a slot:
     *   SaveManager::Save(*GetScene(), 0);   // save to slot 0
     *   SaveManager::Load(*GetScene(), 0);   // resume from slot 0
     *
     * Load is a Play-time operation: it queues a deferred scene switch + restore on the
     * scene (consumed by ProjectManager::SwitchScene) so the saved flags and positions are
     * in place before the loaded scene's scripts start.
     */
    class SaveManager
    {
    public:
        SaveManager() = delete;

        /** @brief Sets the project root used for save slots. Called by the editor on
         *  project open; a function-local static so the engine and the script dylib
         *  share one value (same mechanism as StoryState / DialogueManager). */
        static void SetProjectPath(const std::string& path) { ProjectPath() = path; }

        static bool Save(const Scene& scene, int slot = 0)
        {
            const std::string& projectPath = ProjectPath();
            if (projectPath.empty()) { TraceLog(LOG_ERROR, "SAVE: no project path set"); return false; }
            fs::path savesDir = fs::path(projectPath) / "saves";

            try
            {
                fs::create_directories(savesDir);
                fs::path savePath = savesDir / ("slot_" + std::to_string(slot) + ".json");
                fs::path tmpPath  = savePath; tmpPath += ".tmp";

                nlohmann::json j;
                j["scene"]      = scene.name;
                j["storyState"] = StoryState::Get().serialize();

                nlohmann::json players = nlohmann::json::array();
                for (const auto& e : scene.entities)
                {
                    if (e->tag == "Player")
                        players.push_back({ { "id", e->id }, { "x", e->position.x }, { "y", e->position.y } });
                }
                j["players"] = players;

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

        static bool Load(Scene& scene, int slot = 0)
        {
            const std::string& projectPath = ProjectPath();
            if (projectPath.empty()) { TraceLog(LOG_ERROR, "SAVE: no project path set"); return false; }
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

                // Parse player positions (id -> local position) for restore.
                std::vector<std::pair<int, Vector2>> positions;
                if (j.contains("players") && j["players"].is_array())
                {
                    for (const auto& p : j["players"])
                        positions.push_back({ p.value("id", -1),
                                              Vector2{ p.value("x", 0.0f), p.value("y", 0.0f) } });
                }

                std::map<std::string, StoryValue> story;
                if (j.contains("storyState")) story = StoryValueMapFromJson(j["storyState"]);

                const std::string savedScene = j.value("scene", std::string{});

                if (!savedScene.empty())
                {
                    // Defer: queue the scene switch + restore. ProjectManager::SwitchScene
                    // applies the saved state after seeding authored defaults and before
                    // the loaded scene's scripts start.
                    scene._pendingStoryRestore    = std::move(story);
                    scene._pendingPositionRestore = std::move(positions);
                    scene._hasPendingRestore      = true;
                    scene._pendingSceneLoad       = savedScene;
                }
                else
                {
                    // Legacy save with no scene field: apply story + positions to the
                    // currently loaded scene immediately.
                    StoryState::Get().Clear();
                    StoryState::Get().Seed(story);
                    for (const auto& [id, pos] : positions)
                        if (Entity* e = scene.FindEntity(id)) e->position = pos;
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

        static bool SlotExists(int slot = 0)
        {
            const std::string& projectPath = ProjectPath();
            if (projectPath.empty()) return false;
            return fs::exists(fs::path(projectPath) / "saves" / ("slot_" + std::to_string(slot) + ".json"));
        }

        static bool DeleteSlot(int slot = 0)
        {
            const std::string& projectPath = ProjectPath();
            if (projectPath.empty()) return false;
            fs::path savePath = fs::path(projectPath) / "saves" / ("slot_" + std::to_string(slot) + ".json");
            if (!fs::exists(savePath)) return false;
            fs::remove(savePath);
            TraceLog(LOG_INFO, "SAVE: Slot %d deleted.", slot);
            return true;
        }

    private:
        /** @brief Project root for save slots. Function-local static so it resolves to a
         *  single shared instance across the engine / script-dylib boundary. */
        static std::string& ProjectPath() { static std::string p; return p; }
    };
}
