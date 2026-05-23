#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <dlfcn.h>
#include <unistd.h>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include "NativeScript.hpp"

#if defined(__APPLE__)
    #include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace Indium {

    class ScriptManager {
    private:
        void* libraryHandle = nullptr;
        std::string currentLibPath;

        // Shared-library extension differs per platform: .dylib on macOS, .so elsewhere.
#if defined(__APPLE__)
        static constexpr const char* kLibExtension = ".dylib";
#else
        static constexpr const char* kLibExtension = ".so";
#endif

        // Absolute path to the running engine executable.
        static std::string GetExecutablePath()
        {
#if defined(__APPLE__)
            uint32_t size = 0;
            _NSGetExecutablePath(nullptr, &size);   // first call reports the required size
            std::vector<char> buf(size + 1, '\0');
            if (_NSGetExecutablePath(buf.data(), &size) != 0)
                return {};
            char resolved[PATH_MAX];
            if (realpath(buf.data(), resolved))
                return resolved;
            return buf.data();
#else
            char buf[PATH_MAX];
            ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
            if (len > 0)
            {
                buf[len] = '\0';
                return buf;
            }
            return {};
#endif
        }

        // Engine repository root, derived from the executable at <root>/build/Indium.
        static std::string GetEngineRoot()
        {
            std::string exe = GetExecutablePath();
            if (!exe.empty())
                return fs::path(exe).parent_path().parent_path().string();
            return fs::current_path().string();
        }

        // Newest mtime among the engine headers scripts compile against (everything
        // under core/ and 2D/). A script library older than this was built before the
        // current headers, so its NativeScript/Component vtable layout may not match
        // the engine's — virtual calls (e.g. deserialize) would dispatch to the wrong
        // slot and crash, so it must be recompiled.
        //
        // Keyed off header mtimes rather than the engine *binary* mtime on purpose:
        // the binary changes on every relink, but the script ABI only changes when a
        // header changes. This means simply rebuilding the engine no longer forces a
        // script recompile on the next project open. Computed once per process.
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

        // The C++ compiler used to build the engine. Scripts must be compiled with
        // an ABI-compatible compiler, so we reuse exactly what CMake picked.
        static std::string GetCompiler()
        {
#if defined(INDIUM_CXX_COMPILER)
            return INDIUM_CXX_COMPILER;
#elif defined(__APPLE__)
            return "clang++";
#else
            return "g++";
#endif
        }

        // VS Code IntelliSense mode string for the current platform/architecture.
        static std::string GetIntelliSenseMode()
        {
#if defined(__APPLE__)
    #if defined(__aarch64__) || defined(__arm64__)
            return "macos-clang-arm64";
    #else
            return "macos-clang-x64";
    #endif
#else
            return "linux-gcc-x64";
#endif
        }

        // Directory containing raylib.h. Baked in by CMake; falls back to probing
        // common install prefixes (Homebrew lives outside the default search path).
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
                if (!dir.empty() && fs::exists(fs::path(dir) / "raylib.h", ec))
                    return dir;
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

        ScriptManager() {
            NativeScript::InstantiateCallback = [this](const std::string& name)
            {
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

        // True if the project's scripts need recompiling before they can be safely
        // loaded: there are .cpp sources but the compiled library is missing, was
        // built against an older engine (ABI mismatch -> crash), or is older than a
        // source file. Loading a stale library segfaults, so callers recompile first.
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

            if (!haveDylib) return true;                       // never compiled
            if (libTime < EngineAbiWriteTime()) return true; // built against older engine headers (ABI)
            if (libTime < newestSource) return true;         // sources changed since last compile
            return false;
        }

        bool CompileScripts(const std::string& projectPath, std::string& outLog)
        {
            std::string scriptsDir = projectPath + "/scripts";
            if (!fs::exists(scriptsDir))
            {
                fs::create_directory(scriptsDir);
                // Create a dummy export file if empty
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

            // Clean up old cached libraries to prevent clutter
            for (const auto& entry : fs::directory_iterator(projectPath))
            {
                if (entry.path().filename().string().find("libscripts_") == 0 && entry.path().extension() == kLibExtension)
                {
                    fs::remove(entry.path());
                }
            }

            std::string timeStamp = std::to_string(std::time(nullptr));
            currentLibPath = projectPath + "/libscripts_" + timeStamp + kLibExtension;

            // Escape double-quotes inside a path so the shell command stays valid.
            // A filename containing '"' is pathological on Linux but worth guarding.
            auto shellQuote = [](const std::string& s) -> std::string {
                std::string out;
                out.reserve(s.size() + 2);
                out += '"';
                for (char c : s) {
                    if (c == '"' || c == '\\') out += '\\';
                    out += c;
                }
                out += '"';
                return out;
            };

            // Collect all .cpp files in scriptsDir, quoting paths to handle spaces
            std::string cppFiles;
            for (const auto& entry : fs::directory_iterator(scriptsDir))
            {
                if (entry.path().extension() == ".cpp")
                {
                    cppFiles += shellQuote(entry.path().string()) + " ";
                }
            }

            if (cppFiles.empty())
            {
                outLog = "No scripts to compile.";
                return false;
            }

            std::string engineRoot = GetEngineRoot();

            // macOS builds a .dylib and defers engine/raylib symbol resolution to
            // load time; Linux builds a .so where undefined symbols are permitted
            // in shared objects by default.
#if defined(__APPLE__)
            const std::string platformFlags = "-dynamiclib -undefined dynamic_lookup";
#else
            const std::string platformFlags = "-shared -fPIC";
#endif

            // Homebrew installs raylib headers outside the default search path.
            std::string raylibInclude;
            if (std::string raylibDir = GetRaylibIncludeDir(); !raylibDir.empty())
                raylibInclude = " -I" + shellQuote(raylibDir);

            // Append 2>&1 to capture stderr
            std::string cmd = shellQuote(GetCompiler()) + " " + platformFlags + " -std=c++20 " + cppFiles +
                              " -I" + shellQuote(engineRoot + "/core")    +
                              " -I" + shellQuote(engineRoot + "/2D")      +
                              " -I" + shellQuote(engineRoot + "/include") +
                              raylibInclude +
                              " -o " + shellQuote(currentLibPath) + " 2>&1";

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
            // If we have no valid path from this session, scan the project directory
            // for the most recently compiled library (handles fresh session loads).
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
                            // Never load a library older than the engine headers: it was built
                            // against a previous ABI and its vtables no longer match, which
                            // crashes on the first virtual call. Recompile via Compile & Reload.
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

        bool GenerateClangdConfig(const std::string& projectPath)
        {
            if (projectPath.empty()) return false;

            std::string engineRoot = GetEngineRoot();
            std::string raylibDir  = GetRaylibIncludeDir();
            bool ok = true;

            // .clangd — for clangd language server users
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

            // .vscode/c_cpp_properties.json — for VS Code Microsoft C/C++ extension users
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
