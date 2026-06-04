#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include "scene/Scene.hpp"
#include "../2D/entity/EntityFactory.hpp"
#include "../include/nlohmann/json.hpp"
#include "raylib.h"
#include "ScriptManager.hpp"
#include "AssetManager.hpp"
#include "TagRegistry.hpp"
#include "DialogueManager.hpp"
#include <cstdlib> // for getenv

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace Indium
{
    /**
     * @brief Represents a recently opened project.
     */
    struct RecentProject
    {
        std::string name;
        std::string path;
        std::string lastOpened; // Could be a timestamp string
    };

    /**
     * @brief Manages the lifecycle of an Indium project on disk.
     */
    class ProjectManager
    {
    private:
        std::string currentProjectPath = "";
        std::string currentProjectName = "";
        std::string currentScenePath  = "Scenes/main.scene"; // relative to project root
        EntityFactory factory; // Used to instantiate entities when loading scenes

        /** @brief Gets the path to the global Indium preferences file. */
        std::string GetPrefsPath()
        {
            const char* homeDir = getenv("HOME");
            if (!homeDir) homeDir = getenv("USERPROFILE"); // Windows fallback
            if (!homeDir) return "indium_prefs.json";
            return std::string(homeDir) + "/.indium_prefs.json";
        }

    public:
        /** @brief Returns true if a project is currently open. */
        bool IsProjectOpen() const { return !currentProjectPath.empty(); }
        std::string GetCurrentProjectPath() const { return currentProjectPath; }
        std::string GetCurrentProjectName() const { return currentProjectName; }
        std::string GetCurrentSceneName() const
        {
            return fs::path(currentScenePath).stem().string();
        }

        /** @brief Full project-relative path of the active scene (e.g. "Scenes/main.scene"). */
        std::string GetCurrentScenePath() const { return currentScenePath; }

        /** @brief Overrides the active scene path. The editor uses this to restore the
         *  pre-Play scene on Stop when a script switched scenes mid-Play — otherwise a
         *  later Save would write the restored snapshot over the switched-to scene file. */
        void SetCurrentScenePath(const std::string& path) { currentScenePath = path; }

        /** @brief Returns the default startup scene filename (e.g. "main.scene") from project.indp. */
        std::string GetDefaultSceneName() const
        {
            if (currentProjectPath.empty()) return "";
            fs::path indpPath = fs::path(currentProjectPath) / "project.indp";
            if (!fs::exists(indpPath)) return "";
            try
            {
                std::ifstream f(indpPath);
                json j; f >> j;
                if (j.contains("defaultScene"))
                {
                    return fs::path(j["defaultScene"].get<std::string>()).filename().string();
                }
            }
            catch (...) {}
            return "";
        }

        /** @brief Updates the default startup scene in project.indp. */
        bool SetDefaultScene(const std::string& sceneFileName)
        {
            if (currentProjectPath.empty()) return false;
            fs::path indpPath = fs::path(currentProjectPath) / "project.indp";
            try
            {
                json j;
                if (fs::exists(indpPath))
                {
                    std::ifstream f(indpPath); f >> j;
                }
                j["defaultScene"] = "Scenes/" + sceneFileName;
                std::ofstream o(indpPath);
                o << std::setw(4) << j << std::endl;
                TraceLog(LOG_INFO, "PROJECT: Default scene set to '%s'", sceneFileName.c_str());
                return true;
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "PROJECT: Failed to set default scene: %s", e.what());
                return false;
            }
        }

        /** @brief Reads the project tag list from project.indp. */
        std::vector<std::string> GetProjectTags() const
        {
            return TagRegistry::Get().GetTags();
        }

        /** @brief Persists tag list to project.indp and updates TagRegistry. */
        bool SaveProjectTags(const std::vector<std::string>& tags)
        {
            if (currentProjectPath.empty()) return false;
            fs::path indpPath = fs::path(currentProjectPath) / "project.indp";
            try
            {
                json j;
                if (fs::exists(indpPath)) { std::ifstream f(indpPath); f >> j; }
                json tagsJ = json::array();
                for (const auto& t : tags) tagsJ.push_back(t);
                j["tags"] = tagsJ;
                std::ofstream o(indpPath);
                o << std::setw(4) << j << std::endl;
                TagRegistry::Get().SetTags(tags);
                return true;
            }
            catch (...) { return false; }
        }

        /** @brief Updates the project name in project.indp. */
        bool SetProjectName(const std::string& newName)
        {
            if (currentProjectPath.empty() || newName.empty()) return false;
            fs::path indpPath = fs::path(currentProjectPath) / "project.indp";
            try
            {
                json j;
                if (fs::exists(indpPath))
                {
                    std::ifstream f(indpPath); f >> j;
                }
                j["projectName"] = newName;
                currentProjectName = newName;
                std::ofstream o(indpPath);
                o << std::setw(4) << j << std::endl;
                return true;
            }
            catch (...) {}
            return false;
        }

        /** @brief Lists all .scene files inside the project's Scenes/ folder. */
        std::vector<std::string> GetSceneList() const
        {
            std::vector<std::string> scenes;
            if (currentProjectPath.empty()) return scenes;
            fs::path scenesDir = fs::path(currentProjectPath) / "Scenes";
            if (!fs::exists(scenesDir)) return scenes;
            for (const auto& entry : fs::directory_iterator(scenesDir))
            {
                if (entry.path().extension() == ".scene")
                {
                    scenes.push_back(entry.path().filename().string());
                }
            }
            std::sort(scenes.begin(), scenes.end());
            return scenes;
        }

        /** @brief Renames a scene file. Updates currentScenePath if it was the active scene. */
        bool RenameScene(const std::string& oldFileName, const std::string& newName)
        {
            if (currentProjectPath.empty()) return false;

            fs::path oldPath = fs::path(currentProjectPath) / "Scenes" / oldFileName;
            fs::path newPath = fs::path(currentProjectPath) / "Scenes" / (newName + ".scene");

            if (!fs::exists(oldPath))
            {
                TraceLog(LOG_ERROR, "PROJECT: Rename failed — '%s' not found.", oldFileName.c_str());
                return false;
            }
            if (fs::exists(newPath))
            {
                TraceLog(LOG_ERROR, "PROJECT: Rename failed — '%s' already exists.", newName.c_str());
                return false;
            }

            fs::rename(oldPath, newPath);

            if (fs::path(currentScenePath).filename().string() == oldFileName)
            {
                currentScenePath = "Scenes/" + newName + ".scene";
            }
            TraceLog(LOG_INFO, "PROJECT: Renamed '%s' → '%s'", oldFileName.c_str(), newName.c_str());
            return true;
        }

        /** @brief Permanently deletes a scene file. Cannot delete the currently active scene. */
        bool DeleteScene(const std::string& sceneFileName)
        {
            if (currentProjectPath.empty()) return false;

            if (fs::path(currentScenePath).filename().string() == sceneFileName)
            {
                TraceLog(LOG_ERROR, "PROJECT: Cannot delete the currently active scene.");
                return false;
            }

            fs::path scenePath = fs::path(currentProjectPath) / "Scenes" / sceneFileName;
            if (!fs::exists(scenePath))
            {
                TraceLog(LOG_ERROR, "PROJECT: Delete failed — '%s' not found.", sceneFileName.c_str());
                return false;
            }

            fs::remove(scenePath);
            TraceLog(LOG_INFO, "PROJECT: Deleted scene '%s'", sceneFileName.c_str());
            return true;
        }

        /** @brief Loads a specific scene by filename (e.g. "level2.scene") into outScene. */
        bool SwitchScene(const std::string& sceneFileName, Scene& outScene)
        {
            if (currentProjectPath.empty()) return false;
            fs::path fullPath = fs::path(currentProjectPath) / "Scenes" / sceneFileName;
            if (!fs::exists(fullPath))
            {
                TraceLog(LOG_ERROR, "PROJECT: Scene not found: %s", fullPath.c_str());
                return false;
            }

            outScene.entities.clear();
            outScene.snapshot.clear();
            outScene.nextEntityId = 1;
            outScene.entityCounts.clear();
            outScene.storyState.clear();

            try
            {
                std::ifstream si(fullPath);
                json sj;
                si >> sj;
                outScene.deserialize(sj, factory);
                outScene.name    = fs::path(sceneFileName).stem().string();
                currentScenePath = "Scenes/" + sceneFileName;
                TraceLog(LOG_INFO, "PROJECT: Switched to scene '%s'", sceneFileName.c_str());
                return true;
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "PROJECT: Failed to switch scene: %s", e.what());
                return false;
            }
        }

        /** @brief Creates a new empty scene file and switches to it. */
        bool CreateNewScene(const std::string& sceneName, Scene& outScene)
        {
            if (currentProjectPath.empty()) return false;

            fs::path scenesDir = fs::path(currentProjectPath) / "Scenes";
            fs::path scenePath = scenesDir / (sceneName + ".scene");

            if (fs::exists(scenePath))
            {
                TraceLog(LOG_ERROR, "PROJECT: Scene '%s' already exists.", sceneName.c_str());
                return false;
            }

            try
            {
                fs::create_directories(scenesDir);

                Scene empty;
                empty.worldSize = { 1920, 1080 };
                std::ofstream o(scenePath);
                o << std::setw(4) << empty.serialize() << std::endl;
                o.close();

                outScene.entities.clear();
                outScene.snapshot.clear();
                outScene.nextEntityId = 1;
                outScene.entityCounts.clear();
                outScene.worldSize = { 1920, 1080 };
                // A fresh scene must not inherit the previous one's authored state
                // (matches SwitchScene / LoadProject, which clear these on load).
                outScene.storyState.clear();
                outScene.parallaxEnabled = false;
                outScene.parallaxByLayer.clear();
                outScene.parallaxAnchor  = { 0.0f, 0.0f };
                outScene.name            = sceneName;

                currentScenePath = "Scenes/" + sceneName + ".scene";
                TraceLog(LOG_INFO, "PROJECT: Created new scene '%s'", sceneName.c_str());
                return true;
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "PROJECT: Failed to create scene: %s", e.what());
                return false;
            }
        }

        /** @brief Closes the current project. */
        void CloseProject()
        {
            currentProjectPath = "";
            currentProjectName = "";
            currentScenePath   = "Scenes/main.scene";
            ScriptManager::Get().SetActiveProjectPath("");
            AssetManager::Get().Clear();
            TagRegistry::Get().Reset();
        }

        std::string GetDefaultProjectPath()
        {
            std::string prefsPath = GetPrefsPath();
            if (!fs::exists(prefsPath)) return "";

            try
            {
                std::ifstream i(prefsPath);
                json j;
                i >> j;
                if (j.contains("default_project_path")) { return j["default_project_path"].get<std::string>(); }
            }
            catch (...) {}
            return "";
        }

        void SetDefaultProjectPath(const std::string& path)
        {
            std::string prefsPath = GetPrefsPath();
            json j;

            if (fs::exists(prefsPath))
            {
                try
                {
                    std::ifstream i(prefsPath);
                    i >> j;
                } catch (...) {}
            }

            j["default_project_path"] = path;

            std::ofstream o(prefsPath);
            o << std::setw(4) << j << std::endl;
        }

        /** @brief Reads the list of recent projects from the global preferences. */
        std::vector<RecentProject> GetRecentProjects()
        {
            std::vector<RecentProject> recents;
            std::string prefsPath = GetPrefsPath();

            if (!fs::exists(prefsPath)) return recents;

            try
            {
                std::ifstream i(prefsPath);
                json j;
                i >> j;

                if (j.contains("recent_projects"))
                {
                    for (const auto& item : j["recent_projects"])
                    {
                        RecentProject rp;
                        rp.name = item["name"];
                        rp.path = item["path"];
                        if (item.contains("lastOpened")) rp.lastOpened = item["lastOpened"];
                        recents.push_back(rp);
                    }
                }
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "PROJECT: Failed to read recent projects: %s", e.what());
            }

            return recents;
        }

        /** @brief Adds a project to the recent projects list. */
        void AddRecentProject(const std::string& path, const std::string& name)
        {
            std::string prefsPath = GetPrefsPath();
            json j;

            if (fs::exists(prefsPath))
            {
                try
                {
                    std::ifstream i(prefsPath);
                    i >> j;
                } catch (...) {}
            }

            // Remove if already exists so we can move it to the top
            if (j.contains("recent_projects"))
            {
                auto& arr = j["recent_projects"];
                for (auto it = arr.begin(); it != arr.end(); )
                {
                    if ((*it)["path"] == path)
                    {
                        it = arr.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            else
            {
                j["recent_projects"] = json::array();
            }

            // Create new entry
            json newEntry;
            newEntry["name"] = name;
            newEntry["path"] = path;

            // Get current time
            time_t now = time(0);
            char dt[64];
            strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", localtime(&now));
            newEntry["lastOpened"] = std::string(dt);

            // Insert at the beginning
            j["recent_projects"].insert(j["recent_projects"].begin(), newEntry);

            // Keep only top 10
            if (j["recent_projects"].size() > 10)
            {
                j["recent_projects"].erase(j["recent_projects"].begin() + 10, j["recent_projects"].end());
            }

            // Save back
            std::ofstream o(prefsPath);
            o << std::setw(4) << j << std::endl;
        }

        /** @brief Removes a project from the recent list (e.g. if the folder was deleted). */
        void RemoveRecentProject(const std::string& path)
        {
            std::string prefsPath = GetPrefsPath();
            if (!fs::exists(prefsPath)) return;

            try
            {
                std::ifstream i(prefsPath);
                json j;
                i >> j;

                if (j.contains("recent_projects"))
                {
                    auto& arr = j["recent_projects"];
                    for (auto it = arr.begin(); it != arr.end(); )
                    {
                        if ((*it)["path"] == path) { it = arr.erase(it); }
                        else { ++it; }
                    }
                    std::ofstream o(prefsPath);
                    o << std::setw(4) << j << std::endl;
                }
            }
            catch (...) {}
        }

        /**
         * @brief Creates a new Indium project skeleton at the given directory.
         *
         * @param parentPath The directory where the project folder will be created.
         * @param name The name of the project.
         * @return true if successful, false otherwise.
         */
        bool CreateProject(const std::string& parentPath, const std::string& name)
        {
            fs::path projectPath = fs::path(parentPath) / name;

            if (fs::exists(projectPath))
            {
                TraceLog(LOG_ERROR, "PROJECT: A directory with this name already exists at the specified location.");
                return false;
            }

            try
            {
                // 1. Create directory structure
                fs::create_directories(projectPath);
                fs::create_directories(projectPath / "Assets");
                fs::create_directories(projectPath / "Settings");
                fs::create_directories(projectPath / "Scenes");
                fs::create_directories(projectPath / "scripts");
                // Generate export file (required for the engine to find the scripts)
                std::string exportFile = (projectPath / "scripts" / "IndiumExports.cpp").string();
                std::ofstream exportStream(exportFile);
                exportStream << "/* Auto-generated Indium Export File */\n"
                             << "#include \"IndiumEngine.hpp\"\n\n"
                             << "// This macro registers your scripts so the engine can instantiate them.\n"
                             << "INDIUM_EXPORT_SCRIPTS()\n";
                exportStream.close();

                // Generate a sample PlayerMovement script: a physics-driven side-scrolling
                // platformer controller. Written as a raw string literal so the template
                // reads exactly like the file it produces.
                std::string sampleFile = (projectPath / "scripts" / "PlayerMovement.cpp").string();
                std::ofstream sampleStream(sampleFile);
                sampleStream << R"SCRIPT(#include "IndiumEngine.hpp"

using namespace Indium;

// Side-scrolling platformer controller.
//
//   Move:  A / D   or   Left / Right arrows
//   Jump:  Space   (only while standing on the ground)
//
// Movement is applied through the physics velocity (entity->velocity), NOT by
// writing entity->position directly. Letting the Rigidbody integrate the motion
// means gravity still pulls the player down and the engine resolves collisions,
// so the character can no longer tunnel through walls or floors.
//
// The entity needs a Collider (a Circle/Rectangle already has one). A dynamic
// Rigidbody is added automatically on start if the entity doesn't already have one.
class PlayerMovement : public NativeScript
{
public:
    IND_PROP(float, MoveSpeed,    400.0f);        // top horizontal speed in pixels/second
    IND_PROP(float, Acceleration, 12.0f);         // how quickly we ramp to/from top speed (higher = snappier, lower = floatier)
    IND_PROP(float, JumpForce,    700.0f);        // upward impulse applied on jump
    IND_PROP(float, MaxFallSpeed, 1200.0f);       // terminal velocity — caps fall speed so fast drops can't tunnel through thin floors
    IND_PROP(float, CoyoteTime,   0.10f);         // grace period (sec) after leaving a ledge where a jump still works
    IND_PROP(float, JumpBufferTime, 0.10f);       // window (sec) before landing where an early jump press is remembered
    IND_PROP(float, JumpCutMultiplier, 0.40f);    // how much upward speed is kept when Space is released early (variable jump height)
    IND_PROP(float, GroundCheckDistance, 6.0f);   // how far below the feet to look for ground

    void OnStart() override
    {
        // Make sure the body can fall and collide. Respect an existing Rigidbody so
        // the user's own mass / gravity / drag settings are never overwritten.
        if (!GetComponent<RigidbodyComponent>())
        {
            RigidbodyComponent* rb = AddComponent<RigidbodyComponent>();
            rb->freezeRotation = true;   // keep the character standing upright
        }
    }

    void OnUpdate(float dt) override
    {
        if (!entity) return;

        bool grounded = IsGrounded();

        // Coyote time: keep a small countdown that is refilled while grounded, so a jump
        // pressed just after walking off a ledge still fires.
        coyoteTimer_ = grounded ? CoyoteTime : (coyoteTimer_ - dt);

        // --- Horizontal movement: A / D or the arrow keys ---
        float direction = 0.0f;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  direction -= 1.0f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) direction += 1.0f;

        // Ease the X velocity toward the target instead of snapping to it: this gives a
        // short ramp up to speed and a slide to a stop, so movement has weight and inertia
        // rather than looking like the position is teleporting. Y is left to gravity so
        // jumping and falling stay natural. Clamp keeps it stable at low frame rates.
        float targetVelocityX = direction * MoveSpeed;
        entity->velocity.x = Lerp(entity->velocity.x, targetVelocityX, Clamp(Acceleration * dt, 0.0f, 1.0f));

        // Jump buffering: remember a press for a short window so a jump tapped just before
        // landing still fires the moment we touch down.
        if (IsKeyPressed(KEY_SPACE)) jumpBufferTimer_ = JumpBufferTime;
        else                         jumpBufferTimer_ -= dt;

        // --- Jump: fires when a buffered press lines up with the coyote window ---
        if (jumpBufferTimer_ > 0.0f && coyoteTimer_ > 0.0f)
        {
            entity->velocity.y = -JumpForce;
            jumpBufferTimer_   = 0.0f;   // consume the press
            coyoteTimer_       = 0.0f;   // consume the grace window (no double jumps)
        }

        // Variable jump height: releasing Space while still rising cuts the jump short,
        // so a tap is a short hop and a hold is a full jump.
        if (IsKeyReleased(KEY_SPACE) && entity->velocity.y < 0.0f)
        {
            entity->velocity.y *= JumpCutMultiplier;
        }

        // Terminal velocity: cap downward speed so a long fall can't move so far in one
        // physics step that it passes straight through a thin platform.
        if (entity->velocity.y > MaxFallSpeed) entity->velocity.y = MaxFallSpeed;
    }

private:
    float coyoteTimer_     = 0.0f;   // counts down after leaving the ground
    float jumpBufferTimer_ = 0.0f;   // counts down after an early jump press

    // Grounded if a thin probe box just beneath the feet overlaps a SOLID entity.
    // Trigger colliders (pickups, zones) are skipped — you can't stand on them.
    bool IsGrounded()
    {
        ::Rectangle bounds = entity->getBounds();
        Vector2 probeCenter = { bounds.x + bounds.width * 0.5f,
                                bounds.y + bounds.height + GroundCheckDistance * 0.5f };
        Vector2 probeSize   = { bounds.width * 0.9f, GroundCheckDistance };

        for (Entity* hit : OverlapBox(probeCenter, probeSize))
        {
            if (hit == entity) continue;
            Collider2D* col = hit->getComponent<Collider2D>();
            if (col && col->isTrigger) continue;   // trigger zones are not solid ground
            return true;
        }
        return false;
    }
};

REGISTER_SCRIPT(PlayerMovement)
)SCRIPT";
                sampleStream.close();

                // Generate a sample dialogue: a short branching conversation that
                // showcases choices, requireFlag-gated branches, setFlag, and narration.
                // Written in the exact format DialogueManager parses, so it opens in the
                // editor's Dialogue panel and runs via DialogueManager::Start("intro")
                // (or an InteractableComponent whose Dialogue Id is "intro").
                fs::create_directories(projectPath / "dialogue");
                std::ofstream dialogueStream(projectPath / "dialogue" / "intro.json");
                dialogueStream << R"DLG({
    "start": "greet",
    "nodes": {
        "greet": {
            "speaker": "Guide",
            "text": "Welcome to Indium! Have we spoken before?",
            "choices": [
                { "text": "We have — good to be back.", "next": "welcome_back", "requireFlag": "met_guide" },
                { "text": "No, this is my first time.",       "next": "intro",        "setFlag": "met_guide" },
                { "text": "[Say nothing]",                    "next": "" }
            ]
        },
        "intro": {
            "speaker": "Guide",
            "text": "Then let me show you around. This whole conversation is just data — open the Dialogue tab to see and edit it.",
            "next": "outro"
        },
        "welcome_back": {
            "speaker": "Guide",
            "text": "Always a pleasure. You set the 'met_guide' flag last time — that's why this line only appears on a return visit.",
            "next": "outro"
        },
        "outro": {
            "speaker": "Guide",
            "text": "Edit me in the Dialogue panel, then press Play and 'Preview in Viewport' to run it.",
            "next": ""
        }
    }
}
)DLG";
                dialogueStream.close();

                // 2. Create project.indp
                json indp;
                indp["projectName"] = name;
                indp["engineVersion"] = "1.0.0";
                indp["defaultScene"] = "Scenes/main.scene";

                std::ofstream indpFile(projectPath / "project.indp");
                indpFile << std::setw(4) << indp << std::endl;
                indpFile.close();

                // 3. Create an empty default scene
                Scene defaultScene;
                defaultScene.worldSize = {1920, 1080}; // Default size

                std::ofstream sceneFile(projectPath / "Scenes" / "main.scene");
                sceneFile << std::setw(4) << defaultScene.serialize() << std::endl;
                sceneFile.close();

                ScriptManager::Get().GenerateClangdConfig(projectPath.string());

                TraceLog(LOG_INFO, "PROJECT: Successfully created project '%s'", name.c_str());
                return true;
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "PROJECT: Failed to create project structure: %s", e.what());
                return false;
            }
        }

        /**
         * @brief Loads a project and populates the given Scene object.
         */
        bool LoadProject(const std::string& path, Scene& outScene)
        {
            fs::path projectPath(path);
            fs::path indpFilePath = projectPath / "project.indp";

            if (!fs::exists(indpFilePath))
            {
                TraceLog(LOG_ERROR, "PROJECT: Valid project not found at path: %s", path.c_str());
                return false;
            }

            try
            {
                // Read Project File
                std::ifstream i(indpFilePath);
                json indp;
                i >> indp;

                currentProjectName = indp["projectName"].get<std::string>();
                currentProjectPath = path;
                currentScenePath   = indp["defaultScene"].get<std::string>();
                ScriptManager::Get().SetActiveProjectPath(path);

                if (indp.contains("tags"))
                {
                    std::vector<std::string> tags;
                    for (const auto& t : indp["tags"]) tags.push_back(t.get<std::string>());
                    TagRegistry::Get().SetTags(tags);
                }
                else
                {
                    TagRegistry::Get().Reset();
                }

                fs::path fullScenePath = projectPath / currentScenePath;

                // Tear down any prior scene state BEFORE LoadLibrary. Live NativeScript
                // instances hold vtable/code pointers into the currently-loaded dylib,
                // and LoadLibrary dlclose's it — running their destructors after that
                // is use-after-free. Callers today gate this through CloseProject, but
                // doing it here makes the contract independent of caller discipline.
                outScene.entities.clear();
                outScene.snapshot.clear();
                outScene.startQueue.clear();
                outScene.destroyQueue.clear();
                outScene._activeCollisionPairs.clear();
                outScene._pendingSceneLoad.clear();
                outScene.entityCounts.clear();
                outScene.storyState.clear();

                // Load Scripts FIRST — must happen before LoadEntity so that
                // NativeScript components can be instantiated via InstantiateScript().
                // If no compiled library exists yet this is a no-op (returns false, logs warning).
                ScriptManager::Get().GenerateClangdConfig(path);

                // A script library built against an older engine has a mismatched vtable layout;
                // loading it and calling a virtual (e.g. deserialize during LoadEntity)
                // dispatches to the wrong slot and segfaults. Recompile when stale so we
                // only ever load a library that matches the current engine ABI.
                // Reset the surfaced-error state for this load; set it only on failure.
                ScriptManager::Get().lastAutoCompileFailed = false;
                ScriptManager::Get().lastAutoCompileLog.clear();
                if (ScriptManager::Get().ScriptsAreStale(path))
                {
                    std::string compileLog;
                    if (ScriptManager::Get().CompileScripts(path, compileLog))
                        TraceLog(LOG_INFO, "PROJECT: Recompiled out-of-date scripts for '%s'", path.c_str());
                    else
                    {
                        // Surface to the editor (console + banner) — otherwise the only
                        // symptom the user sees is "my script has no properties".
                        ScriptManager::Get().lastAutoCompileFailed = true;
                        ScriptManager::Get().lastAutoCompileLog = compileLog;
                        TraceLog(LOG_WARNING, "PROJECT: Script recompile failed; scripts disabled for this session.\n%s", compileLog.c_str());
                    }
                }
                ScriptManager::Get().LoadLibrary(path);

                // Load Scene
                if (fs::exists(fullScenePath))
                {
                    std::ifstream si(fullScenePath);
                    json sj;
                    si >> sj;

                    outScene.deserialize(sj, factory);
                    outScene.name = fs::path(currentScenePath).stem().string();

                    TraceLog(LOG_INFO, "PROJECT: Successfully loaded scene from '%s'", fullScenePath.c_str());
                }
                else
                {
                    TraceLog(LOG_WARNING, "PROJECT: Default scene not found: %s", fullScenePath.c_str());
                }

                // Add to recent
                AddRecentProject(path, currentProjectName);

                return true;
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "PROJECT: Exception while loading project: %s", e.what());
                CloseProject();
                return false;
            }
        }

        /**
         * @brief Saves the current state of the scene to the active project.
         */
        bool SaveCurrentProject(const Scene& scene)
        {
            if (currentProjectPath.empty())
            {
                TraceLog(LOG_WARNING, "PROJECT: Save failed — no project is open.");
                return false;
            }

            try
            {
                fs::path sceneFile = fs::path(currentProjectPath) / currentScenePath;
                fs::path sceneDir  = sceneFile.parent_path();
                fs::path tmpFile   = sceneFile;
                tmpFile += ".tmp";

                // Ensure the Scenes directory exists (guards against external deletion)
                fs::create_directories(sceneDir);

                // Write to a temp file first so a crash mid-write never corrupts the real scene.
                {
                    std::ofstream o(tmpFile);
                    if (!o.is_open())
                    {
                        TraceLog(LOG_ERROR, "PROJECT: Save failed — cannot open '%s' for writing.", tmpFile.c_str());
                        return false;
                    }

                    o << std::setw(4) << scene.serialize() << std::endl;
                    o.flush();

                    if (!o.good())
                    {
                        TraceLog(LOG_ERROR, "PROJECT: Save failed — write error on '%s'.", tmpFile.c_str());
                        fs::remove(tmpFile);
                        return false;
                    }
                }

                // Atomic rename: replaces sceneFile only after the full write succeeded.
                fs::rename(tmpFile, sceneFile);

                TraceLog(LOG_INFO, "PROJECT: Saved %d entities to '%s'",
                    (int)scene.entities.size(), sceneFile.c_str());
                return true;
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "PROJECT: Failed to save scene: %s", e.what());
                return false;
            }
        }

        /**
         * @brief Loads a different scene file into the running scene without leaving Play mode.
         *
         * Called by Editor::Update when scene._pendingSceneLoad is set.
         * Destroys all current entities, loads the new scene from disk, and starts all components.
         *
         * @param scene       The active scene to replace.
         * @param sceneName   Scene filename without path. ".scene" extension is optional.
         *                    Example: "level2"  →  <project>/Scenes/level2.scene
         */
        bool SwitchScene(Scene& scene, const std::string& sceneName)
        {
            if (currentProjectPath.empty())
            {
                TraceLog(LOG_WARNING, "SCENE: SwitchScene — no project open.");
                return false;
            }

            // Claim any queued saved-game restore (SaveManager::Load) up front so it is
            // consumed exactly once — regardless of whether the load below succeeds — and
            // can never leak into a later, unrelated scene switch. (The drain that calls us
            // has already cleared _pendingSceneLoad, so every early return below is past the
            // point of no return for this payload.) A normal LoadScene leaves these empty.
            const bool                           hasRestore       = scene._hasPendingRestore;
            std::map<std::string, StoryValue>    restoreStory     = std::move(scene._pendingStoryRestore);
            std::vector<std::pair<int, Vector2>> restorePositions = std::move(scene._pendingPositionRestore);
            scene._hasPendingRestore = false;
            scene._pendingStoryRestore.clear();
            scene._pendingPositionRestore.clear();

            // Resolve + validate the target BEFORE tearing anything down, so a bad name
            // (e.g. a save that references a since-deleted scene) can't leave the live
            // scene wiped and empty.
            std::string name = sceneName;
            if (name.size() > 6 && name.compare(name.size() - 6, 6, ".scene") == 0)
                name = name.substr(0, name.size() - 6);

            fs::path scenePath = fs::path(currentProjectPath) / "Scenes" / (name + ".scene");
            if (!fs::exists(scenePath))
            {
                TraceLog(LOG_ERROR, "SCENE: File not found: %s", scenePath.string().c_str());
                return false;
            }

            // A dialogue from the outgoing scene must not bleed into the new one.
            DialogueManager::Get().End();

            // Destroy live components
            for (auto& e : scene.entities) for (auto& c : e->components) c->destroy(&scene);

            // Clear all runtime state
            scene.entities.clear();
            scene.startQueue.clear();
            scene.destroyQueue.clear();
            scene.fixedAccumulator    = 0.0f;
            scene._activeCollisionPairs.clear();
            scene._pendingSceneLoad.clear();
            Time::elapsed = 0.0f;
            Time::delta   = 0.0f;

            try
            {
                std::ifstream si(scenePath);
                json sj;
                si >> sj;
                scene.deserialize(sj, factory);
                scene.name = name;

                // Apply the new scene's authored starting flags on top of the global
                // blackboard, which persists across scene switches by design.
                StoryState::Get().Seed(scene.storyState);

                // A queued saved-game restore overrides the authored defaults. It runs
                // after the seed but before awake/start so the loaded scene's scripts
                // observe the saved flags and player positions.
                if (hasRestore)
                {
                    StoryState::Get().Clear();
                    StoryState::Get().Seed(restoreStory);
                    for (const auto& [id, pos] : restorePositions)
                        if (Entity* e = scene.FindEntity(id)) e->position = pos;
                }

                // Snapshot component pointers first: a script OnStart() may AddComponent<>(),
                // reallocating e->components mid-iteration (dangling iterator → crash). Heap
                // Component objects don't move, so cached raw pointers stay valid.
                std::vector<Component*> startComps;
                for (auto& e : scene.entities) for (auto& c : e->components) startComps.push_back(c.get());
                for (auto* c : startComps) c->awake(&scene);
                for (auto* c : startComps) c->start(&scene);

                currentScenePath = "Scenes/" + name + ".scene";
                TraceLog(LOG_INFO, "SCENE: Switched to '%s'", name.c_str());
                return true;
            }
            catch (const std::exception& ex)
            {
                TraceLog(LOG_ERROR, "SCENE: Failed to load '%s': %s", name.c_str(), ex.what());
                return false;
            }
        }
    };
}
