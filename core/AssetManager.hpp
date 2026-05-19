#pragma once
#include "raylib.h"
#include <unordered_map>
#include <string>
#include <iostream>

namespace Indium {
    /**
     * @brief Centralized manager for handling game assets like textures and audio.
     *
     * Ensures that assets are loaded only once and shared across all entities
     * that need them, saving memory and improving performance.
     */
    class AssetManager {
    public:
        static AssetManager& Get() {
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
         * @brief Unloads all currently loaded assets and clears the manager.
         */
        void Clear()
        {
            for (auto& pair : textures)
            {
                UnloadTexture(pair.second);
            }
            textures.clear();
        }

    private:
        AssetManager() = default;
        ~AssetManager()
        {
            Clear();
        }

        std::unordered_map<std::string, Texture2D> textures;

        // Prevent copying
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;
    };
}
