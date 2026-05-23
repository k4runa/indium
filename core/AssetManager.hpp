#pragma once
#include "raylib.h"
#include <unordered_map>
#include <string>
#include <iostream>
#include <cstring>

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
         * @brief Gets a texture by path. If not loaded, it loads it.
         *
         * @param path The file path to the texture.
         * @return Texture2D The loaded texture, or an empty texture if failed.
         */
        Texture2D GetTexture(const std::string& path)
        {
            if (path.empty()) return { 0 };

            if (textures.find(path) != textures.end())
            {
                return textures[path];
            }

            // Try loading as image first for better error handling/processing if needed
            Image img = LoadImage(path.c_str());
            if (img.data != nullptr)
            {
                Texture2D tex = LoadTextureFromImage(img);
                UnloadImage(img);

                if (tex.id > 0)
                {
                    textures[path] = tex;
                    return tex;
                }
            }

            std::cout << "ERROR: AssetManager failed to load texture at " << path << std::endl;
            return { 0 }; // Return empty texture if failed
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
            Sound s = ::LoadSound(path.c_str());
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
            Font f = ::LoadFontEx(path.c_str(), size, nullptr, 0);
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

        std::unordered_map<std::string, Texture2D> textures;
        std::unordered_map<std::string, Sound>     sounds_;
        std::unordered_map<std::string, Font>      fonts_;

        // Prevent copying
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;
    };
}
