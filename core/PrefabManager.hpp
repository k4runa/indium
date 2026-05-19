#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include "../include/nlohmann/json.hpp"

namespace fs = std::filesystem;

namespace Indium
{
    class PrefabManager
    {
    public:
        static std::string GetPrefabDir(const std::string& projectPath)
        {
            return projectPath + "/prefabs";
        }

        /** @brief Serializes entityJson and writes it to <projectPath>/prefabs/<name>.prefab. */
        static bool Save(const nlohmann::json& entityJson, const std::string& projectPath, const std::string& name)
        {
            std::string dir = GetPrefabDir(projectPath);
            if (!fs::exists(dir)) fs::create_directories(dir);

            std::ofstream f(dir + "/" + name + ".prefab");
            if (!f.is_open()) return false;
            f << entityJson.dump(4);
            return true;
        }

        /** @brief Reads and returns the entity JSON from a .prefab file path. */
        static nlohmann::json Load(const std::string& prefabPath)
        {
            std::ifstream f(prefabPath);
            if (!f.is_open()) return {};
            try
            {
                nlohmann::json j;
                f >> j;
                return j;
            }
            catch (...) { return {}; }
        }
    };

} // namespace Indium
