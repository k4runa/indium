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

        static std::string GetEngineRoot() {
            char buf[PATH_MAX];
            ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
            if (len > 0) {
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
        ~ScriptManager() {
            UnloadLibrary();
        }

        static ScriptManager& Get() {
            static ScriptManager instance;
            return instance;
        }

        bool CompileScripts(const std::string& projectPath) {
            std::string scriptsDir = projectPath + "/scripts";
            if (!fs::exists(scriptsDir)) {
                fs::create_directory(scriptsDir);
                // Create a dummy export file if empty
                std::string exportFile = scriptsDir + "/IndiumExports.cpp";
                if (!fs::exists(exportFile)) {
                    FILE* f = fopen(exportFile.c_str(), "w");
                    fprintf(f, "#include \"NativeScript.hpp\"\nINDIUM_EXPORT_SCRIPTS()\n");
                    fclose(f);
                }
            }

            std::string outFile = projectPath + "/libscripts.so";

            // Collect all .cpp files in scriptsDir, quoting paths to handle spaces
            std::string cppFiles;
            for (const auto& entry : fs::directory_iterator(scriptsDir)) {
                if (entry.path().extension() == ".cpp") {
                    cppFiles += "\"" + entry.path().string() + "\" ";
                }
            }

            if (cppFiles.empty()) {
                std::cout << "No scripts to compile." << std::endl;
                return false;
            }

            std::string engineRoot = GetEngineRoot();
            std::string cmd = "g++ -shared -fPIC -std=c++17 " + cppFiles +
                              " -I\"" + engineRoot + "/core\""    +
                              " -I\"" + engineRoot + "/2D\""      +
                              " -I\"" + engineRoot + "/include\"" +
                              " -o \"" + outFile + "\"";

            std::cout << "Compiling Scripts: " << cmd << std::endl;

            int result = system(cmd.c_str());
            if (result != 0) {
                std::cerr << "Script compilation failed! Make sure g++ is installed." << std::endl;
                return false;
            }

            return true;
        }

        bool LoadLibrary(const std::string& projectPath) {
            std::string libPath = projectPath + "/libscripts.so";

            if (!fs::exists(libPath)) return false;

            // Unload previous if exists
            UnloadLibrary();

            // Load new library (RTLD_NOW resolves all undefined symbols before dlopen returns)
            libraryHandle = dlopen(libPath.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!libraryHandle) {
                std::cerr << "Failed to load scripts: " << dlerror() << std::endl;
                return false;
            }

            // Bind functions
            getNamesFunc = (GetScriptNamesFunc)dlsym(libraryHandle, "GetScriptNames");
            createFunc = (CreateScriptFunc)dlsym(libraryHandle, "CreateScript");

            if (!getNamesFunc || !createFunc) {
                std::cerr << "Failed to bind script functions: " << dlerror() << std::endl;
                UnloadLibrary();
                return false;
            }

            // Fetch available scripts
            const char** names = nullptr;
            int count = 0;
            getNamesFunc(&names, &count);

            availableScripts.clear();
            for (int i = 0; i < count; i++) {
                availableScripts.push_back(names[i]);
                std::cout << "Loaded Script: " << names[i] << std::endl;
            }

            return true;
        }

        void UnloadLibrary() {
            if (libraryHandle) {
                dlclose(libraryHandle);
                libraryHandle = nullptr;
                getNamesFunc = nullptr;
                createFunc = nullptr;
                availableScripts.clear();
            }
        }

        const std::vector<std::string>& GetAvailableScripts() const {
            return availableScripts;
        }

        Component* InstantiateScript(const std::string& name) {
            if (createFunc) {
                return createFunc(name.c_str());
            }
            return nullptr;
        }
    };
}
