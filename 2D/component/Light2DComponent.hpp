#pragma once
#include "../../core/Component.hpp"
#include "raylib.h"

namespace Indium
{
    /**
     * @brief A 2D point light that brightens the scene around the owning entity.
     *
     * Lighting is a screen-space post-process owned by the Editor, not a per-component
     * draw call: every enabled Light2DComponent on an active entity is accumulated
     * additively into a "light map" render texture (cleared to Scene::ambientLight),
     * which is then composited over the rendered world with BLEND_MULTIPLIED. The
     * component therefore stores only light data — the Editor reads `owner`'s world
     * position and these fields each frame (see Editor::RenderLightMap).
     *
     * The master switch and ambient floor live on the Scene (Scene::lightingEnabled /
     * Scene::ambientLight); a light contributes nothing until lighting is enabled.
     *
     * Script usage:
     *   auto* l = owner->getComponent<Light2DComponent>();
     *   l->intensity = 1.5f;
     *   l->SetColor(255, 120, 40);   // warm torch
     */
    struct Light2DComponent : Component
    {
        /** @brief Point = radial glow; Spot = cone aimed by the entity's rotation;
         *  Directional = flat global add (a "sun", no position/shadows). */
        enum class LightType { Point, Spot, Directional };

        // ---- Light data (serialized) ----
        LightType type        = LightType::Point;
        Color     color       = {255, 244, 214, 255}; // light tint (alpha ignored; intensity drives strength)
        float     radius      = 200.0f;               // reach in world units (Point/Spot; ignored for Directional)
        float     intensity   = 1.0f;                 // brightness multiplier (0 = off, 1 = full, >1 saturates)
        float     coneAngle   = 60.0f;                // Spot cone width in degrees (aimed by entity rotation, 0 = +X)
        bool      castShadows = true;                 // Point/Spot: solid colliders block this light
        float     shadowSoftness = 15.0f;             // Aperture / size of light source for soft shadows (0 = sharp)

        // ---- Flicker (serialized) — animates intensity for torches / fire ----
        float flickerAmount = 0.0f;  // 0 = steady, 1 = swings down to near-dark
        float flickerSpeed  = 8.0f;  // oscillations per second

        // ---- Runtime (never serialized) ----
        float flickerPhase_ = 0.0f;  // randomized per instance so lights don't pulse in sync

        // ---- Scripting helpers ----
        void SetColor(unsigned char r, unsigned char g, unsigned char b) { color = {r, g, b, 255}; }

        /** @brief Intensity for this frame, including flicker. Read by the Editor's light pass. */
        [[nodiscard]] float EffectiveIntensity() const;

        // ---- Component interface ----
        Light2DComponent();
        void update(float dt, Vector2 worldSize, Scene* scene) override {}
        void inspect(std::function<void()> snapshotCb = {}) override;
        std::string getName() const override;
        std::unique_ptr<Component> clone() const override;
        nlohmann::json serialize() const override;
        void deserialize(const nlohmann::json& j) override;
    };
}
