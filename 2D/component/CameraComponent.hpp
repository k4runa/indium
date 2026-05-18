#pragma once

#include <cmath>
#include <cstring>
#include <string>

#include "raylib.h"
#include "raymath.h"
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../core/EventBus.hpp"
#include "../../core/events/GameEvents.hpp"
#include "../../core/scene/Scene.hpp"
#include "imgui.h"

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
        float       followSmoothing  = 8.0f;  // higher = snappier
        Vector2     followOffset     = {0.0f, 0.0f};

        // --- Dead zone (serialized) ---
        // Camera won't move until the target exits this region around the camera centre.
        bool    deadZoneEnabled = false;
        Vector2 deadZoneSize    = {80.0f, 60.0f};

        // --- Bounds clamp (serialized) ---
        // Clamps the camera's look-at point to stay inside the world rect.
        bool    boundsEnabled = false;
        Vector2 boundsMin     = {0.0f,    0.0f};
        Vector2 boundsMax     = {3200.0f, 1800.0f};

        // --- Shake config (serialized) ---
        float shakeMaxOffset = 24.0f;   // max translational offset in screen pixels
        float shakeMaxAngle  = 4.0f;    // max rotational jitter in degrees
        float shakeDecay     = 1.5f;    // trauma lost per second
        float shakeFrequency = 24.0f;   // new random sample draws per second

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

        // --- Scripting API ---

        /** @brief Accumulate trauma [0,1]. Multiple hits stack up to 1. */
        void AddTrauma(float amount)
        {
            trauma_ = Clamp(trauma_ + amount, 0.0f, 1.0f);
        }

        /** @brief Set trauma directly instead of accumulating. */
        void Shake(float trauma)
        {
            trauma_ = Clamp(trauma, 0.0f, 1.0f);
        }

        /** @brief Instantly set zoom (respects min/max). */
        void SetZoom(float z)
        {
            zoom = zoomTarget_ = Clamp(z, zoomMin, zoomMax);
        }

        /** @brief Smoothly animate zoom to target (uses zoomSmoothing). */
        void ZoomTo(float target)
        {
            zoomTarget_ = Clamp(target, zoomMin, zoomMax);
        }

        /** @brief Set the camera's base rotation in degrees. */
        void SetRotation(float deg) { baseRotation = deg; }

        /** @brief Enable smooth follow toward a named entity. Empty string disables. */
        void SetFollowTarget(const std::string& name)
        {
            followTargetName = name;
            followEnabled    = !name.empty();
        }

        // Read by Editor::GetActiveCamera() each frame
        Vector2 GetShakeOffset()       const { return shakeOffset_; }
        float   GetShakeAngle()        const { return shakeAngle_; }
        float   GetEffectiveRotation() const { return baseRotation; }

        // --- Component lifecycle ---

        CameraComponent() = default;

        // Custom copy constructor: copies config fields, zeroes all runtime state.
        // Necessary because SubscriptionHandle is move-only (non-copyable).
        CameraComponent(const CameraComponent& o)
            : zoom(o.zoom), isPrimary(o.isPrimary),
              zoomMin(o.zoomMin), zoomMax(o.zoomMax), zoomSmoothing(o.zoomSmoothing),
              baseRotation(o.baseRotation),
              followEnabled(o.followEnabled), followTargetName(o.followTargetName),
              followSmoothing(o.followSmoothing), followOffset(o.followOffset),
              deadZoneEnabled(o.deadZoneEnabled), deadZoneSize(o.deadZoneSize),
              boundsEnabled(o.boundsEnabled), boundsMin(o.boundsMin), boundsMax(o.boundsMax),
              shakeMaxOffset(o.shakeMaxOffset), shakeMaxAngle(o.shakeMaxAngle),
              shakeDecay(o.shakeDecay), shakeFrequency(o.shakeFrequency)
        { /* runtime fields left at zero defaults */ }

        void start(Scene* scene = nullptr) override
        {
            zoomTarget_ = zoom;
            // Explicit unsubscribe before re-subscribing — SubscriptionHandle's move-assign
            // does not call Unsubscribe on the displaced handle, so do it manually.
            shakeHandle_.Unsubscribe();
            shakeHandle_ = Events::Subscribe<GameEvents::CameraShakeEvent>(
                [this](const GameEvents::CameraShakeEvent& e) { AddTrauma(e.trauma); }
            );
        }

        void destroy(Scene* scene = nullptr) override
        {
            shakeHandle_.Unsubscribe();
        }

        void update(float dt, Vector2 /*worldSize*/, Scene* scene) override
        {
            // --- Follow: moves the camera entity toward the named target ---
            if (followEnabled && !followTargetName.empty() && scene && owner)
            {
                for (const auto& e : scene->entities)
                {
                    if (e->name == followTargetName)
                    {
                        Vector2 targetPos = {
                            e->getGlobalPosition().x + followOffset.x,
                            e->getGlobalPosition().y + followOffset.y
                        };

                        if (deadZoneEnabled)
                        {
                            // Only move camera when target exits the dead zone rect
                            float dx = targetPos.x - owner->position.x;
                            float dy = targetPos.y - owner->position.y;
                            float hx = deadZoneSize.x * 0.5f;
                            float hy = deadZoneSize.y * 0.5f;
                            if (dx >  hx) owner->position.x += dx - hx;
                            if (dx < -hx) owner->position.x += dx + hx;
                            if (dy >  hy) owner->position.y += dy - hy;
                            if (dy < -hy) owner->position.y += dy + hy;
                        }
                        else
                        {
                            // Frame-rate-independent exponential approach
                            float t = (followSmoothing > 0.0f)
                                ? (1.0f - expf(-followSmoothing * dt))
                                : 1.0f;
                            owner->position.x += (targetPos.x - owner->position.x) * t;
                            owner->position.y += (targetPos.y - owner->position.y) * t;
                        }
                        break;
                    }
                }
            }

            // --- Bounds clamp ---
            if (boundsEnabled && owner)
            {
                owner->position.x = Clamp(owner->position.x, boundsMin.x, boundsMax.x);
                owner->position.y = Clamp(owner->position.y, boundsMin.y, boundsMax.y);
            }

            // --- Smooth zoom ---
            if (zoomSmoothing > 0.0f)
            {
                float t = 1.0f - expf(-zoomSmoothing * dt);
                zoom   += (zoomTarget_ - zoom) * t;
            }
            else
            {
                zoom = zoomTarget_;
            }
            zoom = Clamp(zoom, zoomMin, zoomMax);

            // --- Shake ---
            if (trauma_ > 0.0f)
            {
                trauma_ = Clamp(trauma_ - shakeDecay * dt, 0.0f, 1.0f);
                float intensity = trauma_ * trauma_;

                // Advance the smoothed random sampler
                float interval = (shakeFrequency > 0.0f) ? 1.0f / shakeFrequency : 1.0f / 24.0f;
                sampleTimer_ += dt;
                while (sampleTimer_ >= interval)
                {
                    sampleTimer_     -= interval;
                    prevSample_       = nextSample_;
                    prevAngleSample_  = nextAngleSample_;
                    nextSample_.x     = GetRandomValue(-1000, 1000) / 1000.0f;
                    nextSample_.y     = GetRandomValue(-1000, 1000) / 1000.0f;
                    nextAngleSample_  = GetRandomValue(-1000, 1000) / 1000.0f;
                }

                float lerpT    = sampleTimer_ / interval;
                shakeOffset_.x = Lerp(prevSample_.x, nextSample_.x, lerpT) * shakeMaxOffset * intensity;
                shakeOffset_.y = Lerp(prevSample_.y, nextSample_.y, lerpT) * shakeMaxOffset * intensity;
                shakeAngle_    = Lerp(prevAngleSample_, nextAngleSample_, lerpT) * shakeMaxAngle * intensity;
            }
            else
            {
                trauma_      = 0.0f;
                shakeOffset_ = {0.0f, 0.0f};
                shakeAngle_  = 0.0f;
                sampleTimer_ = 0.0f;
            }
        }

        void inspect() override
        {
            // --- Zoom ---
            ImGui::Text("Zoom");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##Zoom", &zoom, 0.05f, zoomMin, zoomMax, "%.2fx"))
                zoomTarget_ = zoom;
            ImGui::Checkbox("Primary Camera", &isPrimary);

            ImGui::Spacing();
            ImGui::TextDisabled("Zoom Limits");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##ZoomMin", &zoomMin, 0.01f, 0.01f, zoomMax, "Min %.2f");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##ZoomMax", &zoomMax, 0.1f, zoomMin, 32.0f, "Max %.1f");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##ZoomSmooth", &zoomSmoothing, 0.1f, 0.0f, 20.0f, "Smooth %.1f (0=instant)");

            ImGui::Spacing();
            ImGui::Separator();

            // --- Rotation ---
            ImGui::Text("Rotation");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##BaseRot", &baseRotation, 0.5f, -180.0f, 180.0f, "%.1f\xC2\xB0");

            ImGui::Spacing();
            ImGui::Separator();

            // --- Follow ---
            ImGui::TextDisabled("Follow");
            ImGui::Checkbox("Enable Follow", &followEnabled);

            char buf[64] = {};
            strncpy(buf, followTargetName.c_str(), sizeof(buf) - 1);
            ImGui::Text("Target Entity Name");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##FollowTarget", buf, sizeof(buf)))
                followTargetName = buf;

            ImGui::Text("Smoothing");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##FollowSmooth", &followSmoothing, 0.1f, 0.0f, 20.0f, "%.1f (0=snap)");

            ImGui::Text("Offset");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat2("##FollowOffset", &followOffset.x, 1.0f);

            ImGui::Spacing();

            // Dead zone
            ImGui::Checkbox("Dead Zone##DZEn", &deadZoneEnabled);
            if (deadZoneEnabled)
            {
                ImGui::Text("Dead Zone Size");
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat2("##DZSize", &deadZoneSize.x, 1.0f, 0.0f, 2000.0f, "%.0f");
            }

            ImGui::Spacing();
            ImGui::Separator();

            // --- Bounds ---
            ImGui::TextDisabled("World Bounds");
            ImGui::Checkbox("Clamp To Bounds##BndEn", &boundsEnabled);
            if (boundsEnabled)
            {
                ImGui::Text("Min (top-left)");
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat2("##BndMin", &boundsMin.x, 4.0f);
                ImGui::Text("Max (bottom-right)");
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat2("##BndMax", &boundsMax.x, 4.0f);
            }

            ImGui::Spacing();
            ImGui::Separator();

            // --- Shake ---
            ImGui::TextDisabled("Shake");
            ImGui::Text("Max Offset (px)");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##ShkOffset", &shakeMaxOffset, 0.5f, 0.0f, 200.0f, "%.0f px");

            ImGui::Text("Max Angle (deg)");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##ShkAngle", &shakeMaxAngle, 0.1f, 0.0f, 45.0f, "%.1f\xC2\xB0");

            ImGui::Text("Decay Rate");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##ShkDecay", &shakeDecay, 0.05f, 0.1f, 10.0f, "%.2f / sec");

            ImGui::Text("Frequency");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##ShkFreq", &shakeFrequency, 0.5f, 1.0f, 60.0f, "%.0f Hz");

            ImGui::Spacing();
            ImGui::TextDisabled("Trauma: %.2f", trauma_);
            if (ImGui::Button("Test Shake", ImVec2(-1, 0)))
                AddTrauma(0.5f);
        }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<CameraComponent>(*this);
        }

        std::string getName() const override { return "Camera Component"; }

        nlohmann::json serialize() const override
        {
            nlohmann::json j;
            j["type"]             = getName();
            j["zoom"]             = zoom;
            j["isPrimary"]        = isPrimary;
            j["zoomMin"]          = zoomMin;
            j["zoomMax"]          = zoomMax;
            j["zoomSmoothing"]    = zoomSmoothing;
            j["baseRotation"]     = baseRotation;
            j["followEnabled"]    = followEnabled;
            j["followTargetName"] = followTargetName;
            j["followSmoothing"]  = followSmoothing;
            j["followOffset"]     = {followOffset.x, followOffset.y};
            j["deadZoneEnabled"]  = deadZoneEnabled;
            j["deadZoneSize"]     = {deadZoneSize.x, deadZoneSize.y};
            j["boundsEnabled"]    = boundsEnabled;
            j["boundsMin"]        = {boundsMin.x, boundsMin.y};
            j["boundsMax"]        = {boundsMax.x, boundsMax.y};
            j["shakeMaxOffset"]   = shakeMaxOffset;
            j["shakeMaxAngle"]    = shakeMaxAngle;
            j["shakeDecay"]       = shakeDecay;
            j["shakeFrequency"]   = shakeFrequency;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            if (j.contains("zoom"))             zoom             = j["zoom"];
            if (j.contains("isPrimary"))        isPrimary        = j["isPrimary"];
            if (j.contains("zoomMin"))          zoomMin          = j["zoomMin"];
            if (j.contains("zoomMax"))          zoomMax          = j["zoomMax"];
            if (j.contains("zoomSmoothing"))    zoomSmoothing    = j["zoomSmoothing"];
            if (j.contains("baseRotation"))     baseRotation     = j["baseRotation"];
            if (j.contains("followEnabled"))    followEnabled    = j["followEnabled"];
            if (j.contains("followTargetName")) followTargetName = j["followTargetName"].get<std::string>();
            if (j.contains("followSmoothing"))  followSmoothing  = j["followSmoothing"];
            if (j.contains("followOffset"))     { followOffset.x = j["followOffset"][0]; followOffset.y = j["followOffset"][1]; }
            if (j.contains("deadZoneEnabled"))  deadZoneEnabled  = j["deadZoneEnabled"];
            if (j.contains("deadZoneSize"))     { deadZoneSize.x = j["deadZoneSize"][0]; deadZoneSize.y = j["deadZoneSize"][1]; }
            if (j.contains("boundsEnabled"))    boundsEnabled    = j["boundsEnabled"];
            if (j.contains("boundsMin"))        { boundsMin.x = j["boundsMin"][0]; boundsMin.y = j["boundsMin"][1]; }
            if (j.contains("boundsMax"))        { boundsMax.x = j["boundsMax"][0]; boundsMax.y = j["boundsMax"][1]; }
            if (j.contains("shakeMaxOffset")) shakeMaxOffset = j["shakeMaxOffset"];
            if (j.contains("shakeMaxAngle"))  shakeMaxAngle  = j["shakeMaxAngle"];
            if (j.contains("shakeDecay"))     shakeDecay     = j["shakeDecay"];
            if (j.contains("shakeFrequency")) shakeFrequency = j["shakeFrequency"];
        }
    };
}
