#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <algorithm>
#include "raylib.h"
#include "AudioMixer.hpp"
#include "InputManager.hpp"
#include "../include/nlohmann/json.hpp"

namespace Indium
{
    /**
     * @brief Per-project player settings — audio volumes + key rebinds — persisted to
     * <project>/settings.json. Header-only singleton shared across the script dylib
     * boundary, like the two stores it fronts (AudioMixer / InputManager).
     *
     * Save() snapshots the LIVE state of AudioMixer and InputManager. Load() applies
     * audio directly, but LAYERS input rebinds: only action names that already exist in
     * InputManager are overridden — never added or cleared — so a missing or partial
     * settings file leaves the project-authored action map intact. (Deliberately not
     * InputManager::Load, which clears all actions first.)
     *
     * Dirty tracking: the in-game settings menu (MenuManager) calls MarkDirty() when the
     * player actually changes something, and the editor saves on Stop only if dirty —
     * so a script that ducks a bus at runtime never gets its temporary volume persisted
     * as a player preference.
     *
     * File shape:
     *   { "audio": { "master": 0.8, "buses": { "Music": 0.5, ... } },
     *     "input": { "actions": { "Jump": { "key": 87, "mouseBtn": -1, "useMouse": false } } } }
     */
    class GameSettings
    {
    public:
        GameSettings(const GameSettings&)            = delete;
        GameSettings& operator=(const GameSettings&) = delete;
        GameSettings(GameSettings&&)                 = delete;
        GameSettings& operator=(GameSettings&&)      = delete;

        static GameSettings& Get() { static GameSettings inst; return inst; }

        /** @brief Where settings.json lives. Set by the editor when Play starts. */
        void SetProjectPath(const std::string& path) { projectPath_ = path; }
        [[nodiscard]] const std::string& GetProjectPath() const { return projectPath_; }

        /** @brief The player changed something in the settings menu since the last
         *  Save/Load — the editor's Stop saves only when this is set. */
        void MarkDirty() { dirty_ = true; }
        [[nodiscard]] bool IsDirty() const { return dirty_; }

        /** @brief Snapshots live AudioMixer + InputManager state to settings.json
         *  (atomic temp+rename, like ItemManager::SaveDefinition). */
        bool Save()
        {
            if (projectPath_.empty()) return false;
            std::error_code       ec;
            std::filesystem::path path = std::filesystem::path(projectPath_) / "settings.json";
            std::filesystem::path tmp  = path; tmp += ".tmp";
            {
                std::ofstream out(tmp);
                if (!out.is_open()) { TraceLog(LOG_ERROR, "SETTINGS: cannot write %s", tmp.string().c_str()); return false; }
                out << std::setw(2) << ToJson() << std::endl;
                if (!out.good()) { std::filesystem::remove(tmp, ec); return false; }
            }
            std::filesystem::rename(tmp, path, ec);
            if (ec) { TraceLog(LOG_ERROR, "SETTINGS: cannot finalize %s", path.string().c_str()); return false; }
            dirty_ = false;
            return true;
        }

        /** @brief Reads settings.json and applies it (see Apply for the layering rule).
         *  A missing or unparsable file is not an error — authored defaults stand. */
        bool Load()
        {
            dirty_ = false;   // freshly loaded (or absent) = nothing to persist
            if (projectPath_.empty()) return false;
            std::ifstream f(std::filesystem::path(projectPath_) / "settings.json");
            if (!f.is_open()) return false;
            nlohmann::json j;
            try { f >> j; } catch (...) { TraceLog(LOG_WARNING, "SETTINGS: invalid JSON in settings.json"); return false; }
            Apply(j);
            return true;
        }

        /** @brief Live state -> JSON (public + static so tests can round-trip in memory). */
        static nlohmann::json ToJson()
        {
            nlohmann::json j;
            const auto& mix = AudioMixer::Get();
            j["audio"]["master"] = mix.master;
            for (const auto& [name, v] : mix.buses) j["audio"]["buses"][name] = v;
            j["input"]["actions"] = nlohmann::json::object();
            for (const auto& [name, b] : InputManager::Get().GetActions())
                j["input"]["actions"][name] = { {"key", b.key}, {"mouseBtn", b.mouseBtn}, {"useMouse", b.useMouse} };
            return j;
        }

        /** @brief JSON -> live state. Audio applies directly; input rebinds layer onto
         *  the existing action map (unknown action names are ignored, nothing is removed). */
        static void Apply(const nlohmann::json& j)
        {
            if (j.contains("audio"))
            {
                const auto& a = j["audio"];
                auto& mix = AudioMixer::Get();
                if (a.contains("master") && a["master"].is_number())
                    mix.master = std::clamp(a["master"].get<float>(), 0.0f, 1.0f);
                if (a.contains("buses") && a["buses"].is_object())
                    for (const auto& [name, v] : a["buses"].items())
                        if (v.is_number()) mix.SetBusVolume(name, v.get<float>());
            }
            if (j.contains("input") && j["input"].contains("actions") && j["input"]["actions"].is_object())
            {
                auto& actions = InputManager::Get().GetActions();
                for (const auto& [name, val] : j["input"]["actions"].items())
                {
                    auto it = actions.find(name);
                    if (it == actions.end() || !val.is_object()) continue;   // layer: never add
                    if (val.contains("key"))      it->second.key      = val["key"].get<int>();
                    if (val.contains("mouseBtn")) it->second.mouseBtn = val["mouseBtn"].get<int>();
                    if (val.contains("useMouse")) it->second.useMouse = val["useMouse"].get<bool>();
                }
            }
        }

    private:
        GameSettings()  = default;
        ~GameSettings() = default;

        std::string projectPath_;
        bool        dirty_ = false;
    };
}
