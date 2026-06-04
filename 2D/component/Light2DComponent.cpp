#include "Light2DComponent.hpp"
#include "raymath.h"
#include "imgui.h"
#include <cmath>

namespace Indium
{
    Light2DComponent::Light2DComponent()
    {
        // Randomize the flicker phase so several torches placed at once don't pulse in lockstep.
        flickerPhase_ = (float)GetRandomValue(0, 6283) / 1000.0f; // 0 .. ~2π
    }

    float Light2DComponent::EffectiveIntensity() const
    {
        if (flickerAmount <= 0.0f) return intensity;
        // Map sin(-1..1) → (1-flickerAmount .. 1) so flicker only ever dims, never overbrightens.
        float wave = 0.5f * (1.0f + sinf((float)GetTime() * flickerSpeed + flickerPhase_));
        float mult = 1.0f - flickerAmount * (1.0f - wave);
        return intensity * mult;
    }

    std::string Light2DComponent::getName() const { return "Light2D"; }

    void Light2DComponent::inspect(std::function<void()> snapshotCb)
    {
        const char* typeNames[] = { "Point", "Spot", "Directional" };
        int typeIdx = (int)type;
        ImGui::Text("Type");
        ImGui::PushItemWidth(-1);
        if (ImGui::Combo("##LightType", &typeIdx, typeNames, 3))
        {
            if (snapshotCb) snapshotCb();
            type = (LightType)typeIdx;
        }
        ImGui::PopItemWidth();
        ImGui::TextDisabled("Needs Scene lighting enabled");
        ImGui::Spacing();

        ImGui::Text("Color");
        float c[3] = { color.r / 255.f, color.g / 255.f, color.b / 255.f };
        ImGui::PushItemWidth(-1);
        if (ImGui::ColorEdit3("##LightColor", c))
        {
            color.r = (unsigned char)(c[0] * 255);
            color.g = (unsigned char)(c[1] * 255);
            color.b = (unsigned char)(c[2] * 255);
        }
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        if (type != LightType::Directional)
        {
            ImGui::Text(type == LightType::Spot ? "Range" : "Radius");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##LightRadius", &radius, 1.0f, 1.0f, 8000.0f, "%.0f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
        }

        ImGui::Text("Intensity");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##LightIntensity", &intensity, 0.01f, 0.0f, 2.0f, "%.2f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        if (type == LightType::Spot)
        {
            ImGui::Text("Cone Angle");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##LightCone", &coneAngle, 1.0f, 1.0f, 180.0f, "%.0f deg");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::TextDisabled("Aim with the entity's Rotation (0 = right)");
        }

        if (type != LightType::Directional)
        {
            ImGui::Checkbox("Cast Shadows", &castShadows);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            
            if (castShadows)
            {
                ImGui::Indent(8.0f);
                ImGui::Text("Shadow Softness");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat("##LightShadowSoft", &shadowSoftness, 0.5f, 0.0f, 100.0f, "%.1f");
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
                ImGui::Unindent(8.0f);
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Flicker (torch / fire effect)");

        ImGui::Text("Flicker Amount");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##LightFlickerAmt", &flickerAmount, 0.01f, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();
        ImGui::TextDisabled("0 = steady light");

        if (flickerAmount > 0.0f)
        {
            ImGui::Text("Flicker Speed");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##LightFlickerSpd", &flickerSpeed, 0.1f, 0.1f, 60.0f, "%.1f /s");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
        }
    }

    std::unique_ptr<Component> Light2DComponent::clone() const
    {
        auto c = std::make_unique<Light2DComponent>();
        c->enabled       = enabled;
        c->type          = type;
        c->color         = color;
        c->radius        = radius;
        c->intensity     = intensity;
        c->coneAngle     = coneAngle;
        c->castShadows   = castShadows;
        c->shadowSoftness = shadowSoftness;
        c->flickerAmount = flickerAmount;
        c->flickerSpeed  = flickerSpeed;
        // flickerPhase_ intentionally left at the clone's freshly randomized value.
        return c;
    }

    nlohmann::json Light2DComponent::serialize() const
    {
        nlohmann::json j = Component::serialize();
        j["lightType"]     = (int)type;
        j["color"]         = { color.r, color.g, color.b, color.a };
        j["radius"]        = radius;
        j["intensity"]     = intensity;
        j["coneAngle"]     = coneAngle;
        j["castShadows"]   = castShadows;
        j["shadowSoftness"] = shadowSoftness;
        j["flickerAmount"] = flickerAmount;
        j["flickerSpeed"]  = flickerSpeed;
        return j;
    }

    void Light2DComponent::deserialize(const nlohmann::json& j)
    {
        Component::deserialize(j);
        auto g = [&](const char* k, auto& v) { if (j.contains(k)) v = j[k].get<std::decay_t<decltype(v)>>(); };
        g("radius",        radius);
        g("intensity",     intensity);
        g("coneAngle",     coneAngle);
        g("castShadows",   castShadows);
        g("shadowSoftness", shadowSoftness);
        g("flickerAmount", flickerAmount);
        g("flickerSpeed",  flickerSpeed);
        if (j.contains("lightType")) type = (LightType)j["lightType"].get<int>();
        if (j.contains("color"))
            color = { j["color"][0], j["color"][1], j["color"][2], j["color"][3] };
    }
}
