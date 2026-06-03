#pragma once
#include "raylib.h"
#include "../2D/component/PostProcessComponent.hpp"
#include <vector>
#include <array>

namespace Indium
{
    // --------------------------------------------------------------------
    // PostProcessManager
    //
    // Owns the GPU side of the screen-effect system: one shader per
    // PostEffect plus two ping-pong scratch render targets. Apply() chains
    // a list of effects over the viewport texture and writes the final
    // result back into the viewport, so the rest of the editor is unaware.
    //
    // All RT→RT blits flip the source vertically (negative source height) to
    // preserve orientation — render textures are stored bottom-up, matching
    // the convention used by the lighting compositor.
    //
    // Shaders are GLSL 330 (desktop). Embedded as strings so there are no
    // external asset files to ship.
    // --------------------------------------------------------------------
    class PostProcessManager
    {
    public:
        void Init()
        {
            const char* VS = nullptr; // use raylib's default vertex shader
            shaders_[(int)PostEffect::Grayscale]           = LoadShaderFromMemory(VS, FS_GRAYSCALE);
            shaders_[(int)PostEffect::Invert]              = LoadShaderFromMemory(VS, FS_INVERT);
            shaders_[(int)PostEffect::Vignette]            = LoadShaderFromMemory(VS, FS_VIGNETTE);
            shaders_[(int)PostEffect::ChromaticAberration] = LoadShaderFromMemory(VS, FS_ABERRATION);
            shaders_[(int)PostEffect::Scanlines]           = LoadShaderFromMemory(VS, FS_SCANLINES);
            shaders_[(int)PostEffect::Pixelate]            = LoadShaderFromMemory(VS, FS_PIXELATE);
            shaders_[(int)PostEffect::ColorGrade]          = LoadShaderFromMemory(VS, FS_COLORGRADE);
            loaded_ = true;

            a_ = LoadRenderTexture(1, 1);
            b_ = LoadRenderTexture(1, 1);
        }

        void Shutdown()
        {
            if (!loaded_) return;
            for (auto& s : shaders_) UnloadShader(s);
            UnloadRenderTexture(a_);
            UnloadRenderTexture(b_);
            loaded_ = false;
        }

        void Resize(int w, int h)
        {
            if (!loaded_ || w <= 0 || h <= 0) return;
            if (a_.texture.width == w && a_.texture.height == h) return;
            UnloadRenderTexture(a_);
            UnloadRenderTexture(b_);
            a_ = LoadRenderTexture(w, h);
            b_ = LoadRenderTexture(w, h);
        }

        // Apply the chain of effects (in order) onto `viewport`, writing the
        // final composited image back into `viewport`.
        void Apply(RenderTexture2D& viewport, const std::vector<PostProcessComponent*>& effects)
        {
            if (!loaded_ || effects.empty()) return;

            const float w = (float)viewport.texture.width;
            const float h = (float)viewport.texture.height;
            Resize((int)w, (int)h);

            RenderTexture2D* src = &viewport;
            RenderTexture2D* dst = &a_;

            for (auto* pp : effects)
            {
                if (!pp) continue;
                Shader sh = shaders_[(int)pp->effect];
                SetEffectUniforms_(sh, *pp, w, h);

                BeginTextureMode(*dst);
                    ClearBackground(BLANK);
                    BeginShaderMode(sh);
                        // Negative source height = vertical flip (preserve orientation)
                        DrawTexturePro(src->texture,
                            ::Rectangle{ 0, 0, w, -h },
                            ::Rectangle{ 0, 0, w,  h },
                            Vector2{ 0, 0 }, 0.0f, WHITE);
                    EndShaderMode();
                EndTextureMode();

                src = dst;
                dst = (dst == &a_) ? &b_ : &a_;
            }

            // Copy the final result back into the viewport if it isn't already there.
            if (src != &viewport)
            {
                BeginTextureMode(viewport);
                    DrawTexturePro(src->texture,
                        ::Rectangle{ 0, 0, w, -h },
                        ::Rectangle{ 0, 0, w,  h },
                        Vector2{ 0, 0 }, 0.0f, WHITE);
                EndTextureMode();
            }
        }

    private:
        bool loaded_ = false;
        std::array<Shader, (int)PostEffect::COUNT> shaders_{};
        RenderTexture2D a_{};
        RenderTexture2D b_{};

        void SetEffectUniforms_(Shader sh, const PostProcessComponent& pp, float w, float h)
        {
            // Helper to set a float uniform by name (location looked up each call;
            // effect chains are tiny so this is negligible).
            auto setF = [&](const char* name, float v)
            {
                int loc = GetShaderLocation(sh, name);
                if (loc != -1) SetShaderValue(sh, loc, &v, SHADER_UNIFORM_FLOAT);
            };
            auto setV2 = [&](const char* name, float x, float y)
            {
                int loc = GetShaderLocation(sh, name);
                if (loc != -1) { float v[2] = { x, y }; SetShaderValue(sh, loc, v, SHADER_UNIFORM_VEC2); }
            };

            setV2("resolution", w, h);

            switch (pp.effect)
            {
                case PostEffect::Grayscale:
                case PostEffect::Invert:
                    setF("intensity", pp.intensity);
                    break;
                case PostEffect::Vignette:
                    setF("intensity", pp.intensity);
                    setF("radius",    pp.vignetteRadius);
                    break;
                case PostEffect::ChromaticAberration:
                    setF("intensity",  pp.intensity);
                    setF("aberration", pp.aberration);
                    break;
                case PostEffect::Scanlines:
                    setF("intensity", pp.intensity);
                    setF("density",   pp.scanlineDensity);
                    break;
                case PostEffect::Pixelate:
                    setF("pixelSize", pp.pixelSize);
                    break;
                case PostEffect::ColorGrade:
                    setF("brightness", pp.brightness);
                    setF("contrast",   pp.contrast);
                    setF("saturation", pp.saturation);
                    break;
                default: break;
            }
        }

        // ---- Embedded GLSL 330 fragment shaders ----
        // raylib supplies: fragTexCoord, fragColor, texture0, colDiffuse.

        static constexpr const char* FS_GRAYSCALE = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform float intensity;
out vec4 finalColor;
void main()
{
    vec4 c = texture(texture0, fragTexCoord);
    float g = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    finalColor = vec4(mix(c.rgb, vec3(g), intensity), c.a);
}
)";

        static constexpr const char* FS_INVERT = R"(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float intensity;
out vec4 finalColor;
void main()
{
    vec4 c = texture(texture0, fragTexCoord);
    finalColor = vec4(mix(c.rgb, 1.0 - c.rgb, intensity), c.a);
}
)";

        static constexpr const char* FS_VIGNETTE = R"(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float intensity;
uniform float radius;
out vec4 finalColor;
void main()
{
    vec4 c = texture(texture0, fragTexCoord);
    vec2 uv = fragTexCoord - 0.5;
    float d = length(uv);
    float vig = 1.0 - smoothstep(radius * 0.5, radius, d);
    finalColor = vec4(c.rgb * mix(1.0, vig, intensity), c.a);
}
)";

        static constexpr const char* FS_ABERRATION = R"(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float intensity;
uniform float aberration;
uniform vec2 resolution;
out vec4 finalColor;
void main()
{
    vec2 dir = fragTexCoord - 0.5;
    vec2 amt = dir * (aberration / max(resolution.x, 1.0)) * intensity;
    vec4 c;
    c.r = texture(texture0, fragTexCoord + amt).r;
    c.g = texture(texture0, fragTexCoord).g;
    c.b = texture(texture0, fragTexCoord - amt).b;
    c.a = texture(texture0, fragTexCoord).a;
    finalColor = c;
}
)";

        static constexpr const char* FS_SCANLINES = R"(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float intensity;
uniform float density;
uniform vec2 resolution;
out vec4 finalColor;
void main()
{
    vec4 c = texture(texture0, fragTexCoord);
    float s = sin(fragTexCoord.y * resolution.y * 3.14159265 * density);
    float scan = 0.5 + 0.5 * s;          // 0..1
    float line = mix(1.0, scan, intensity * 0.6);
    finalColor = vec4(c.rgb * line, c.a);
}
)";

        static constexpr const char* FS_PIXELATE = R"(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float pixelSize;
uniform vec2 resolution;
out vec4 finalColor;
void main()
{
    vec2 px = vec2(max(pixelSize, 1.0)) / resolution;
    vec2 uv = (floor(fragTexCoord / px) + 0.5) * px;
    finalColor = texture(texture0, uv);
}
)";

        static constexpr const char* FS_COLORGRADE = R"(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float brightness;
uniform float contrast;
uniform float saturation;
out vec4 finalColor;
void main()
{
    vec4 c = texture(texture0, fragTexCoord);
    vec3 col = c.rgb + brightness;
    col = (col - 0.5) * contrast + 0.5;
    float g = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(g), col, saturation);
    finalColor = vec4(clamp(col, 0.0, 1.0), c.a);
}
)";
    };
}
