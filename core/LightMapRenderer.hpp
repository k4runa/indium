#pragma once
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"   // immediate-mode vertices for spotlight cones + culling control for shadows
#include <utility>
#include <vector>
#include "scene/Scene.hpp"
#include "../2D/component/Light2DComponent.hpp"
#include "../2D/component/Collider2D.hpp"

namespace Indium
{
    /**
     * @brief The 2D lighting pass, shared by the editor viewport and the standalone
     * player so both render light identically.
     *
     * Owns the light accumulation buffer, the per-light scratch buffer and the baked
     * radial gradient. Usage each frame (raylib can't nest render targets, so Render()
     * must run BEFORE the world's own BeginTextureMode scope):
     *
     *   if (LightMapRenderer::SceneWantsLighting(scene)) lighting.Render(scene, cam);
     *   BeginTextureMode(worldRT);
     *       scene.Draw(cam);
     *       lighting.Composite(worldRT.texture.width, worldRT.texture.height);
     *   EndTextureMode();
     */
    class LightMapRenderer
    {
    public:
        void Init()
        {
            // Dummy size; Resize() matches the world render target each frame.
            lightMap_     = LoadRenderTexture(1, 1);
            lightScratch_ = LoadRenderTexture(1, 1);

            // Bake the radial light splat once (white core fading to transparent edge).
            // Each Light2DComponent draws this additively, tinted and scaled, into the
            // light map.
            Image grad     = GenImageGradientRadial(256, 256, 0.0f, WHITE, BLANK);
            lightGradient_ = LoadTextureFromImage(grad);
            UnloadImage(grad);
            SetTextureFilter(lightGradient_, TEXTURE_FILTER_BILINEAR);
        }

        void Shutdown()
        {
            UnloadRenderTexture(lightMap_);
            UnloadRenderTexture(lightScratch_);
            UnloadTexture(lightGradient_);
        }

        /** @brief Keeps the buffers the same size as the world render target. */
        void Resize(int w, int h)
        {
            if (w <= 0 || h <= 0) return;
            if (lightMap_.texture.width == w && lightMap_.texture.height == h) return;
            UnloadRenderTexture(lightMap_);
            UnloadRenderTexture(lightScratch_);
            lightMap_     = LoadRenderTexture(w, h);
            lightScratch_ = LoadRenderTexture(w, h);
        }

        /** @brief Lighting activates when the scene's master toggle is on OR the scene
         *  contains at least one enabled Light2D — so dropping a light in "just works"
         *  without hunting for the Project Settings switch. */
        static bool SceneWantsLighting(const Scene& scene)
        {
            if (scene.lightingEnabled) return true;
            for (const auto& e : scene.entities)
            {
                if (!e->activeInHierarchy()) continue;
                for (const auto& c : e->components)
                    if (c->enabled && dynamic_cast<Light2DComponent*>(c.get())) return true;
            }
            return false;
        }

        /** @brief Accumulates every active Light2DComponent into the light map (cleared
         *  to the scene's ambient color). Call before BeginTextureMode on the world
         *  target; the result is then composited via Composite(). */
        void Render(const Scene& scene, const Camera2D& cam)
        {
            using LT = Light2DComponent::LightType;

            // --- Gather solid (non-trigger) collider polygons once; reused as shadow occluders. ---
            std::vector<std::pair<Entity*, std::vector<Vector2>>> occluders;
            for (const auto& e : scene.entities)
            {
                if (!e->activeInHierarchy()) continue;
                auto* col = e->getComponent<Collider2D>();
                if (!col || col->isTrigger) continue;

                std::vector<Vector2> poly = col->getVertices();
                if (poly.empty() && col->isCircleShape())
                {
                    // Approximate a circle collider with a polygon so it can cast shadows too.
                    Vector2 cc = Vector2Add(e->getGlobalPosition(), col->offset);
                    float   r  = col->getCircleRadius();
                    const int N = 16;
                    poly.reserve(N);
                    for (int i = 0; i < N; ++i)
                    {
                        float ang = (float)i / N * 2.0f * PI;
                        poly.push_back({ cc.x + cosf(ang) * r, cc.y + sinf(ang) * r });
                    }
                }
                if (poly.size() >= 3) occluders.emplace_back(e.get(), std::move(poly));
            }

            const float gw = (float)lightGradient_.width;
            const float gh = (float)lightGradient_.height;

            // --- Accumulation buffer: ambient floor, then flat Directional lights. ---
            BeginTextureMode(lightMap_);
                ClearBackground(Color{ scene.ambientLight.r, scene.ambientLight.g, scene.ambientLight.b, 255 });

                BeginBlendMode(BLEND_ADDITIVE);
                for (const auto& e : scene.entities)
                {
                    if (!e->activeInHierarchy()) continue;
                    for (const auto& c : e->components)
                    {
                        if (!c->enabled) continue;
                        auto* light = dynamic_cast<Light2DComponent*>(c.get());
                        if (!light || light->type != LT::Directional) continue;
                        float eff = light->EffectiveIntensity();
                        if (eff <= 0.0f) continue;
                        auto a = (unsigned char)Clamp(eff * 255.0f, 0.0f, 255.0f);
                        DrawRectangle(0, 0, lightMap_.texture.width, lightMap_.texture.height,
                                      Color{ light->color.r, light->color.g, light->color.b, a });
                    }
                }
                EndBlendMode();
            EndTextureMode();

            // --- Point / Spot lights: render each (minus its shadows) to scratch, then add in. ---
            for (const auto& e : scene.entities)
            {
                if (!e->activeInHierarchy()) continue;
                for (const auto& c : e->components)
                {
                    if (!c->enabled) continue;
                    auto* light = dynamic_cast<Light2DComponent*>(c.get());
                    if (!light || light->type == LT::Directional) continue;

                    float eff = light->EffectiveIntensity();
                    if (eff <= 0.0f || light->radius <= 0.0f) continue;

                    Vector2 p    = e->getGlobalPosition();
                    auto    a    = (unsigned char)Clamp(eff * 255.0f, 0.0f, 255.0f);
                    Color   tint = { light->color.r, light->color.g, light->color.b, a };
                    float   d    = light->radius * 2.0f;

                    // 1) Draw this light's shape into the scratch buffer (over transparent black).
                    BeginTextureMode(lightScratch_);
                        ClearBackground(BLANK);
                        BeginMode2D(cam);

                            if (light->type == LT::Point)
                            {
                                DrawTexturePro(lightGradient_,
                                    ::Rectangle{ 0.0f, 0.0f, gw, gh },
                                    ::Rectangle{ p.x, p.y, d, d },
                                    Vector2{ d * 0.5f, d * 0.5f }, 0.0f, tint);
                            }
                            else // Spot: a cone fan, bright apex fading to nothing at the rim.
                            {
                                float baseA = e->getGlobalRotation() * DEG2RAD;
                                float halfA = light->coneAngle * 0.5f * DEG2RAD;
                                float range = light->radius;
                                const int SEG = 32;
                                rlSetTexture(rlGetTextureIdDefault()); // untextured white verts
                                rlBegin(RL_TRIANGLES);
                                for (int s = 0; s < SEG; ++s)
                                {
                                    float a0 = baseA - halfA + (2.0f * halfA) * ((float)s       / SEG);
                                    float a1 = baseA - halfA + (2.0f * halfA) * ((float)(s + 1) / SEG);
                                    rlColor4ub(tint.r, tint.g, tint.b, a);
                                    rlVertex2f(p.x, p.y);
                                    rlColor4ub(tint.r, tint.g, tint.b, 0);
                                    rlVertex2f(p.x + cosf(a0) * range, p.y + sinf(a0) * range);
                                    rlColor4ub(tint.r, tint.g, tint.b, 0);
                                    rlVertex2f(p.x + cosf(a1) * range, p.y + sinf(a1) * range);
                                }
                                rlEnd();
                            }

                            // 2) Carve shadows: extrude each occluder edge away from the light and
                            //    paint the resulting quad solid black, removing this light behind it.
                            if (light->castShadows)
                            {
                                rlDisableBackfaceCulling(); // shadow quad winding varies per edge
                                rlSetTexture(rlGetTextureIdDefault()); // draw untextured triangles

                                float reach = light->radius * 2.5f;
                                float softness = light->shadowSoftness;

                                for (auto& oc : occluders)
                                {
                                    if (oc.first == e.get()) continue; // a light never shadows itself
                                    const auto& poly = oc.second;
                                    size_t n = poly.size();
                                    for (size_t i = 0; i < n; ++i)
                                    {
                                        Vector2 va = poly[i];
                                        Vector2 vb = poly[(i + 1) % n];

                                        Vector2 dirA = Vector2Subtract(va, p);
                                        Vector2 dirB = Vector2Subtract(vb, p);
                                        float distA = Vector2Length(dirA);
                                        float distB = Vector2Length(dirB);

                                        if (distA <= 0.001f || distB <= 0.001f) continue;

                                        Vector2 ndirA = Vector2Scale(dirA, 1.0f / distA);
                                        Vector2 ndirB = Vector2Scale(dirB, 1.0f / distB);

                                        // Generate perpendiculars pointing outwards from the edge AB
                                        Vector2 perpA = { -ndirA.y, ndirA.x };
                                        Vector2 toB = Vector2Subtract(vb, va);
                                        if (Vector2DotProduct(perpA, toB) > 0.0f) perpA = Vector2Scale(perpA, -1.0f);

                                        Vector2 perpB = { -ndirB.y, ndirB.x };
                                        Vector2 toA = Vector2Subtract(va, vb);
                                        if (Vector2DotProduct(perpB, toA) > 0.0f) perpB = Vector2Scale(perpB, -1.0f);

                                        // Soft shadow math:
                                        // Outer penumbra ray diverges away from the edge.
                                        // Inner umbra ray converges towards the inside of the edge.
                                        float divA = softness / distA;
                                        float divB = softness / distB;

                                        Vector2 rOuterA = Vector2Normalize(Vector2Add(ndirA, Vector2Scale(perpA, divA)));
                                        Vector2 rInnerA = Vector2Normalize(Vector2Subtract(ndirA, Vector2Scale(perpA, divA)));

                                        Vector2 rOuterB = Vector2Normalize(Vector2Add(ndirB, Vector2Scale(perpB, divB)));
                                        Vector2 rInnerB = Vector2Normalize(Vector2Subtract(ndirB, Vector2Scale(perpB, divB)));

                                        Vector2 vaP_inner = Vector2Add(va, Vector2Scale(rInnerA, reach));
                                        Vector2 vbP_inner = Vector2Add(vb, Vector2Scale(rInnerB, reach));
                                        Vector2 vaP_outer = Vector2Add(va, Vector2Scale(rOuterA, reach));
                                        Vector2 vbP_outer = Vector2Add(vb, Vector2Scale(rOuterB, reach));

                                        rlBegin(RL_TRIANGLES);
                                            // 1. Solid Umbra core (BLACK)
                                            rlColor4ub(0, 0, 0, 255); rlVertex2f(va.x, va.y);
                                            rlColor4ub(0, 0, 0, 255); rlVertex2f(vb.x, vb.y);
                                            rlColor4ub(0, 0, 0, 255); rlVertex2f(vbP_inner.x, vbP_inner.y);

                                            rlColor4ub(0, 0, 0, 255); rlVertex2f(va.x, va.y);
                                            rlColor4ub(0, 0, 0, 255); rlVertex2f(vbP_inner.x, vbP_inner.y);
                                            rlColor4ub(0, 0, 0, 255); rlVertex2f(vaP_inner.x, vaP_inner.y);

                                            // 2. Left Penumbra triangle (va -> vaP_inner -> vaP_outer)
                                            // Fades from BLACK to BLANK (transparent)
                                            rlColor4ub(0, 0, 0, 255); rlVertex2f(va.x, va.y);
                                            rlColor4ub(0, 0, 0, 255); rlVertex2f(vaP_inner.x, vaP_inner.y);
                                            rlColor4ub(0, 0, 0, 0);   rlVertex2f(vaP_outer.x, vaP_outer.y);

                                            // 3. Right Penumbra triangle (vb -> vbP_outer -> vbP_inner)
                                            // Fades from BLACK to BLANK (transparent)
                                            rlColor4ub(0, 0, 0, 255); rlVertex2f(vb.x, vb.y);
                                            rlColor4ub(0, 0, 0, 0);   rlVertex2f(vbP_outer.x, vbP_outer.y);
                                            rlColor4ub(0, 0, 0, 255); rlVertex2f(vbP_inner.x, vbP_inner.y);
                                        rlEnd();
                                    }
                                }
                            }

                        EndMode2D();
                    EndTextureMode();

                    // 3) Add the shadowed light into the accumulation buffer with a straight RGB
                    //    add (BLEND_ADD_COLORS). The scratch is a render texture, so flip the
                    //    source height (-h) to undo raylib's bottom-up framebuffer orientation.
                    BeginTextureMode(lightMap_);
                        BeginBlendMode(BLEND_ADD_COLORS);
                        DrawTexturePro(lightScratch_.texture,
                            ::Rectangle{ 0.0f, 0.0f, (float)lightScratch_.texture.width, -(float)lightScratch_.texture.height },
                            ::Rectangle{ 0.0f, 0.0f, (float)lightMap_.texture.width,  (float)lightMap_.texture.height },
                            Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
                        EndBlendMode();
                    EndTextureMode();
                }
            }
        }

        /** @brief Multiplies the lit world by the accumulated light map. Call inside the
         *  world's BeginTextureMode scope, after the scene draw and before any overlays
         *  that should stay full-bright. The light map is a render texture, so its source
         *  rect height is flipped (-h) to undo raylib's bottom-up framebuffer orientation. */
        void Composite(int targetW, int targetH) const
        {
            BeginBlendMode(BLEND_MULTIPLIED);
            DrawTexturePro(
                lightMap_.texture,
                ::Rectangle{ 0.0f, 0.0f, (float)lightMap_.texture.width, -(float)lightMap_.texture.height },
                ::Rectangle{ 0.0f, 0.0f, (float)targetW, (float)targetH },
                Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
            EndBlendMode();
        }

    private:
        RenderTexture2D lightMap_     = { 0 };
        RenderTexture2D lightScratch_ = { 0 };
        Texture2D       lightGradient_ = { 0 };
    };
}
