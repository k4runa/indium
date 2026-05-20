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
            if (!homeDir) return "indium_prefs.json";
            return std::string(homeDir) + "/.indium_prefs.json";
        }

        /** @brief Populates outScene from an already-parsed scene JSON object. */
        void loadSceneFromJSON_(Scene& outScene, const json& sj)
        {
            if (sj.contains("worldSize"))
            {
                outScene.worldSize.x = sj["worldSize"][0];
                outScene.worldSize.y = sj["worldSize"][1];
            }
            if (sj.contains("editorCamera"))
            {
                outScene.editorCameraTarget.x = sj["editorCamera"][0];
                outScene.editorCameraTarget.y = sj["editorCamera"][1];
                outScene.editorCameraZoom     = sj["editorCamera"][2];
            }
            if (sj.contains("nextEntityId"))
                outScene.nextEntityId = sj["nextEntityId"].get<int>();
            if (sj.contains("entities"))
            {
                for (const auto& ej : sj["entities"])
                {
                    auto entity = factory.LoadEntity(ej);
                    if (entity) outScene.entities.push_back(std::move(entity));
                }
                outScene.RebuildHierarchy();
                factory.RebuildEntityCounts(outScene);
            }
            if (sj.contains("storyState"))
                outScene.storyState = StoryValueMapFromJson(sj["storyState"]);
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
                loadSceneFromJSON_(outScene, sj);
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
                if (j.contains("default_project_path"))
                {
                    return j["default_project_path"].get<std::string>();
                }
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
                        if ((*it)["path"] == path)
                        {
                            it = arr.erase(it);
                        }
                        else
                        {
                            ++it;
                        }
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

                // Generate a sample PlayerMovement script
                std::string sampleFile = (projectPath / "scripts" / "PlayerMovement.cpp").string();
                std::ofstream sampleStream(sampleFile);
                sampleStream << "#include \"IndiumEngine.hpp\"\n\n"
                             << "using namespace Indium;\n\n"
                             << "class PlayerMovement : public NativeScript {\n"
                             << "public:\n"
                             << "    IND_PROP(float, Speed, 300.0f);\n\n"
                             << "    void OnStart() override {\n"
                             << "        TraceLog(LOG_INFO, \"PlayerMovement: Hello Indium!\");\n"
                             << "    }\n\n"
                             << "    void OnUpdate(float dt) override {\n"
                             << "        Vector2 move = { 0, 0 };\n"
                             << "        if (IsKeyDown(KEY_W)) move.y -= 1;\n"
                             << "        if (IsKeyDown(KEY_S)) move.y += 1;\n"
                             << "        if (IsKeyDown(KEY_A)) move.x -= 1;\n"
                             << "        if (IsKeyDown(KEY_D)) move.x += 1;\n\n"
                             << "        if (Vector2Length(move) > 0) {\n"
                             << "            move = Vector2Normalize(move);\n"
                             << "            entity->position.x += move.x * Speed * dt;\n"
                             << "            entity->position.y += move.y * Speed * dt;\n"
                             << "        }\n"
                             << "    }\n"
                             << "};\n\n"
                             << "REGISTER_SCRIPT(PlayerMovement)\n";
                sampleStream.close();

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

                fs::path fullScenePath = projectPath / currentScenePath;

                // Load Scripts FIRST — must happen before LoadEntity so that
                // NativeScript components can be instantiated via InstantiateScript().
                // If no compiled library exists yet this is a no-op (returns false, logs warning).
                ScriptManager::Get().GenerateClangdConfig(path);
                ScriptManager::Get().LoadLibrary(path);

                // Load Scene
                if (fs::exists(fullScenePath))
                {
                    std::ifstream si(fullScenePath);
                    json sj;
                    si >> sj;

                    outScene.entities.clear();
                    outScene.snapshot.clear();
                    outScene.entityCounts.clear();
                    outScene.storyState.clear();
                    loadSceneFromJSON_(outScene, sj);

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

                // Ensure the Scenes directory exists (guards against external deletion)
                fs::create_directories(sceneDir);

                std::ofstream o(sceneFile);
                if (!o.is_open())
                {
                    TraceLog(LOG_ERROR, "PROJECT: Save failed — cannot open '%s' for writing.", sceneFile.c_str());
                    return false;
                }

                o << std::setw(4) << scene.serialize() << std::endl;
                o.flush();

                if (!o.good())
                {
                    TraceLog(LOG_ERROR, "PROJECT: Save failed — write error on '%s'.", sceneFile.c_str());
                    return false;
                }

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

            // Destroy live components
            for (auto& e : scene.entities)
                for (auto& c : e->components)
                    c->destroy(&scene);

            // Clear all runtime state
            scene.entities.clear();
            scene.startQueue.clear();
            scene.destroyQueue.clear();
            scene.fixedAccumulator    = 0.0f;
            scene._activeCollisionPairs.clear();
            scene._pendingSceneLoad.clear();
            Time::elapsed = 0.0f;

            // Resolve file path (strip extension if caller passed it)
            std::string name = sceneName;
            if (name.size() > 6 && name.compare(name.size() - 6, 6, ".scene") == 0)
                name = name.substr(0, name.size() - 6);

            fs::path scenePath = fs::path(currentProjectPath) / "Scenes" / (name + ".scene");
            if (!fs::exists(scenePath))
            {
                TraceLog(LOG_ERROR, "SCENE: File not found: %s", scenePath.string().c_str());
                return false;
            }

            try
            {
                std::ifstream si(scenePath);
                json sj;
                si >> sj;
                loadSceneFromJSON_(scene, sj);

                for (auto& e : scene.entities)
                    for (auto& c : e->components)
                        c->start(&scene);

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
