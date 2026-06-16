#pragma once
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>
#include <string>

// GL-side lighting primitives shared by 3D meshes and normal-mapped sprites.
// Deliberately free of any Scene / component dependency so it can be included by
// low-level code (AssetManager) without creating an include cycle. The Scene-aware
// gathering lives in LightingEnvironment.hpp.
namespace Indium
{
    // A single light, expressed in whatever space the consumer shades in (meshes
    // transform these into their local render frame; sprites use them in world space).
    // pos.z / dir.z carry the parallax-derived depth (see LightingEnvironment::LayerZ).
    struct GpuLight
    {
        Vector3 pos;       // position (Point/Spot)
        Vector3 color;     // 0..1
        float   intensity; // brightness
        float   radius;    // reach (world units, or scaled into mesh-local units)
        int     type;      // 0 = Point, 1 = Spot, 2 = Directional (flat add)
        Vector3 dir;       // spot aim direction (normalized)
        float   coneCos;   // cos(coneAngle/2) for Spot
    };

    static constexpr int kMaxLights = 16;

    // GLSL 330 lighting block: uniform declarations + indiumLighting(). NO #version and
    // NO main() — callers prepend "#version 330\n" and append their own ins/outs/main so
    // the math is identical between the mesh and sprite fragment shaders. Directional
    // lights are a flat color add (matching the 2D light-map's "sun"); Point/Spot use
    // N·L with linear-squared attenuation and a soft spot cone.
    inline const char* LightingGlslBlock()
    {
        return
        "#define MAX_LIGHTS 16\n"
        "uniform vec3 uLightPos[MAX_LIGHTS];\n"
        "uniform vec3 uLightColor[MAX_LIGHTS];\n"
        "uniform vec4 uLightParam[MAX_LIGHTS];\n"   // x=intensity y=radius z=type w=coneCos
        "uniform vec3 uLightDir[MAX_LIGHTS];\n"
        "uniform int  uLightCount;\n"
        "uniform vec3 uAmbient;\n"
        "vec3 indiumLighting(vec3 P, vec3 Nin){\n"
        "  vec3 lit = uAmbient;\n"
        "  vec3 N = normalize(Nin);\n"
        "  for(int i=0;i<uLightCount;i++){\n"
        "    float intensity=uLightParam[i].x;\n"
        "    float radius=uLightParam[i].y;\n"
        "    int   type=int(uLightParam[i].z+0.5);\n"
        "    float coneCos=uLightParam[i].w;\n"
        "    if(type==2){ lit += uLightColor[i]*intensity; continue; }\n"
        "    vec3 toL = uLightPos[i]-P;\n"
        "    float dist = length(toL);\n"
        "    vec3 L = toL/max(dist,0.0001);\n"
        "    float atten = clamp(1.0 - dist/max(radius,0.0001), 0.0, 1.0);\n"
        "    atten *= atten;\n"
        "    if(type==1){\n"
        "      float sc = dot(normalize(uLightDir[i]), -L);\n"
        "      atten *= smoothstep(coneCos, mix(coneCos,1.0,0.5), sc);\n"
        "    }\n"
        "    float ndl = max(dot(N,L),0.0);\n"
        "    lit += uLightColor[i]*intensity*ndl*atten;\n"
        "  }\n"
        "  return lit;\n"
        "}\n";
    }

    // Cached uniform locations for the shared lighting block.
    struct LightingLocs
    {
        int pos=-1, color=-1, param=-1, dir=-1, count=-1, ambient=-1;
        void Fetch(Shader sh)
        {
            pos     = GetShaderLocation(sh, "uLightPos");
            color   = GetShaderLocation(sh, "uLightColor");
            param   = GetShaderLocation(sh, "uLightParam");
            dir     = GetShaderLocation(sh, "uLightDir");
            count   = GetShaderLocation(sh, "uLightCount");
            ambient = GetShaderLocation(sh, "uAmbient");
        }
    };

    // Upload a light set + ambient to a shader using the shared block. Shader must be
    // active (BeginShaderMode) or set via raylib's deferred uniform path. `lights` may be
    // any container of GpuLight with size()/operator[].
    template <typename Lights>
    inline void UploadLights(Shader sh, const LightingLocs& locs, const Lights& lights,
                             Vector3 ambient)
    {
        int n = (int)std::min((size_t)kMaxLights, lights.size());
        if (locs.count >= 0)   SetShaderValue(sh, locs.count, &n, SHADER_UNIFORM_INT);
        if (locs.ambient >= 0) SetShaderValue(sh, locs.ambient, &ambient, SHADER_UNIFORM_VEC3);
        if (n <= 0) return;

        float pos[kMaxLights * 3], col[kMaxLights * 3], par[kMaxLights * 4], dir[kMaxLights * 3];
        for (int i = 0; i < n; ++i)
        {
            const GpuLight& g = lights[i];
            pos[i*3+0]=g.pos.x;   pos[i*3+1]=g.pos.y;   pos[i*3+2]=g.pos.z;
            col[i*3+0]=g.color.x; col[i*3+1]=g.color.y; col[i*3+2]=g.color.z;
            par[i*4+0]=g.intensity; par[i*4+1]=g.radius; par[i*4+2]=(float)g.type; par[i*4+3]=g.coneCos;
            dir[i*3+0]=g.dir.x;   dir[i*3+1]=g.dir.y;   dir[i*3+2]=g.dir.z;
        }
        if (locs.pos   >= 0) SetShaderValueV(sh, locs.pos,   pos, SHADER_UNIFORM_VEC3, n);
        if (locs.color >= 0) SetShaderValueV(sh, locs.color, col, SHADER_UNIFORM_VEC3, n);
        if (locs.param >= 0) SetShaderValueV(sh, locs.param, par, SHADER_UNIFORM_VEC4, n);
        if (locs.dir   >= 0) SetShaderValueV(sh, locs.dir,   dir, SHADER_UNIFORM_VEC3, n);
    }

    // Lazily-compiled shader for the normal-mapped sprite additive pass, shared by every
    // SpriteRendererComponent. Samples albedo (texture0) + normal map (texture1) and emits
    // albedo * indiumLighting(worldPos, N) — additive relief on top of the 2D light-map.
    struct SpriteLightShader
    {
        Shader       shader{};
        LightingLocs lights;
        int          locNormalMap = -1;
        int          locCenter    = -1;
        int          locSize      = -1;
        int          locZ         = -1;
        bool         tried = false;
        bool         valid = false;

        static SpriteLightShader& Get()
        {
            static SpriteLightShader s;
            s.Ensure_();
            return s;
        }

    private:
        void Ensure_()
        {
            if (tried) return;
            tried = true;

            const char* vs =
                "#version 330\n"
                "in vec3 vertexPosition;\n"
                "in vec2 vertexTexCoord;\n"
                "in vec4 vertexColor;\n"
                "uniform mat4 mvp;\n"
                "out vec2 fragTexCoord;\n"
                "out vec4 fragColor;\n"
                "void main(){ fragTexCoord=vertexTexCoord; fragColor=vertexColor;"
                " gl_Position=mvp*vec4(vertexPosition,1.0); }\n";

            std::string fs =
                std::string("#version 330\n") + LightingGlslBlock() +
                "in vec2 fragTexCoord;\n"
                "in vec4 fragColor;\n"
                "uniform sampler2D texture0;\n"   // albedo (bound by DrawTexturePro)
                "uniform sampler2D texture1;\n"   // normal map
                "uniform vec4 colDiffuse;\n"
                "uniform vec2 uSpriteCenter;\n"
                "uniform vec2 uSpriteSize;\n"
                "uniform float uSpriteZ;\n"
                "out vec4 finalColor;\n"
                "void main(){\n"
                "  vec4 albedo = texture(texture0,fragTexCoord)*colDiffuse*fragColor;\n"
                "  vec3 n = texture(texture1,fragTexCoord).rgb*2.0-1.0;\n"
                "  vec3 N = normalize(vec3(n.x, -n.y, -n.z));\n"
                "  vec3 wp = vec3(uSpriteCenter.x + (fragTexCoord.x-0.5)*uSpriteSize.x,\n"
                "                 uSpriteCenter.y + (fragTexCoord.y-0.5)*uSpriteSize.y,\n"
                "                 uSpriteZ);\n"
                "  vec3 lit = indiumLighting(wp, N);\n"
                "  finalColor = vec4(albedo.rgb*lit, albedo.a);\n"
                "}\n";

            shader = LoadShaderFromMemory(vs, fs.c_str());
            if (shader.id != 0 && shader.id != rlGetShaderIdDefault())
            {
                lights.Fetch(shader);
                locNormalMap = GetShaderLocation(shader, "texture1");
                locCenter    = GetShaderLocation(shader, "uSpriteCenter");
                locSize      = GetShaderLocation(shader, "uSpriteSize");
                locZ         = GetShaderLocation(shader, "uSpriteZ");
                valid = true;
            }
        }
    };
}
