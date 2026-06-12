#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include "raylib.h"
#include "../include/nlohmann/json.hpp"

namespace Indium
{
    /**
     * @brief Packages a project + the IndiumPlayer runtime into a shippable folder
     * (File > Export Game in the editor). Header-only and window-free so the logic
     * is unit-testable.
     *
     * Output layout:
     *   <outParent>/<ProjectName>/
     *     <ProjectName>[.exe]   — the IndiumPlayer binary, renamed after the game
     *     game.json             — window title/size/fps + project dir ("data")
     *     data/                 — the project, minus dev-only entries (scripts/
     *                             sources, saves/, Export/, IDE configs); includes
     *                             the prebuilt libscripts_* library
     *
     * Two portability fixes happen during the copy:
     *   1. Absolute asset paths under the project root (the editor's file browser
     *      stores absolute paths) are rewritten to project-relative inside every
     *      copied .scene/.prefab/.json/.indp — AssetManager resolves relative paths
     *      against the project root at load.
     *   2. The copied script library's mtime is bumped to "now", after the player
     *      binary is copied: ScriptManager refuses libraries older than the host
     *      executable (ABI-staleness check), and copy timestamps would otherwise
     *      be toolchain-dependent.
     */
    class Exporter
    {
    public:
        struct Result
        {
            bool        ok = false;
            std::string error;
            std::string outputDir;
        };

        /**
         * @brief Rewrites every string value in `j` that points inside `projectRoot`
         * to a project-relative path. Non-path strings, relative paths and absolute
         * paths outside the project pass through untouched. Public for tests.
         */
        static nlohmann::json RelativizePaths(const nlohmann::json& j, const std::string& projectRoot)
        {
            namespace fs = std::filesystem;
            const std::string root = fs::path(projectRoot).lexically_normal().generic_string();

            if (j.is_string())
            {
                const std::string s = j.get<std::string>();
                // Normalize separators for the prefix test only; preserve the tail as-is
                // apart from separator normalization (generic '/' loads on every platform).
                const std::string gen = fs::path(s).lexically_normal().generic_string();
                if (gen.size() > root.size() + 1 && gen.compare(0, root.size(), root) == 0
                    && gen[root.size()] == '/')
                {
                    return nlohmann::json(gen.substr(root.size() + 1));
                }
                return j;
            }
            if (j.is_object())
            {
                nlohmann::json out = nlohmann::json::object();
                for (auto it = j.begin(); it != j.end(); ++it) out[it.key()] = RelativizePaths(it.value(), projectRoot);
                return out;
            }
            if (j.is_array())
            {
                nlohmann::json out = nlohmann::json::array();
                for (const auto& v : j) out.push_back(RelativizePaths(v, projectRoot));
                return out;
            }
            return j;
        }

        static Result ExportGame(const std::string& projectPath,
                                 const std::string& projectName,
                                 const std::string& playerBinaryPath,
                                 const std::string& outParent)
        {
            namespace fs = std::filesystem;
            Result res;

            std::error_code ec;
            if (!fs::exists(fs::path(projectPath) / "project.indp"))
            {
                res.error = "Not a project: " + projectPath;
                return res;
            }
            if (playerBinaryPath.empty() || !fs::exists(playerBinaryPath))
            {
                res.error = "IndiumPlayer runtime not found (build the IndiumPlayer target first): "
                          + playerBinaryPath;
                return res;
            }

            const std::string safeName = SanitizeName(projectName);
            const fs::path outDir = fs::path(outParent) / safeName;

            // Refuse to wipe a directory we didn't create: a previous export always
            // contains game.json, so its absence means this folder is something else.
            if (fs::exists(outDir))
            {
                if (!fs::exists(outDir / "game.json"))
                {
                    res.error = "Output folder exists and is not a previous export: " + outDir.string();
                    return res;
                }
                fs::remove_all(outDir, ec);
                if (ec) { res.error = "Could not clear previous export: " + ec.message(); return res; }
            }

            try
            {
                fs::create_directories(outDir);

                // 1) The player binary, renamed after the game.
#if defined(_WIN32)
                const fs::path exeOut = outDir / (safeName + ".exe");
#else
                const fs::path exeOut = outDir / safeName;
#endif
                fs::copy_file(playerBinaryPath, exeOut, fs::copy_options::overwrite_existing);
                // copy_file does not guarantee permissions/mtime; make it executable.
                fs::permissions(exeOut,
                                fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                                fs::perm_options::add, ec);

                // 2) The project data, minus dev-only entries.
                const fs::path dataDir = outDir / "data";
                CopyProjectTree(projectPath, dataDir, fs::path(outParent));

                // 3) Portability rewrite: absolute in-project paths → project-relative.
                RelativizeTree(dataDir, projectPath);

                // 4) game.json — the player's window/boot config.
                {
                    nlohmann::json g;
                    g["title"]     = projectName;
                    g["width"]     = 1280;
                    g["height"]    = 720;
                    g["targetFps"] = 60;
                    g["project"]   = "data";
                    g["showTitle"] = true;
                    std::ofstream o(outDir / "game.json");
                    o << std::setw(4) << g << std::endl;
                    if (!o.good()) { res.error = "Failed to write game.json"; return res; }
                }

                // 5) ABI-staleness guard: the script library must be newer than the
                // player binary or ScriptManager refuses to load it.
                const auto now = fs::file_time_type::clock::now();
                for (const auto& entry : fs::directory_iterator(dataDir))
                {
                    const std::string fname = entry.path().filename().string();
                    if (fname.rfind("libscripts_", 0) == 0)
                        fs::last_write_time(entry.path(), now, ec);
                }
            }
            catch (const std::exception& e)
            {
                res.error = e.what();
                return res;
            }

            res.ok        = true;
            res.outputDir = outDir.string();
            TraceLog(LOG_INFO, "EXPORT: Game exported to '%s'", res.outputDir.c_str());
            return res;
        }

        /** @brief Filesystem-safe game/executable name: keeps alnum, space, '-', '_',
         *  collapses everything else; falls back to "Game" for a degenerate name. */
        static std::string SanitizeName(const std::string& name)
        {
            std::string out;
            out.reserve(name.size());
            for (char c : name)
            {
                if (isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_') out += c;
            }
            // Trim leading/trailing spaces left by stripped characters.
            const auto first = out.find_first_not_of(' ');
            const auto last  = out.find_last_not_of(' ');
            out = (first == std::string::npos) ? "" : out.substr(first, last - first + 1);
            return out.empty() ? "Game" : out;
        }

    private:
        /** @brief Top-level project entries that never ship: script sources (the
         *  prebuilt library at the project root ships instead), player saves, prior
         *  exports, and IDE configs. */
        static bool SkipTopLevel(const std::string& name)
        {
            return name == "scripts" || name == "saves" || name == "Export"
                || name == ".vscode" || name == ".clangd" || name == "logs";
        }

        static bool SkipFile(const std::filesystem::path& p)
        {
            const std::string name = p.filename().string();
            return name == ".DS_Store" || p.extension() == ".tmp";
        }

        static void CopyProjectTree(const std::filesystem::path& srcRoot,
                                    const std::filesystem::path& dstRoot,
                                    const std::filesystem::path& exportParent)
        {
            namespace fs = std::filesystem;
            fs::create_directories(dstRoot);
            for (const auto& entry : fs::directory_iterator(srcRoot))
            {
                const std::string name = entry.path().filename().string();
                if (SkipTopLevel(name)) continue;
                // Never recurse into the export destination itself (covers a custom
                // outParent placed inside the project but not named "Export").
                std::error_code eqEc;
                if (fs::equivalent(entry.path(), exportParent, eqEc)) continue;

                if (entry.is_directory())
                    CopyDirRecursive(entry.path(), dstRoot / name);
                else if (!SkipFile(entry.path()))
                    fs::copy_file(entry.path(), dstRoot / name, fs::copy_options::overwrite_existing);
            }
        }

        static void CopyDirRecursive(const std::filesystem::path& src, const std::filesystem::path& dst)
        {
            namespace fs = std::filesystem;
            fs::create_directories(dst);
            for (const auto& entry : fs::directory_iterator(src))
            {
                const std::string name = entry.path().filename().string();
                if (entry.is_directory())      CopyDirRecursive(entry.path(), dst / name);
                else if (!SkipFile(entry.path()))
                    fs::copy_file(entry.path(), dst / name, fs::copy_options::overwrite_existing);
            }
        }

        /** @brief Rewrites absolute in-project paths to relative inside every JSON-ish
         *  document of the exported data tree. Files that fail to parse are left
         *  untouched (never let one malformed file abort the export). */
        static void RelativizeTree(const std::filesystem::path& dataDir, const std::string& projectRoot)
        {
            namespace fs = std::filesystem;
            for (auto it = fs::recursive_directory_iterator(dataDir);
                 it != fs::recursive_directory_iterator(); ++it)
            {
                if (!it->is_regular_file()) continue;
                const std::string ext = it->path().extension().string();
                if (ext != ".scene" && ext != ".prefab" && ext != ".json" && ext != ".indp") continue;

                try
                {
                    nlohmann::json j;
                    {
                        std::ifstream in(it->path());
                        in >> j;
                    }
                    nlohmann::json rewritten = RelativizePaths(j, projectRoot);
                    if (rewritten == j) continue;
                    std::ofstream out(it->path(), std::ios::trunc);
                    out << std::setw(4) << rewritten << std::endl;
                }
                catch (...)
                {
                    TraceLog(LOG_WARNING, "EXPORT: skipped path rewrite for unparseable '%s'",
                             it->path().string().c_str());
                }
            }
        }
    };
}
