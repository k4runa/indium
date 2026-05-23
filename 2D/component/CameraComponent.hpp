#pragma once
#include <string>
#include "raylib.h"
#include "raymath.h"
#include "../../core/Component.hpp"
#include "../../core/EventBus.hpp"

namespace Indium
{
    /**
     * @brief Makes an entity act as the scene camera with follow, shake, zoom, and rotation.
     *
     * The entity's position is the world point the camera looks at.
     * Follow mode moves the entity toward a named target each frame.
     * Shake is applied as a screen-space offset so the effect is zoom-independent.
     * All config fields serialize; runtime state (trauma, offsets) is zeroed on clone.
     */
    struct CameraComponent : Component
    {
        // --- Core (serialized) ---
        float zoom      = 1.0f;
        bool  isPrimary = true;

        // --- Zoom limits (serialized) ---
        float zoomMin       = 0.1f;
        float zoomMax       = 10.0f;
        float zoomSmoothing = 0.0f;   // 0 = instant; higher = more lag

        // --- Rotation (serialized) ---
        float baseRotation = 0.0f;

        // --- Follow (serialized) ---
        bool        followEnabled    = false;
        std::string followTargetName = "";
        float       followSmoothing  = 8.0f;
        Vector2     followOffset     = {0.0f, 0.0f};

        // --- Dead zone (serialized) ---
        bool    deadZoneEnabled = false;
        Vector2 deadZoneSize    = {80.0f, 60.0f};

        // --- Bounds clamp (serialized) ---
        bool    boundsEnabled = false;
        Vector2 boundsMin     = {0.0f,    0.0f};
        Vector2 boundsMax     = {3200.0f, 1800.0f};

        // --- Shake config (serialized) ---
        float shakeMaxOffset = 24.0f;
        float shakeMaxAngle  = 4.0f;
        float shakeDecay     = 1.5f;
        float shakeFrequency = 24.0f;

        // --- Runtime (never serialized, zeroed on clone) ---
        float   trauma_          = 0.0f;
        float   zoomTarget_      = 1.0f;
        Vector2 shakeOffset_     = {0.0f, 0.0f};
        float   shakeAngle_      = 0.0f;
        float   sampleTimer_     = 0.0f;
        Vector2 prevSample_      = {0.0f, 0.0f};
        Vector2 nextSample_      = {0.0f, 0.0f};
        float   prevAngleSample_ = 0.0f;
        float   nextAngleSample_ = 0.0f;
        SubscriptionHandle shakeHandle_;
        // Set by Editor each frame so bounds clamp can account for viewport size
        Vector2 viewportPx_      = {800.0f, 600.0f};

        // --- Scripting API (inline — called from hot paths / scripts) ---
        void AddTrauma(float amount) { trauma_ = Clamp(trauma_ + amount, 0.0f, 1.0f); }
        void Shake(float trauma)     { trauma_ = Clamp(trauma, 0.0f, 1.0f); }
        void SetZoom(float z)        { zoom = zoomTarget_ = Clamp(z, zoomMin, zoomMax); }
        void ZoomTo(float target)    { zoomTarget_ = Clamp(target, zoomMin, zoomMax); }
        void SetRotation(float deg)  { baseRotation = deg; }
        void SetFollowTarget(const std::string& name) { followTargetName = name; followEnabled = !name.empty(); }

        Vector2 GetShakeOffset()       const { return shakeOffset_; }
        float   GetShakeAngle()        const { return shakeAngle_; }
        float   GetEffectiveRotation() const { return baseRotation; }

        // --- Constructors ---
        CameraComponent() = default;
        // Custom copy ctor: copies config fields, zeroes runtime state.
        // Necessary because SubscriptionHandle is move-only (non-copyable).
        CameraComponent(const CameraComponent& o);

        // --- Component lifecycle ---
        void start(Scene* scene = nullptr) override;
        void destroy(Scene* scene = nullptr) override;
        void update(float dt, Vector2 worldSize, Scene* scene) override;
        void inspect(std::function<void()> snapshotCb) override;
        std::unique_ptr<Component> clone() const override;
        std::string getName() const override;
        nlohmann::json serialize() const override;
        void deserialize(const nlohmann::json& j) override;
    };
}
