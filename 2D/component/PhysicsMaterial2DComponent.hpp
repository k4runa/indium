#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "RigidbodyComponent.hpp"
#include "../../include/nlohmann/json.hpp"
#include "imgui.h"
#include <string>

namespace Indium
{
    // --------------------------------------------------------------------
    // PhysicsMaterial2DComponent
    //
    // A reusable bundle of surface physics settings (bounciness + drag) that
    // configures the entity's Rigidbody.  Copy/paste it between entities to
    // share a feel — "Ice" (no bounce, low drag), "Rubber" (high bounce),
    // "Mud" (high drag) — instead of re-tuning each Rigidbody by hand.
    //
    // It applies its values to the RigidbodyComponent on Play start (and via
    // Apply() at runtime), so the Rigidbody stays the single source of truth
    // the physics resolver reads.
    // --------------------------------------------------------------------
    struct PhysicsMaterial2DComponent : Component
    {
        float bounciness = 0.3f;   // restitution → Rigidbody.bounciness (0..1)
        float linearDrag = 0.5f;   // velocity decay → Rigidbody.linearDrag
        float angularDrag = 5.0f;  // spin decay → Rigidbody.angularDrag

        void update(float, Vector2, Scene*) override {}

        void start(Scene*) override { Apply(); }

        // --- Script-facing ---
        void Apply()
        {
            if (!owner) return;
            if (auto* rb = owner->getComponent<RigidbodyComponent>())
            {
                rb->bounciness  = bounciness;
                rb->linearDrag  = linearDrag;
                rb->angularDrag = angularDrag;
            }
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::TextDisabled("Applies to this entity's Rigidbody on Play.");
            ImGui::Spacing();

            // Presets
            ImGui::Text("Preset");
            auto preset = [&](const char* name, float b, float ld, float ad)
            {
                if (ImGui::Button(name)) { if (snapshotCb) snapshotCb(); bounciness=b; linearDrag=ld; angularDrag=ad; Apply(); }
                ImGui::SameLine();
            };
            preset("Default", 0.3f, 0.5f, 5.0f);
            preset("Ice",     0.0f, 0.02f, 1.0f);
            preset("Rubber",  0.85f, 0.3f, 4.0f);
            preset("Mud",     0.0f, 6.0f, 12.0f);
            ImGui::NewLine();
            ImGui::Separator();

            ImGui::Text("Bounciness");
            ImGui::PushItemWidth(-1);
            ImGui::SliderFloat("##PMBounce", &bounciness, 0.0f, 1.0f, "%.2f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Text("Linear Drag");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##PMDrag", &linearDrag, 0.05f, 0.0f, 100.0f, "%.2f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Text("Angular Drag");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##PMADrag", &angularDrag, 0.05f, 0.0f, 100.0f, "%.2f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            if (owner && !owner->getComponent<RigidbodyComponent>())
                ImGui::TextColored(ImVec4(0.9f,0.6f,0.2f,1), "Needs a Rigidbody to take effect.");
        }

        std::string getName() const override { return "PhysicsMaterial2D"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<PhysicsMaterial2DComponent>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j  = Component::serialize();
            j["bounciness"]   = bounciness;
            j["linearDrag"]   = linearDrag;
            j["angularDrag"]  = angularDrag;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("bounciness"))  bounciness  = j["bounciness"].get<float>();
            if (j.contains("linearDrag"))  linearDrag  = j["linearDrag"].get<float>();
            if (j.contains("angularDrag")) angularDrag = j["angularDrag"].get<float>();
        }
    };
}
