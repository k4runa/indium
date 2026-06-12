#pragma once
//
// Logger — centralised, file-backed logging for Indium.
//
// Hooks raylib's TraceLog system via SetTraceLogCallback so that EVERY engine
// message (and anything the engine logs with TraceLog) is mirrored to a file
// under <engine-root>/logs/, in addition to the console. This means crashes and
// misbehaviour leave a durable trail that can be inspected after the fact,
// instead of scrolling away in an in-app console that can't be copied from.
//
// Usage (once, right after InitWindow):
//     Indium::Logger::Init();
// and at shutdown:
//     Indium::Logger::Shutdown();
//
// Anywhere in the engine, the existing TraceLog(LOG_INFO, ...) calls are picked
// up automatically. For explicit gameplay/editor events, use:
//     Indium::Logger::Event("SCRIPTS", "Compiled %s", name);
//
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <string>
#include <mutex>
#include <filesystem>

#include "raylib.h"

namespace Indium
{
    namespace fs = std::filesystem;

    class Logger
    {
    public:
        static void Init()
        {
            auto& s = State();
            if (s.file) return;   // already initialised

            // logs/ lives at the engine root (one level above the exe, which sits
            // in build-windows/ or build/). Fall back to the exe dir if that path
            // can't be created (e.g. a read-only install location).
            fs::path logDir = ResolveLogDir();
            std::error_code ec;
            fs::create_directories(logDir, ec);

            // One file per session: indium_YYYY-MM-DD_HH-MM-SS.log
            char stamp[32];
            std::time_t t = std::time(nullptr);
            std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", std::localtime(&t));
            fs::path logPath = logDir / ("indium_" + std::string(stamp) + ".log");

            s.file = std::fopen(logPath.string().c_str(), "w");
            s.path = logPath.string();

            // Route raylib's TraceLog through us. From here on every TraceLog call
            // (engine-wide) is written to both console and file.
            SetTraceLogCallback(&Logger::TraceCallback);

            Event("LOGGER", "Session log started: %s", s.path.c_str());
        }

        static void Shutdown()
        {
            auto& s = State();
            if (s.file)
            {
                Event("LOGGER", "Session log closed.");
                std::fclose(s.file);
                s.file = nullptr;
            }
        }

        // Explicit, well-formed event line: [HH:MM:SS] [CATEGORY] message
        // Use for notable gameplay/editor actions (scene load, compile, play, etc.).
        static void Event(const char* category, const char* fmt, ...)
        {
            char msg[1024];
            va_list args;
            va_start(args, fmt);
            std::vsnprintf(msg, sizeof(msg), fmt, args);
            va_end(args);

            char line[1152];
            std::snprintf(line, sizeof(line), "%s [%s] %s", TimeStamp().c_str(), category, msg);

            auto& s = State();
            std::lock_guard<std::mutex> lock(s.mutex);
            std::printf("%s\n", line);
            if (s.file) { std::fprintf(s.file, "%s\n", line); std::fflush(s.file); }
        }

        static const std::string& Path() { return State().path; }

        /** @brief Re-emits an already-formatted TraceLog message through the
         *  console/file writer. The editor replaces our TraceLog callback with its
         *  own (in-app console) via SetTraceLogCallback — that callback must chain
         *  here, otherwise the session file silently stops capturing TraceLog
         *  output the moment the editor initializes. Thread-safe. */
        static void Mirror(int logLevel, const char* msg)
        {
            char line[1152];
            std::snprintf(line, sizeof(line), "%s [%s] %s", TimeStamp().c_str(), LevelTag(logLevel), msg);

            auto& s = State();
            std::lock_guard<std::mutex> lock(s.mutex);
            std::printf("%s\n", line);
            if (s.file) { std::fprintf(s.file, "%s\n", line); std::fflush(s.file); }
        }

    private:
        struct LogState
        {
            std::FILE*  file = nullptr;
            std::string path;
            std::mutex  mutex;
        };

        static LogState& State()
        {
            static LogState s;
            return s;
        }

        static std::string TimeStamp()
        {
            char buf[16];
            std::time_t t = std::time(nullptr);
            std::strftime(buf, sizeof(buf), "[%H:%M:%S]", std::localtime(&t));
            return buf;
        }

        static const char* LevelTag(int logLevel)
        {
            switch (logLevel)
            {
                case LOG_TRACE:   return "TRACE";
                case LOG_DEBUG:   return "DEBUG";
                case LOG_WARNING: return "WARNING";
                case LOG_ERROR:   return "ERROR";
                case LOG_FATAL:   return "FATAL";
                default:          return "INFO";
            }
        }

        // raylib trace-log callback: prefix with a level tag + timestamp, then
        // emit to console and file. Signature must match TraceLogCallback.
        static void TraceCallback(int logLevel, const char* text, va_list args)
        {
            char msg[1024];
            std::vsnprintf(msg, sizeof(msg), text, args);
            Mirror(logLevel, msg);
        }

        static fs::path ResolveLogDir()
        {
            std::string appDir = GetApplicationDirectory();   // exe's directory (with trailing slash)
            if (!appDir.empty())
            {
                fs::path exeDir = fs::path(appDir);
                // Engine root is normally one level up (exe in build-windows/ or build/).
                // Use it if it looks like the source tree; otherwise log beside the exe.
                fs::path parent = exeDir.parent_path();
                std::error_code ec;
                if (fs::exists(parent / "core", ec)) return parent / "logs";
                return exeDir / "logs";
            }
            return fs::current_path() / "logs";
        }
    };
}
