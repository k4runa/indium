#pragma once
#include "raylib.h"
#include "raymath.h"   // Vector3Normalize for the lit-shader light direction
#include "rlgl.h"      // rlGetShaderIdDefault — distinguishes a real shader from the fallback
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
         * @brief Gets a 3D model by path. If not loaded, loads it. Cached by the
         * authored path and shared — do not call UnloadModel on the returned handle.
         *
         * Used by MeshRendererComponent to draw 3D props into the 2.5D scene. Loads
         * glTF/glb/OBJ via raylib's built-in loaders. Missing paths are throttled the
         * same way GetTexture throttles missing textures, so per-frame pollers don't
         * re-hit the disk or spam the log.
         */
        Model GetModel(const std::string& path)
        {
            if (path.empty()) return { 0 };

            auto it = models_.find(path);
            if (it != models_.end()) return it->second;

            // Built-in primitives — "@cube" / "@sphere" are generated procedurally rather
            // than loaded from disk, so MeshRenderer works (and has a sensible default)
            // without shipping any model asset. Cached + unloaded like a normal model.
            if (path == "@cube" || path == "@sphere")
            {
                Mesh mesh = (path == "@sphere") ? GenMeshSphere(0.5f, 24, 24)
                                                : GenMeshCube(1.0f, 1.0f, 1.0f);
                Model model = LoadModelFromMesh(mesh);   // takes ownership of mesh
                models_[path] = model;
                return model;
            }

            auto         fit = failedModelAt_.find(path);
            const double now = GetTime();
            if (fit != failedModelAt_.end() && (now - fit->second) < kRetryMissSeconds) return { 0 };

            Model model = LoadModel(ResolvePath(path).c_str());
            if (model.meshCount > 0)
            {
                models_[path] = model;
                failedModelAt_.erase(path);
                return model;
            }
            // LoadModel returns a 0-mesh model on failure; release the empty shell so we
            // don't leak the default material it allocates.
            UnloadModel(model);
            if (fit == failedModelAt_.end())
                TraceLog(LOG_WARNING, "ASSET: failed to load model '%s' (will retry)", path.c_str());
            failedModelAt_[path] = now;
            return { 0 };
        }

        /**
         * @brief Returns a shared directional-diffuse shader for shading 3D meshes, or
         * nullptr if it could not be compiled (caller then renders unlit/flat).
         *
         * Lazily compiled once and shared by every MeshRendererComponent. Without it a
         * generated sphere shades flat and reads as a 2D circle; with one fixed key light
         * primitives look properly 3D. Requires a live GL context (only ever called from
         * the off-screen mesh render, never from headless tests).
         */
        const Shader* GetLitShader()
        {
            if (!litShaderTried_)
            {
                litShaderTried_ = true;
                litShader_ = LoadShaderFromMemory(kLitVS_, kLitFS_);
                // LoadShaderFromMemory falls back to the default shader id on compile
                // failure; only treat a genuinely new program as valid.
                if (litShader_.id != 0 && litShader_.id != rlGetShaderIdDefault())
                {
                    int loc = GetShaderLocation(litShader_, "lightDir");
                    Vector3 dir = Vector3Normalize({ -0.4f, -0.7f, -0.6f });
                    SetShaderValue(litShader_, loc, &dir, SHADER_UNIFORM_VEC3);
                    litShaderValid_ = true;
                }
            }
            return litShaderValid_ ? &litShader_ : nullptr;
        }

        /**
         * @brief Unloads all currently loaded assets and clears the manager.
         */
        void Clear()
        {
            if (litShaderValid_ && IsWindowReady()) UnloadShader(litShader_);
            litShaderValid_ = false;
            litShaderTried_ = false;
            litShader_      = {};
            for (auto& pair : textures) UnloadTexture(pair.second);
            textures.clear();
            failedAt_.clear();
            for (auto& pair : sounds_)  UnloadSound(pair.second);
            sounds_.clear();
            for (auto& pair : fonts_)   UnloadFont(pair.second);
            fonts_.clear();
            for (auto& pair : models_)  UnloadModel(pair.second);
            models_.clear();
            failedModelAt_.clear();
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
        std::unordered_map<std::string, Model>     models_;
        std::unordered_map<std::string, double>    failedAt_;       // path -> GetTime() of last failed texture load (throttles retries)
        std::unordered_map<std::string, double>    failedModelAt_;  // path -> GetTime() of last failed model load (throttles retries)

        // Shared directional-diffuse shader for 3D meshes (see GetLitShader).
        Shader litShader_{};
        bool   litShaderTried_ = false;
        bool   litShaderValid_ = false;

        // GLSL 330 (desktop GL 3.3, all supported platforms). raylib auto-wires the
        // standard mvp/matNormal/colDiffuse/texture0 + vertex attribs by name; only
        // lightDir is set manually. Falls back to flat if compilation fails.
        static constexpr const char* kLitVS_ =
            "#version 330\n"
            "in vec3 vertexPosition;\n"
            "in vec3 vertexNormal;\n"
            "in vec2 vertexTexCoord;\n"
            "in vec4 vertexColor;\n"
            "uniform mat4 mvp;\n"
            "uniform mat4 matNormal;\n"
            "out vec3 fragNormal;\n"
            "out vec2 fragTexCoord;\n"
            "out vec4 fragColor;\n"
            "void main(){\n"
            "  fragTexCoord = vertexTexCoord;\n"
            "  fragColor = vertexColor;\n"
            "  fragNormal = normalize(vec3(matNormal*vec4(vertexNormal,0.0)));\n"
            "  gl_Position = mvp*vec4(vertexPosition,1.0);\n"
            "}\n";
        static constexpr const char* kLitFS_ =
            "#version 330\n"
            "in vec3 fragNormal;\n"
            "in vec2 fragTexCoord;\n"
            "in vec4 fragColor;\n"
            "uniform sampler2D texture0;\n"
            "uniform vec4 colDiffuse;\n"
            "uniform vec3 lightDir;\n"
            "out vec4 finalColor;\n"
            "void main(){\n"
            "  vec4 texel = texture(texture0, fragTexCoord);\n"
            "  float d = max(dot(normalize(fragNormal), -normalize(lightDir)), 0.0);\n"
            "  float shade = 0.25 + 0.75*d;\n"
            "  finalColor = texel*colDiffuse*fragColor*vec4(vec3(shade),1.0);\n"
            "}\n";

        // Re-attempt a missing texture at most this often, so a file added/fixed on disk
        // mid-session recovers without re-hitting the disk every frame.
        static constexpr double kRetryMissSeconds = 1.0;

        // Prevent copying
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;
    };
}
