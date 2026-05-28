#include "CameraComponent.hpp"
#include <cmath>
#include <cstring>
#include "raymath.h"
#include "imgui.h"
#include "../../core/Entity.hpp"
#include "../../core/scene/Scene.hpp"
#include "../../core/events/GameEvents.hpp"

namespace Indium
{
    CameraComponent::CameraComponent(const CameraComponent& o)
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

    void CameraComponent::start(Scene*)
    {
        zoomTarget_ = zoom;
        shakeHandle_ = Events::Subscribe<GameEvents::CameraShakeEvent>(
            [this](const GameEvents::CameraShakeEvent& e) { AddTrauma(e.trauma); }
        );
    }

    void CameraComponent::destroy(Scene*)
    {
        shakeHandle_.Unsubscribe();
    }

    void CameraComponent::update(float dt, Vector2 /*worldSize*/, Scene* scene)
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
                        float t = (followSmoothing > 0.0f) ? (1.0f - expf(-followSmoothing * dt)) : 1.0f;
                        owner->position.x += (targetPos.x - owner->position.x) * t;
                        owner->position.y += (targetPos.y - owner->position.y) * t;
                    }
                    break;
                }
            }
        }

        // --- Bounds clamp (viewport-aware) ---
        if (boundsEnabled && owner)
        {
            float halfViewW = (viewportPx_.x * 0.5f) / zoom;
            float halfViewH = (viewportPx_.y * 0.5f) / zoom;
            float clampXMin = boundsMin.x + halfViewW;
            float clampXMax = boundsMax.x - halfViewW;
            float clampYMin = boundsMin.y + halfViewH;
            float clampYMax = boundsMax.y - halfViewH;
            if (clampXMin > clampXMax) { float mid = (boundsMin.x + boundsMax.x) * 0.5f; clampXMin = clampXMax = mid; }
            if (clampYMin > clampYMax) { float mid = (boundsMin.y + boundsMax.y) * 0.5f; clampYMin = clampYMax = mid; }
            owner->position.x = Clamp(owner->position.x, clampXMin, clampXMax);
            owner->position.y = Clamp(owner->position.y, clampYMin, clampYMax);
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

            float interval = (shakeFrequency > 0.0f) ? 1.0f / shakeFrequency : 1.0f / 24.0f;
            sampleTimer_ += dt;
            while (sampleTimer_ >= interval)
            {
                sampleTimer_    -= interval;
                prevSample_      = nextSample_;
                prevAngleSample_ = nextAngleSample_;
                nextSample_.x    = GetRandomValue(-1000, 1000) / 1000.0f;
                nextSample_.y    = GetRandomValue(-1000, 1000) / 1000.0f;
                nextAngleSample_ = GetRandomValue(-1000, 1000) / 1000.0f;
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

    void CameraComponent::inspect(std::function<void()> snapshotCb)
    {
        // --- Zoom ---
        ImGui::Text("Zoom");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##Zoom", &zoom, 0.05f, zoomMin, zoomMax, "%.2fx"))
            zoomTarget_ = zoom;
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::Checkbox("Primary Camera", &isPrimary);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

        ImGui::Spacing();
        ImGui::TextDisabled("Zoom Limits");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##ZoomMin", &zoomMin, 0.01f, 0.01f, zoomMax, "Min %.2f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##ZoomMax", &zoomMax, 0.1f, zoomMin, 32.0f, "Max %.1f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##ZoomSmooth", &zoomSmoothing, 0.1f, 0.0f, 20.0f, "Smooth %.1f (0=instant)");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

        ImGui::Spacing();
        ImGui::Separator();

        // --- Rotation ---
        ImGui::Text("Rotation");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##BaseRot", &baseRotation, 0.5f, -180.0f, 180.0f, "%.1f\xC2\xB0");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

        ImGui::Spacing();
        ImGui::Separator();

        // --- Follow ---
        ImGui::TextDisabled("Follow");
        ImGui::Checkbox("Enable Follow", &followEnabled);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

        char buf[64] = {};
        strncpy(buf, followTargetName.c_str(), sizeof(buf) - 1);
        ImGui::Text("Target Entity Name");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##FollowTarget", buf, sizeof(buf))) followTargetName = buf;
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::Text("Smoothing");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##FollowSmooth", &followSmoothing, 0.1f, 0.0f, 20.0f, "%.1f (0=snap)");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::Text("Offset");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat2("##FollowOffset", &followOffset.x, 1.0f);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

        ImGui::Spacing();

        // Dead zone
        ImGui::Checkbox("Dead Zone##DZEn", &deadZoneEnabled);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        if (deadZoneEnabled)
        {
            ImGui::Text("Dead Zone Size");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat2("##DZSize", &deadZoneSize.x, 1.0f, 0.0f, 2000.0f, "%.0f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // --- Bounds ---
        ImGui::TextDisabled("World Bounds");
        ImGui::Checkbox("Clamp To Bounds##BndEn", &boundsEnabled);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        if (boundsEnabled)
        {
            ImGui::Text("Min (top-left)");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat2("##BndMin", &boundsMin.x, 4.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::Text("Max (bottom-right)");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat2("##BndMax", &boundsMax.x, 4.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // --- Shake ---
        ImGui::TextDisabled("Shake");
        ImGui::Text("Max Offset (px)");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##ShkOffset", &shakeMaxOffset, 0.5f, 0.0f, 200.0f, "%.0f px");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

        ImGui::Text("Max Angle (deg)");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##ShkAngle", &shakeMaxAngle, 0.1f, 0.0f, 45.0f, "%.1f\xC2\xB0");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

        ImGui::Text("Decay Rate");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##ShkDecay", &shakeDecay, 0.05f, 0.1f, 10.0f, "%.2f / sec");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

        ImGui::Text("Frequency");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##ShkFreq", &shakeFrequency, 0.5f, 1.0f, 60.0f, "%.0f Hz");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

        ImGui::Spacing();
        ImGui::TextDisabled("Trauma: %.2f", trauma_);
        if (ImGui::Button("Test Shake", ImVec2(-1, 0))) AddTrauma(0.5f);
    }

    std::unique_ptr<Component> CameraComponent::clone() const
    {
        return std::make_unique<CameraComponent>(*this);
    }

    std::string CameraComponent::getName() const { return "Camera Component"; }

    nlohmann::json CameraComponent::serialize() const
    {
        nlohmann::json j      = Component::serialize(); // writes "type" + "enabled"
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

    void CameraComponent::deserialize(const nlohmann::json& j)
    {
        Component::deserialize(j); // restore "enabled" (and any future base fields)
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
        if (j.contains("shakeMaxOffset"))   shakeMaxOffset = j["shakeMaxOffset"];
        if (j.contains("shakeMaxAngle"))    shakeMaxAngle  = j["shakeMaxAngle"];
        if (j.contains("shakeDecay"))       shakeDecay     = j["shakeDecay"];
        if (j.contains("shakeFrequency"))   shakeFrequency = j["shakeFrequency"];
    }
}
