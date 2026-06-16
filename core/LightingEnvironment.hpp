#pragma once
#include "LightingShader.hpp"
#include "scene/Scene.hpp"
#include "../2D/component/Light2DComponent.hpp"
#include <vector>
#include <cmath>

namespace Indium
{
    /**
     * @brief Per-frame snapshot of the scene's lights, shared by the 3D mesh shader and
     * the normal-mapped sprite pass so both shade from the same lights.
     *
     * Populated once per frame (Editor::Run / PlayerMain) right where LightMapRenderer is
     * driven, then read by MeshRendererComponent and SpriteRendererComponent during their
     * draws. The 2D light-map (LightMapRenderer) still owns ambient + flat illumination +
     * shadows; this adds the per-pixel directional term those passes can't express.
     *
     * Each light gets a Z derived from the parallax system: ParallaxFactor encodes apparent
     * depth (>1 = nearer/foreground, <1 = farther/background), so a light on a different
     * depthLayer than a surface lights it from a true 3D angle and attenuates over the real
     * distance — see LayerZ().
     */
    class LightingEnvironment
    {
    public:
        // World units of Z separation per unit of parallax-factor divergence from layer 0.
        static constexpr float kZScale = 200.0f;

        static LightingEnvironment& Get()
        {
            static LightingEnvironment inst;
            return inst;
        }

        /** @brief Maps a depthLayer to a Z depth from the layer's parallax factor.
         *  Layer 0 -> 0; background (factor < 1) -> +Z (deeper into the screen);
         *  foreground (factor > 1) -> -Z (toward the viewer).
         *
         *  Uses the factor directly (explicit per-layer override, else the default formula)
         *  rather than Scene::ParallaxFactor, which collapses to 1.0 for every layer when
         *  the parallax *scrolling* master switch is off. Depth layers are depth for
         *  lighting whether or not parallax scrolling is enabled — otherwise moving a light
         *  to another layer would change nothing in an unscrolled scene. */
        static float LayerZ(const Scene& s, int depthLayer)
        {
            if (depthLayer == 0) return 0.0f;
            auto it = s.parallaxByLayer.find(depthLayer);
            float factor = (it != s.parallaxByLayer.end()) ? it->second
                                                           : Scene::DefaultParallaxFactor(depthLayer);
            return kZScale * (1.0f - factor);
        }

        /** @brief Collect every enabled Light2D into world-space GpuLights (capped at
         *  kMaxLights). Mirrors LightMapRenderer's gather so the two passes agree. */
        void Gather(const Scene& scene)
        {
            using LT = Light2DComponent::LightType;
            scene_ = &scene;
            lights_.clear();
            ambient_ = { scene.ambientLight.r / 255.0f,
                         scene.ambientLight.g / 255.0f,
                         scene.ambientLight.b / 255.0f };

            for (const auto& e : scene.entities)
            {
                if (!e->activeInHierarchy()) continue;
                for (const auto& c : e->components)
                {
                    if (!c->enabled) continue;
                    auto* L = dynamic_cast<Light2DComponent*>(c.get());
                    if (!L) continue;
                    float eff = L->EffectiveIntensity();
                    if (eff <= 0.0f) continue;
                    if ((int)lights_.size() >= kMaxLights) break;

                    Vector2 p   = e->getGlobalPosition();
                    float   ang = e->getGlobalRotation() * DEG2RAD;

                    GpuLight g{};
                    g.pos       = { p.x, p.y, LayerZ(scene, e->depthLayer) };
                    g.color     = { L->color.r / 255.0f, L->color.g / 255.0f, L->color.b / 255.0f };
                    g.intensity = eff;
                    g.radius    = L->radius;
                    g.type      = (L->type == LT::Point) ? 0 : (L->type == LT::Spot) ? 1 : 2;
                    g.dir       = { cosf(ang), sinf(ang), 0.0f };
                    g.coneCos   = cosf(L->coneAngle * 0.5f * DEG2RAD);
                    lights_.push_back(g);
                }
            }
            active_ = !lights_.empty();
        }

        const std::vector<GpuLight>& Lights()  const { return lights_; }
        Vector3                      Ambient()  const { return ambient_; }
        bool                         Active()   const { return active_; }

        /** @brief Z for a surface on `depthLayer`, using the scene captured by the last
         *  Gather(). Lets a consumer (e.g. MeshRendererComponent) get its own parallax Z
         *  without holding the Scene. Returns 0 if no scene has been gathered yet. */
        float SurfaceZ(int depthLayer) const
        {
            return scene_ ? LayerZ(*scene_, depthLayer) : 0.0f;
        }

    private:
        std::vector<GpuLight> lights_;
        Vector3               ambient_ = { 0, 0, 0 };
        bool                  active_  = false;
        const Scene*          scene_   = nullptr;
    };
}
