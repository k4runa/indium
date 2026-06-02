#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include "NativeScript.hpp"
#include "Logger.hpp"

#if defined(_WIN32)
    // Forward-declare only what we need — avoids pulling in windows.h which
    // conflicts with raylib's Rectangle, CloseWindow, DrawText, etc.
    extern "C" {
        void* __stdcall LoadLibraryA(const char*);
        void* __stdcall GetProcAddress(void*, const char*);
        int   __stdcall FreeLibrary(void*);
        unsigned long __stdcall GetLastError();
    }
#else
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

        // Remove any trailing '/' or '\'. Critical on Windows: a path ending in
        // backslash, when wrapped in double quotes for the compiler command line,
        // escapes the closing quote (e.g. -L"dir\" eats the quote).
        static std::string StripTrailingSlash(std::string s)
        {
            while (!s.empty() && (s.back() == '/' || s.back() == '\\')) s.pop_back();
            return s;
        }

        static std::string GetEngineRoot()
        {
            // GetApplicationDirectory() (raylib) is the most reliable cross-platform
            // way to get the exe's directory. Engine root is one level up (exe lives
            // in build-windows/ or build/, sources live in the repo root).
            std::string appDir = GetApplicationDirectory();
            if (!appDir.empty())
            {
                fs::path p = fs::path(appDir).parent_path();
                if (fs::exists(p / "core")) return p.string();
                // Standalone exe (no build subdir): try appDir itself
                if (fs::exists(fs::path(appDir) / "core")) return appDir;
            }
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
            // Prefer a compiler bundled next to the engine (release builds ship a
            // portable MinGW under toolchain/) so end users don't need MSYS2/MinGW
            // installed to compile gameplay scripts. Falls through to the build-time
            // compiler / platform default when no bundled toolchain is present.
            std::string bundled = (fs::path(GetEngineRoot()) / "toolchain" / "bin" / "g++.exe").string();
            if (fs::exists(bundled)) return bundled;
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
            candidates.emplace_back("C:/msys64/ucrt64/include");
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
            std::string scriptsDir = projectPath + "/scripts";
            if (!fs::exists(scriptsDir))
            {
                fs::create_directory(scriptsDir);
                std::string exportFile = scriptsDir + "/IndiumExports.cpp";
                if (!fs::exists(exportFile))
                {
                    std::ofstream f(exportFile);
                    if (!f.is_open()) { outLog = "Failed to create export file: " + exportFile; return false; }
                    f << "#include \"IndiumEngine.hpp\"\nINDIUM_EXPORT_SCRIPTS()\n";
                }
            }

            // Best-effort delete of stale script DLLs. Use the non-throwing
            // fs::remove overload: the currently-loaded DLL is still mapped into
            // the process and Windows refuses to delete it ("access denied"). The
            // throwing overload turned that into an uncaught filesystem_error that
            // crashed the editor. A leftover locked DLL is harmless — the new build
            // gets a fresh timestamped name.
            //
            // Do NOT UnloadLibrary() here: the caller (Compile & Reload) still needs
            // the live script instances' vtables valid so it can serialize the scene
            // BEFORE the old image is released. Unloading early dangles those vtables
            // and segfaults in Entity::serialize.
            std::error_code rmEc;
            for (const auto& entry : fs::directory_iterator(projectPath, rmEc))
            {
                if (entry.path().filename().string().find("libscripts_") == 0 && entry.path().extension() == kLibExtension)
                    fs::remove(entry.path(), rmEc);   // ignore failure; never throw
            }

            std::string timeStamp = std::to_string(std::time(nullptr));
            currentLibPath = projectPath + "/libscripts_" + timeStamp + kLibExtension;

            auto shellQuote = [](const std::string& s) -> std::string
            {
#if defined(_WIN32)
                // cmd.exe: wrap in double quotes, escape internal double quotes
                std::string out = "\"";
                for (char c : s) { if (c == '"') out += "\\\""; else out += c; }
                out += '"';
                return out;
#else
                std::string out;
                out.reserve(s.size() + 2);
                out += '\'';
                for (char c : s) { if (c == '\'') out += "'\\''"; else out += c; }
                out += '\'';
                return out;
#endif
            };

            std::string cppFiles;
            for (const auto& entry : fs::directory_iterator(scriptsDir))
            {
                if (entry.path().extension() == ".cpp")
                    cppFiles += shellQuote(entry.path().string()) + " ";
            }
            if (cppFiles.empty()) { outLog = "No scripts to compile."; return false; }

            std::string engineRoot = GetEngineRoot();

#if defined(_WIN32)
            const std::string platformFlags = "-shared";
#elif defined(__APPLE__)
            const std::string platformFlags = "-dynamiclib -undefined dynamic_lookup";
#else
            const std::string platformFlags = "-shared -fPIC";
#endif

            std::string raylibInclude;
            if (std::string raylibDir = GetRaylibIncludeDir(); !raylibDir.empty())
                raylibInclude = " -I" + shellQuote(raylibDir);

            std::string cmd = shellQuote(GetCompiler()) + " " + platformFlags + " -std=c++20 " + cppFiles +
                              " -I" + shellQuote(engineRoot + "/core")    +
                              " -I" + shellQuote(engineRoot + "/2D")      +
                              " -I" + shellQuote(engineRoot + "/include") +
                              raylibInclude +
#if defined(_WIN32)
                              // Resolve symbols against import libraries shipped next to
                              // the exe: libIndium.dll.a (ImGui + engine, via the exe's
                              // --export-all-symbols) and libraylib.dll.a (raylib's own
                              // DLL). Both are copied beside Indium.exe by the build.
                              //
                              // GetApplicationDirectory() returns a trailing slash on
                              // Windows ("...build-windows\"). A trailing backslash inside
                              // double quotes escapes the closing quote, mangling -L and
                              // breaking every symbol lookup — strip it first.
                              " -L" + shellQuote(StripTrailingSlash(GetApplicationDirectory())) +
                              " -lIndium -lraylib" +
#endif
                              " -o " + shellQuote(currentLibPath) + " 2>&1";

            Logger::Event("SCRIPTS", "Compiling: %s", cmd.c_str());

#if defined(_WIN32)
            // cmd.exe requires the entire command to be wrapped in an outer quote
            // when the command itself starts with a quoted executable path.
            std::string pipeCmd = "cmd /c \"" + cmd + "\"";
            FILE* pipe = _popen(pipeCmd.c_str(), "r");
#else
            FILE* pipe = popen(cmd.c_str(), "r");
#endif
            if (!pipe) { outLog = "Failed to start compilation process."; return false; }
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) { outLog += buffer; }
#if defined(_WIN32)
            int result = _pclose(pipe);
#else
            int result = pclose(pipe);
#endif

            if (result != 0)
            {
                outLog += "\n[ERROR] Script compilation failed! Return code: " + std::to_string(result);
                Logger::Event("SCRIPTS", "Compilation FAILED (code %d):\n%s", result, outLog.c_str());
                return false;
            }

            outLog += "\n[SUCCESS] Compiled successfully: " + currentLibPath;
            Logger::Event("SCRIPTS", "Compiled OK: %s", currentLibPath.c_str());
            return true;
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
            libraryHandle = LoadLibraryA(currentLibPath.c_str());
            if (!libraryHandle)
            {
                TraceLog(LOG_ERROR, "SCRIPTS: Failed to load DLL '%s' (error %lu)", currentLibPath.c_str(), (unsigned long)GetLastError());
                return false;
            }
            getNamesFunc = (GetScriptNamesFunc)GetProcAddress(libraryHandle, "GetScriptNames");
            createFunc   = (CreateScriptFunc)  GetProcAddress(libraryHandle, "CreateScript");
            if (!getNamesFunc || !createFunc)
            {
                TraceLog(LOG_ERROR, "SCRIPTS: Failed to bind script functions (error %lu)", GetLastError());
                UnloadLibrary();
                return false;
            }
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

            Logger::Event("SCRIPTS", "Loaded %d script(s) from %s",
                          (int)availableScripts.size(), currentLibPath.c_str());
            return true;
        }

        void UnloadLibrary()
        {
            if (libraryHandle)
            {
#if defined(_WIN32)
                FreeLibrary(libraryHandle);
#else
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
