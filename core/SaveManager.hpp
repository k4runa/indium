#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <algorithm>
#include "raylib.h"
#include "EventBus.hpp"
#include "events/GameEvents.hpp"
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
     *     "players": [ { "id": 3, "x": 100.0, "y": 50.0 }, ... ],
     *     "savedAt": 1781234567
     *   }
     * ("savedAt" is epoch seconds, used for slot lists and "Continue"; saves written
     * before it existed still load — ListSlots falls back to the file's mtime.)
     *
     * The project path is set once by the editor (SetProjectPath) and shared across the
     * script dylib boundary like the other engine singletons, so scripts only pass a slot:
     *   SaveManager::Save(*GetScene(), 0);   // save to slot 0
     *   SaveManager::Load(*GetScene(), 0);   // resume from slot 0
     *
     * Load is a Play-time operation: it queues a deferred scene switch + restore on the
     * scene (consumed by ProjectManager::SwitchScene) so the saved flags and positions are
     * in place before the loaded scene's scripts start.
     *
     * Autosave: slot kAutosaveSlot (0) is written automatically by the editor on every
     * gameplay scene switch (and by CheckpointComponent zones). Scripts customize this:
     *   SaveManager::SetAutosaveOnSceneSwitch(false);        // opt out of switch saves
     *   SaveManager::AddAutosaveCondition("chapter >= 2");   // StoryEval expr; saves when
     *                                                        // it first becomes true
     *   SaveManager::RequestAutosave();                      // save at next frame boundary
     * Conditions and RequestAutosave are deferred — the editor performs the actual write
     * with scene access at a frame boundary, never mid-event-dispatch.
     */
    class SaveManager
    {
    public:
        SaveManager() = delete;

        /** @brief The reserved autosave slot. Manual save-menu slots start at 1. */
        static constexpr int kAutosaveSlot = 0;

        /** @brief One row of ListSlots: which slot, the scene it was saved in, and when
         *  (epoch seconds; 0 = unknown, e.g. unreadable legacy file). */
        struct SaveSlotInfo
        {
            int         slot    = -1;
            std::string scene;
            long long   savedAt = 0;
        };

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
                j["savedAt"]    = (long long)std::chrono::duration_cast<std::chrono::seconds>(
                                      std::chrono::system_clock::now().time_since_epoch()).count();

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

        // --- Slot enumeration (save/load menus) --------------------------------

        /** @brief Scans <project>/saves/ for slot_N.json files and returns their
         *  metadata sorted by slot number. A save written before "savedAt" existed
         *  reports the file's mtime instead (0 if even that fails); malformed files
         *  are skipped. */
        static std::vector<SaveSlotInfo> ListSlots()
        {
            std::vector<SaveSlotInfo> out;
            const std::string& projectPath = ProjectPath();
            if (projectPath.empty()) return out;
            fs::path savesDir = fs::path(projectPath) / "saves";

            try
            {
                if (!fs::exists(savesDir)) return out;
                for (const auto& entry : fs::directory_iterator(savesDir))
                {
                    if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
                    const std::string stem = entry.path().stem().string();
                    if (stem.rfind("slot_", 0) != 0) continue;

                    SaveSlotInfo info;
                    try { info.slot = std::stoi(stem.substr(5)); }
                    catch (...) { continue; }

                    try
                    {
                        std::ifstream in(entry.path());
                        nlohmann::json j;
                        in >> j;
                        info.scene   = j.value("scene", std::string{});
                        info.savedAt = j.value("savedAt", 0LL);
                    }
                    catch (...) { continue; }

                    if (info.savedAt == 0) info.savedAt = FileMtimeEpoch(entry.path());
                    out.push_back(std::move(info));
                }
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "SAVE: ListSlots failed: %s", e.what());
            }

            std::sort(out.begin(), out.end(),
                      [](const SaveSlotInfo& a, const SaveSlotInfo& b) { return a.slot < b.slot; });
            return out;
        }

        /** @brief The slot with the newest savedAt across every existing save (autosave
         *  included) — what the title screen's "Continue" loads. -1 when no saves exist. */
        static int MostRecentSlot()
        {
            int       best     = -1;
            long long bestTime = -1;
            for (const auto& s : ListSlots())
                if (s.savedAt > bestTime) { bestTime = s.savedAt; best = s.slot; }
            return best;
        }

        /** @brief "2026-06-12 14:30" in local time, or "" for 0 (unknown). */
        static std::string FormatTimestamp(long long epochSecs)
        {
            if (epochSecs <= 0) return "";
            std::time_t t = (std::time_t)epochSecs;
            std::tm     tmBuf{};
#ifdef _WIN32
            localtime_s(&tmBuf, &t);
#else
            localtime_r(&t, &tmBuf);
#endif
            char buf[32];
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tmBuf) == 0) return "";
            return buf;
        }

        // --- Autosave configuration (script-facing) ----------------------------
        //
        // All state lives in function-local statics (the ProjectPath mechanism) so the
        // engine and the script dylib share one copy. The editor resets the config at
        // Play start BEFORE scripts' OnStart, so per-game setup in OnStart sticks.

        /** @brief Master switch. Off = scene-switch saves, conditions, and
         *  RequestAutosave all become no-ops. Default true. */
        static void SetAutosaveEnabled(bool on) { AutosaveEnabledRef() = on; }
        static bool AutosaveEnabled()           { return AutosaveEnabledRef(); }

        /** @brief Which slot autosaves write to. Default kAutosaveSlot (0). */
        static void SetAutosaveSlot(int slot) { AutosaveSlotRef() = slot; }
        static int  AutosaveSlot()            { return AutosaveSlotRef(); }

        /** @brief Whether the editor autosaves the outgoing scene on a gameplay scene
         *  switch (never on a switch that is itself a save-restore). Default true. */
        static void SetAutosaveOnSceneSwitch(bool on) { AutosaveOnSwitchRef() = on; }
        static bool AutosaveOnSceneSwitch()           { return AutosaveOnSwitchRef(); }

        /**
         * @brief Registers a StoryEval expression (e.g. "boss_defeated", "chapter >= 2")
         * that triggers an autosave when it transitions false -> true. Edge-triggered:
         * it re-arms when the expression goes false again, and a condition that is
         * already true when added does not fire until it goes false and back.
         */
        static void AddAutosaveCondition(const std::string& expr)
        {
            if (expr.empty()) return;
            AutosaveConditions().push_back({ expr, StoryEval(expr) });
        }

        static void ClearAutosaveConditions() { AutosaveConditions().clear(); }

        /** @brief Queues an autosave; the editor performs the write at the next frame
         *  boundary (it has the scene; event handlers and scripts may not). */
        static void RequestAutosave()
        {
            if (AutosaveEnabled()) AutosavePendingRef() = true;
        }

        /** @brief Editor-side drain: returns whether an autosave was pending and clears
         *  the flag. The caller does the actual Save(scene, AutosaveSlot()). */
        static bool ConsumeAutosaveRequest()
        {
            bool pending = AutosavePendingRef() && AutosaveEnabled();
            AutosavePendingRef() = false;
            return pending;
        }

        /** @brief (Re)arms the StoryStateChangedEvent subscription that evaluates
         *  autosave conditions. Must be called on Play start: EventBus::Clear() on Stop
         *  wipes every channel (mirrors QuestManager::SubscribeToEvents). */
        static void SubscribeToEvents()
        {
            ChangeSub() = Events::Subscribe<GameEvents::StoryStateChangedEvent>(
                [](const GameEvents::StoryStateChangedEvent&) { EvaluateAutosaveConditions(); });
        }

        /** @brief Restores autosave defaults and clears conditions + any pending
         *  request. Called by the editor at Play start before scripts run. */
        static void ResetAutosaveConfig()
        {
            AutosaveEnabledRef()  = true;
            AutosaveSlotRef()     = kAutosaveSlot;
            AutosaveOnSwitchRef() = true;
            AutosaveConditions().clear();
            AutosavePendingRef()  = false;
        }

    private:
        struct AutosaveCondition
        {
            std::string expr;
            bool        last = false; // last observed StoryEval value (edge detection)
        };

        /** @brief Re-evaluates every registered condition against current StoryState;
         *  a false -> true edge queues an autosave. Only reads StoryState and sets the
         *  pending flag, so it cannot re-enter the event dispatch. */
        static void EvaluateAutosaveConditions()
        {
            for (auto& c : AutosaveConditions())
            {
                const bool now = StoryEval(c.expr);
                if (now && !c.last) RequestAutosave();
                c.last = now;
            }
        }

        static long long FileMtimeEpoch(const fs::path& p)
        {
            try
            {
                // file_clock -> system_clock without clock_cast (not in Apple libc++):
                // offset the file time by the difference between the two clocks' nows.
                const auto ft  = fs::last_write_time(p);
                const auto sys = std::chrono::time_point_cast<std::chrono::seconds>(
                    ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                return (long long)sys.time_since_epoch().count();
            }
            catch (...) { return 0; }
        }

        /** @brief Project root for save slots. Function-local static so it resolves to a
         *  single shared instance across the engine / script-dylib boundary. */
        static std::string& ProjectPath() { static std::string p; return p; }

        // Autosave state — same shared-static mechanism as ProjectPath().
        static bool& AutosaveEnabledRef()  { static bool b = true;          return b; }
        static int&  AutosaveSlotRef()     { static int  s = kAutosaveSlot; return s; }
        static bool& AutosaveOnSwitchRef() { static bool b = true;          return b; }
        static bool& AutosavePendingRef()  { static bool b = false;         return b; }
        static std::vector<AutosaveCondition>& AutosaveConditions()
        { static std::vector<AutosaveCondition> v; return v; }
        static SubscriptionHandle& ChangeSub() { static SubscriptionHandle h; return h; }
    };
}
