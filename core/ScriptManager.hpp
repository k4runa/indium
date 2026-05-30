#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include "NativeScript.hpp"

#if !defined(_WIN32)
    #include <dlfcn.h>
    #include <unistd.h>
    #if defined(__APPLE__)
        #include <mach-o/dyld.h>
    #endif
#endif

namespace fs = std::filesystem;

namespace Indium {

    class ScriptManager {
    private:
        void* libraryHandle = nullptr;
        std::string currentLibPath;

#if defined(_WIN32)
        static constexpr const char* kLibExtension = ".dll";
#elif defined(__APPLE__)
        static constexpr const char* kLibExtension = ".dylib";
#else
        static constexpr const char* kLibExtension = ".so";
#endif

        static std::string GetExecutablePath()
        {
#if defined(_WIN32)
            // _get_pgmptr is MSVC CRT, available from <stdlib.h>, no windows.h needed
            char* path = nullptr;
            if (_get_pgmptr(&path) == 0 && path) return path;
            return {};
#elif defined(__APPLE__)
            uint32_t size = 0;
            _NSGetExecutablePath(nullptr, &size);
            std::vector<char> buf(size + 1, '\0');
            if (_NSGetExecutablePath(buf.data(), &size) != 0) return {};
            char resolved[PATH_MAX];
            if (realpath(buf.data(), resolved)) return resolved;
            return buf.data();
#else
            char buf[PATH_MAX];
            ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
            if (len > 0) { buf[len] = '\0'; return buf; }
            return {};
#endif
        }

        static std::string GetEngineRoot()
        {
            std::string exe = GetExecutablePath();
            if (!exe.empty()) return fs::path(exe).parent_path().parent_path().string();
            return fs::current_path().string();
        }

        static fs::file_time_type EngineAbiWriteTime()
        {
            static const fs::file_time_type cached = []
            {
                fs::file_time_type newest = fs::file_time_type::min();
                std::error_code ec;
                const std::string root = GetEngineRoot();
                for (const char* sub : { "/core", "/2D" })
                {
                    fs::path dir = root + sub;
                    if (!fs::exists(dir, ec)) continue;
                    for (auto it = fs::recursive_directory_iterator(dir, ec);
                         it != fs::recursive_directory_iterator(); it.increment(ec))
                    {
                        const auto ext = it->path().extension();
                        if (ext == ".hpp" || ext == ".h")
                        {
                            auto t = it->last_write_time(ec);
                            if (!ec && t > newest) newest = t;
                        }
                    }
                }
                return newest;
            }();
            return cached;
        }

        static std::string GetCompiler()
        {
#if defined(INDIUM_CXX_COMPILER)
            return INDIUM_CXX_COMPILER;
#elif defined(_WIN32)
            return "cl.exe";
#elif defined(__APPLE__)
            return "clang++";
#else
            return "g++";
#endif
        }

        static std::string GetIntelliSenseMode()
        {
#if defined(_WIN32)
            return "windows-msvc-x64";
#elif defined(__APPLE__)
    #if defined(__aarch64__) || defined(__arm64__)
            return "macos-clang-arm64";
    #else
            return "macos-clang-x64";
    #endif
#else
            return "linux-gcc-x64";
#endif
        }

        static std::string GetRaylibIncludeDir()
        {
            std::vector<std::string> candidates;
#if defined(INDIUM_RAYLIB_INCLUDE_DIR)
            candidates.emplace_back(INDIUM_RAYLIB_INCLUDE_DIR);
#endif
            candidates.emplace_back("/opt/homebrew/include");
            candidates.emplace_back("/usr/local/include");
            candidates.emplace_back("/usr/include");

            for (const auto& dir : candidates)
            {
                std::error_code ec;
                if (!dir.empty() && fs::exists(fs::path(dir) / "raylib.h", ec)) return dir;
            }
            return {};
        }

        std::vector<std::string> availableScripts;

        typedef void (*GetScriptNamesFunc)(const char***, int*);
        typedef Component* (*CreateScriptFunc)(const char*);

        GetScriptNamesFunc getNamesFunc = nullptr;
        CreateScriptFunc createFunc = nullptr;

    public:
        std::string activeProjectPath = "";

        std::string GetActiveProjectPath() const { return activeProjectPath; }
        void SetActiveProjectPath(const std::string& path) { activeProjectPath = path; }

        ScriptManager()
        {
            NativeScript::InstantiateCallback = [this](const std::string& name)
            {
                return this->InstantiateScript(name);
            };
        }
        ~ScriptManager() { UnloadLibrary(); }

        static ScriptManager& Get()
        {
            static ScriptManager instance;
            return instance;
        }

        bool ScriptsAreStale(const std::string& projectPath)
        {
            std::error_code ec;
            fs::path scriptsDir = fs::path(projectPath) / "scripts";
            if (!fs::exists(scriptsDir, ec)) return false;

            bool hasSources = false;
            fs::file_time_type newestSource = fs::file_time_type::min();
            for (const auto& e : fs::directory_iterator(scriptsDir, ec))
            {
                if (e.path().extension() == ".cpp")
                {
                    hasSources = true;
                    auto t = e.last_write_time(ec);
                    if (t > newestSource) newestSource = t;
                }
            }
            if (!hasSources) return false;

            bool haveDylib = false;
            fs::file_time_type libTime = fs::file_time_type::min();
            for (const auto& e : fs::directory_iterator(projectPath, ec))
            {
                const std::string fname = e.path().filename().string();
                if (fname.rfind("libscripts_", 0) == 0 && e.path().extension() == kLibExtension)
                {
                    haveDylib = true;
                    auto t = e.last_write_time(ec);
                    if (t > libTime) libTime = t;
                }
            }

            if (!haveDylib) return true;
            if (libTime < EngineAbiWriteTime()) return true;
            if (libTime < newestSource) return true;
            return false;
        }

        bool CompileScripts(const std::string& projectPath, std::string& outLog)
        {
#if defined(_WIN32)
            outLog = "Script compilation is not supported on Windows in this release.";
            return false;
#else
            std::string scriptsDir = projectPath + "/scripts";
            if (!fs::exists(scriptsDir))
            {
                fs::create_directory(scriptsDir);
                std::string exportFile = scriptsDir + "/IndiumExports.cpp";
                if (!fs::exists(exportFile))
                {
                    std::ofstream f(exportFile);
                    if (!f.is_open())
                    {
                        outLog = "Failed to create export file: " + exportFile;
                        return false;
                    }
                    f << "#include \"IndiumEngine.hpp\"\nINDIUM_EXPORT_SCRIPTS()\n";
                }
            }

            for (const auto& entry : fs::directory_iterator(projectPath))
            {
                if (entry.path().filename().string().find("libscripts_") == 0 && entry.path().extension() == kLibExtension) { fs::remove(entry.path()); }
            }

            std::string timeStamp = std::to_string(std::time(nullptr));
            currentLibPath = projectPath + "/libscripts_" + timeStamp + kLibExtension;

            auto shellQuote = [](const std::string& s) -> std::string
            {
                std::string out;
                out.reserve(s.size() + 2);
                out += '\'';
                for (char c : s) { if (c == '\'') out += "'\\''"; else out += c; }
                out += '\'';
                return out;
            };

            std::string cppFiles;
            for (const auto& entry : fs::directory_iterator(scriptsDir)) { if (entry.path().extension() == ".cpp") { cppFiles += shellQuote(entry.path().string()) + " "; } }

            if (cppFiles.empty()) { outLog = "No scripts to compile."; return false; }

            std::string engineRoot = GetEngineRoot();

#if defined(__APPLE__)
            const std::string platformFlags = "-dynamiclib -undefined dynamic_lookup";
#else
            const std::string platformFlags = "-shared -fPIC";
#endif

            std::string raylibInclude;
            if (std::string raylibDir = GetRaylibIncludeDir(); !raylibDir.empty()) raylibInclude = " -I" + shellQuote(raylibDir);

            std::string cmd = shellQuote(GetCompiler()) + " " + platformFlags + " -std=c++20 " + cppFiles +
                              " -I" + shellQuote(engineRoot + "/core")    +
                              " -I" + shellQuote(engineRoot + "/2D")      +
                              " -I" + shellQuote(engineRoot + "/include") +
                              raylibInclude +
                              " -o " + shellQuote(currentLibPath) + " 2>&1";

            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) { outLog = "Failed to start compilation process (popen failed)."; return false; }
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) { outLog += buffer; }

            int result = pclose(pipe);
            if (result != 0)
            {
                outLog += "\n[ERROR] Script compilation failed! Return code: " + std::to_string(result);
                return false;
            }

            outLog += "\n[SUCCESS] Compiled successfully: " + currentLibPath;
            return true;
#endif
        }

        bool LoadLibrary(const std::string& projectPath)
        {
            if (currentLibPath.empty() || !fs::exists(currentLibPath))
            {
                std::string bestPath;
                fs::file_time_type bestTime;
                bool found = false;

                const fs::file_time_type engineTime = EngineAbiWriteTime();
                bool skippedStale = false;

                if (fs::exists(projectPath))
                {
                    for (const auto& entry : fs::directory_iterator(projectPath))
                    {
                        const std::string fname = entry.path().filename().string();
                        if (fname.find("libscripts_") == 0 && entry.path().extension() == kLibExtension)
                        {
                            auto modTime = entry.last_write_time();
                            if (modTime < engineTime) { skippedStale = true; continue; }
                            if (!found || modTime > bestTime)
                            {
                                bestPath = entry.path().string();
                                bestTime = modTime;
                                found = true;
                            }
                        }
                    }
                }

                if (!found)
                {
                    if (skippedStale)
                        TraceLog(LOG_WARNING, "SCRIPTS: Ignoring script library in '%s' — it predates the current engine headers (ABI mismatch). Recompile with Scripts > Compile & Reload.", projectPath.c_str());
                    else
                        TraceLog(LOG_WARNING, "SCRIPTS: No compiled library found in '%s'", projectPath.c_str());
                    return false;
                }

                currentLibPath = bestPath;
            }

            UnloadLibrary();

#if defined(_WIN32)
            // Script loading not supported in Windows builds
            return false;
#else
            libraryHandle = dlopen(currentLibPath.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!libraryHandle)
            {
                std::cerr << "Failed to load scripts: " << dlerror() << std::endl;
                return false;
            }
            getNamesFunc = (GetScriptNamesFunc)dlsym(libraryHandle, "GetScriptNames");
            createFunc   = (CreateScriptFunc)  dlsym(libraryHandle, "CreateScript");
            if (!getNamesFunc || !createFunc)
            {
                std::cerr << "Failed to bind script functions: " << dlerror() << std::endl;
                UnloadLibrary();
                return false;
            }
#endif

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
#if !defined(_WIN32)
                dlclose(libraryHandle);
#endif
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

        bool GenerateClangdConfig(const std::string& projectPath)
        {
            if (projectPath.empty()) return false;

            std::string engineRoot = GetEngineRoot();
            std::string raylibDir  = GetRaylibIncludeDir();
            bool ok = true;

            try
            {
                std::ofstream f(fs::path(projectPath) / ".clangd");
                f << "CompileFlags:\n"
                  << "  Add:\n"
                  << "    - \"-I" << engineRoot << "/core\"\n"
                  << "    - \"-I" << engineRoot << "/2D\"\n"
                  << "    - \"-I" << engineRoot << "/include\"\n";
                if (!raylibDir.empty())
                  f << "    - \"-I" << raylibDir << "\"\n";
                f << "    - \"-std=c++20\"\n";
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "SCRIPTS: Failed to write .clangd: %s", e.what());
                ok = false;
            }

            try
            {
                fs::path vscodeDir = fs::path(projectPath) / ".vscode";
                fs::create_directories(vscodeDir);
                std::ofstream f(vscodeDir / "c_cpp_properties.json");
                f << "{\n"
                  << "    \"configurations\": [\n"
                  << "        {\n"
                  << "            \"name\": \"Indium\",\n"
                  << "            \"includePath\": [\n"
                  << "                \"" << engineRoot << "/core\",\n"
                  << "                \"" << engineRoot << "/2D\",\n"
                  << "                \"" << engineRoot << "/include\"";
                if (!raylibDir.empty())
                  f << ",\n                \"" << raylibDir << "\"";
                f << "\n"
                  << "            ],\n"
                  << "            \"defines\": [],\n"
                  << "            \"compilerPath\": \"" << GetCompiler() << "\",\n"
                  << "            \"cppStandard\": \"c++20\",\n"
                  << "            \"intelliSenseMode\": \"" << GetIntelliSenseMode() << "\"\n"
                  << "        }\n"
                  << "    ],\n"
                  << "    \"version\": 4\n"
                  << "}\n";
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_ERROR, "SCRIPTS: Failed to write c_cpp_properties.json: %s", e.what());
                ok = false;
            }

            if (ok)
                TraceLog(LOG_INFO, "SCRIPTS: IDE configs generated for '%s'", projectPath.c_str());
            return ok;
        }
    };
}
