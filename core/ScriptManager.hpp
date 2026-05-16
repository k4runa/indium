#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <dlfcn.h>
#include <unistd.h>
#include <climits>
#include "NativeScript.hpp"

namespace fs = std::filesystem;

namespace Indium {

    class ScriptManager {
    private:
        void* libraryHandle = nullptr;
        std::string currentLibPath;

        static std::string GetEngineRoot()
        {
            char buf[PATH_MAX];
            ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
            if (len > 0)
            {
                buf[len] = '\0';
                return fs::path(buf).parent_path().parent_path().string();
            }
            return fs::current_path().string();
        }
        std::vector<std::string> availableScripts;

        typedef void (*GetScriptNamesFunc)(const char***, int*);
        typedef Component* (*CreateScriptFunc)(const char*);

        GetScriptNamesFunc getNamesFunc = nullptr;
        CreateScriptFunc createFunc = nullptr;

    public:
        ScriptManager() {
            NativeScript::InstantiateCallback = [this](const std::string& name) {
                return this->InstantiateScript(name);
            };
        }
        ~ScriptManager()
        {
            UnloadLibrary();
        }

        static ScriptManager& Get()
        {
            static ScriptManager instance;
            return instance;
        }

        bool CompileScripts(const std::string& projectPath, std::string& outLog) {
            std::string scriptsDir = projectPath + "/scripts";
            if (!fs::exists(scriptsDir))
            {
                fs::create_directory(scriptsDir);
                // Create a dummy export file if empty
                std::string exportFile = scriptsDir + "/IndiumExports.cpp";
                if (!fs::exists(exportFile))
                {
                    FILE* f = fopen(exportFile.c_str(), "w");
                    fprintf(f, "#include \"IndiumEngine.hpp\"\nINDIUM_EXPORT_SCRIPTS()\n");
                    fclose(f);
                }
            }

            // Clean up old cached libraries to prevent clutter
            for (const auto& entry : fs::directory_iterator(projectPath))
            {
                if (entry.path().filename().string().find("libscripts_") == 0 && entry.path().extension() == ".so")
                {
                    fs::remove(entry.path());
                }
            }

            std::string timeStamp = std::to_string(std::time(nullptr));
            currentLibPath = projectPath + "/libscripts_" + timeStamp + ".so";

            // Collect all .cpp files in scriptsDir, quoting paths to handle spaces
            std::string cppFiles;
            for (const auto& entry : fs::directory_iterator(scriptsDir))
            {
                if (entry.path().extension() == ".cpp")
                {
                    cppFiles += "\"" + entry.path().string() + "\" ";
                }
            }

            if (cppFiles.empty())
            {
                outLog = "No scripts to compile.";
                return false;
            }

            std::string engineRoot = GetEngineRoot();
            // Append 2>&1 to capture stderr
            std::string cmd = "g++ -shared -fPIC -std=c++20 " + cppFiles +
                              " -I\"" + engineRoot + "/core\""    +
                              " -I\"" + engineRoot + "/2D\""      +
                              " -I\"" + engineRoot + "/include\"" +
                              " -o \"" + currentLibPath + "\" 2>&1";

            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe)
            {
                outLog = "Failed to start compilation process (popen failed).";
                return false;
            }

            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
            {
                outLog += buffer;
            }

            int result = pclose(pipe);
            if (result != 0)
            {
                outLog += "\n[ERROR] Script compilation failed! Return code: " + std::to_string(result);
                return false;
            }

            outLog += "\n[SUCCESS] Compiled successfully: " + currentLibPath;
            return true;
        }

        bool LoadLibrary(const std::string& projectPath)
        {
            // On fresh session load, currentLibPath is empty — scan for newest compiled .so
            if (currentLibPath.empty() || !fs::exists(currentLibPath))
            {
                std::string newest;
                fs::file_time_type newestTime;
                bool found = false;

                if (fs::exists(projectPath))
                {
                    for (const auto& entry : fs::directory_iterator(projectPath))
                    {
                        const auto& fname = entry.path().filename().string();
                        if (fname.find("libscripts_") == 0 && entry.path().extension() == ".so")
                        {
                            if (!found || entry.last_write_time() > newestTime)
                            {
                                found        = true;
                                newestTime   = entry.last_write_time();
                                newest       = entry.path().string();
                            }
                        }
                    }
                }

                if (!found) return false;
                currentLibPath = newest;
            }

            // Unload previous if exists
            UnloadLibrary();

            // Load new library (RTLD_NOW resolves all undefined symbols before dlopen returns)
            libraryHandle = dlopen(currentLibPath.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!libraryHandle)
            {
                std::cerr << "Failed to load scripts: " << dlerror() << std::endl;
                return false;
            }

            // Bind functions
            getNamesFunc = (GetScriptNamesFunc)dlsym(libraryHandle, "GetScriptNames");
            createFunc = (CreateScriptFunc)dlsym(libraryHandle, "CreateScript");

            if (!getNamesFunc || !createFunc)
            {
                std::cerr << "Failed to bind script functions: " << dlerror() << std::endl;
                UnloadLibrary();
                return false;
            }

            // Fetch available scripts
            const char** names = nullptr;
            int count = 0;
            getNamesFunc(&names, &count);

            availableScripts.clear();
            for (int i = 0; i < count; i++)
            {
                availableScripts.push_back(names[i]);
            }

            return true;
        }

        void UnloadLibrary()
        {
            if (libraryHandle)
            {
                dlclose(libraryHandle);
                libraryHandle = nullptr;
                getNamesFunc = nullptr;
                createFunc = nullptr;
                availableScripts.clear();
            }
        }

        const std::vector<std::string>& GetAvailableScripts() const
        {
            return availableScripts;
        }

        Component* InstantiateScript(const std::string& name)
        {
            if (createFunc)
            {
                return createFunc(name.c_str());
            }
            return nullptr;
        }
    };
}
