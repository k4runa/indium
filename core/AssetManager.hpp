#pragma once
#include "raylib.h"
#include <unordered_map>
#include <string>
#include <iostream>
#include <cstring>
#include <filesystem>

namespace Indium {
    /**
     * @brief Centralized manager for handling game assets like textures and audio.
     *
     * Ensures that assets are loaded only once and shared across all entities
     * that need them, saving memory and improving performance.
     */
    class AssetManager {
    public:
        static AssetManager& Get()
        {
            static AssetManager instance;
            return instance;
        }

        /**
         * @brief Root that project-relative asset paths resolve against.
         *
         * Set by ProjectManager on project open/close. Historically the editor's file
         * browser stored absolute paths in scenes, which only resolve on the authoring
         * machine; the game exporter rewrites them to project-relative on export, and
         * this is what makes those relative paths load — in the editor, the standalone
         * player, and any future host — without depending on the process working
         * directory.
         */
        void SetProjectRoot(const std::string& root) { projectRoot_ = root; }

        /**
         * @brief Resolves a (possibly project-relative) asset path to the path used
         * for disk loads. Absolute paths and paths with no project root pass through
         * untouched; relative ones are joined onto the project root.
         */
        std::string ResolvePath(const std::string& path) const
        {
            if (path.empty() || projectRoot_.empty()) return path;
            std::filesystem::path p(path);
            if (p.is_absolute()) return path;
            return (std::filesystem::path(projectRoot_) / p).string();
        }

        /**
         * @brief Gets a texture by path. If not loaded, it loads it.
         *
         * @param path The file path to the texture.
         * @return Texture2D The loaded texture, or an empty texture if failed.
         */
        Texture2D GetTexture(const std::string& path)
        {
            if (path.empty()) return { 0 };

            auto it = textures.find(path);
            if (it != textures.end()) return it->second;   // loaded once, shared

            // A prior load failed. Pollers like the dialogue portrait call GetTexture every
            // frame, so we must NOT re-hit the disk and log on every one — but we also can't
            // remember the miss forever: in the editor a file is often added or its path fixed
            // on disk mid-session, and it must then appear on its own (it used to). So throttle:
            // retry a missing path at most once per kRetryMissSeconds, silently, recovering by itself.
            auto         fit = failedAt_.find(path);
            const double now = GetTime();
            if (fit != failedAt_.end() && (now - fit->second) < kRetryMissSeconds) return { 0 };

            // Try loading as image first for better error handling/processing if needed
            // (cache stays keyed on the path as authored; only the disk load resolves).
            Image img = LoadImage(ResolvePath(path).c_str());
            if (img.data != nullptr)
            {
                Texture2D tex = LoadTextureFromImage(img);
                UnloadImage(img);

                if (tex.id > 0)
                {
                    textures[path] = tex;
                    failedAt_.erase(path);   // recovered — it's a real texture now
                    return tex;
                }
            }

            // Record the miss so the next kRetryMissSeconds of polls are free; log only on the
            // first failure for a path, not on each silent retry.
            if (fit == failedAt_.end())
                TraceLog(LOG_WARNING, "ASSET: failed to load texture '%s' (will retry)", path.c_str());
            failedAt_[path] = now;
            return { 0 };
        }

        /**
         * @brief Gets a Sound by path. If not loaded, loads it.
         * NOTE: The returned Sound handle is shared — do not call UnloadSound on it.
         * Per-component volume/pitch must be applied via copies or aliases if needed.
         */
        Sound GetSound(const std::string& path)
        {
            if (path.empty()) return {};
            auto it = sounds_.find(path);
            if (it != sounds_.end()) return it->second;
            Sound s = ::LoadSound(ResolvePath(path).c_str());
            if (s.stream.buffer != nullptr)
            {
                sounds_[path] = s;
                return s;
            }
            std::cout << "ERROR: AssetManager failed to load sound at " << path << std::endl;
            return {};
        }

        /**
         * @brief Gets a Font by path and rasterisation size. Cached by "path:size".
         * NOTE: The returned Font handle is shared — do not call UnloadFont on it.
         */
        Font GetFont(const std::string& path, int size)
        {
            if (path.empty()) return ::GetFontDefault();
            std::string key = path + ":" + std::to_string(size);
            auto it = fonts_.find(key);
            if (it != fonts_.end()) return it->second;
            Font f = ::LoadFontEx(ResolvePath(path).c_str(), size, nullptr, 0);
            if (f.texture.id > 0)
            {
                fonts_[key] = f;
                return f;
            }
            std::cout << "ERROR: AssetManager failed to load font at " << path << std::endl;
            return ::GetFontDefault();
        }

        /**
         * @brief Unloads all currently loaded assets and clears the manager.
         */
        void Clear()
        {
            for (auto& pair : textures) UnloadTexture(pair.second);
            textures.clear();
            failedAt_.clear();
            for (auto& pair : sounds_)  UnloadSound(pair.second);
            sounds_.clear();
            for (auto& pair : fonts_)   UnloadFont(pair.second);
            fonts_.clear();
        }

    private:
        AssetManager() = default;
        ~AssetManager()
        {
            Clear();
        }

        std::string projectRoot_;   // resolves project-relative asset paths (see SetProjectRoot)

        std::unordered_map<std::string, Texture2D> textures;
        std::unordered_map<std::string, Sound>     sounds_;
        std::unordered_map<std::string, Font>      fonts_;
        std::unordered_map<std::string, double>    failedAt_;  // path -> GetTime() of last failed texture load (throttles retries)

        // Re-attempt a missing texture at most this often, so a file added/fixed on disk
        // mid-session recovers without re-hitting the disk every frame.
        static constexpr double kRetryMissSeconds = 1.0;

        // Prevent copying
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;
    };
}
