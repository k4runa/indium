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
        EntityFactory factory; // Used to instantiate entities when loading scenes

        /** @brief Gets the path to the global Indium preferences file. */
        std::string GetPrefsPath()
        {
            const char* homeDir = getenv("HOME");
            if (!homeDir) return "indium_prefs.json"; // Fallback to current dir if HOME is not set
            return std::string(homeDir) + "/.indium_prefs.json";
        }

    public:
        /** @brief Returns true if a project is currently open. */
        bool IsProjectOpen() const { return !currentProjectPath.empty(); }
        std::string GetCurrentProjectPath() const { return currentProjectPath; }
        std::string GetCurrentProjectName() const { return currentProjectName; }

        /** @brief Closes the current project. */
        void CloseProject()
        {
            currentProjectPath = "";
            currentProjectName = "";
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
                             << "#include \"NativeScript.hpp\"\n\n"
                             << "// This macro registers your scripts so the engine can instantiate them.\n"
                             << "INDIUM_EXPORT_SCRIPTS()\n";
                exportStream.close();

                // Generate a sample PlayerMovement script
                std::string sampleFile = (projectPath / "scripts" / "PlayerMovement.cpp").string();
                std::ofstream sampleStream(sampleFile);
                sampleStream << "/**\n"
                             << " * Indium Engine Sample Script\n"
                             << " * ---------------------------\n"
                             << " * Use OnStart() for initialization and OnUpdate() for logic per frame.\n"
                             << " */\n\n"
                             << "#include \"NativeScript.hpp\"\n"
                             << "#include \"raylib.h\"\n"
                             << "#include \"raymath.h\"\n\n"
                             << "class PlayerMovement : public Indium::NativeScript {\n"
                             << "public:\n"
                             << "    IND_PROP(float, Speed, 300.0f);\n\n"
                             << "    void OnStart() override {\n"
                             << "        // This runs once when the game starts\n"
                             << "        TraceLog(LOG_INFO, \"PlayerMovement: Hello Indium!\");\n"
                             << "    }\n\n"
                             << "    void OnUpdate(float dt) override {\n"
                             << "        // Simple movement logic using WASD\n"
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

                std::string defaultScenePath = indp["defaultScene"].get<std::string>();
                fs::path fullScenePath = projectPath / defaultScenePath;

                // Load Scene
                if (fs::exists(fullScenePath))
                {
                    std::ifstream si(fullScenePath);
                    json sj;
                    si >> sj;

                    outScene.entities.clear();
                    outScene.snapshot.clear();

                    if (sj.contains("worldSize"))
                    {
                        outScene.worldSize.x = sj["worldSize"][0];
                        outScene.worldSize.y = sj["worldSize"][1];
                    }

                    if (sj.contains("nextEntityId"))
                    {
                        outScene.nextEntityId = sj["nextEntityId"].get<int>();
                    }

                    if (sj.contains("entities"))
                    {
                        for (const auto& ej : sj["entities"])
                        {
                            auto entity = factory.LoadEntity(ej);
                            if (entity)
                            {
                                outScene.entities.push_back(std::move(entity));
                            }
                        }
                        outScene.RebuildHierarchy();
                    }
                    TraceLog(LOG_INFO, "PROJECT: Successfully loaded scene from '%s'", fullScenePath.c_str());
                }
                else
                {
                    TraceLog(LOG_WARNING, "PROJECT: Default scene not found: %s", fullScenePath.c_str());
                    // Not fatal, we just have an empty scene
                }

                // Add to recent
                AddRecentProject(path, currentProjectName);

                // Load Scripts
                ScriptManager::Get().LoadLibrary(path);

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
                fs::path sceneDir  = fs::path(currentProjectPath) / "Scenes";
                fs::path sceneFile = sceneDir / "main.scene";

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
    };
}
