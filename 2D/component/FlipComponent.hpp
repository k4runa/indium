#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../include/nlohmann/json.hpp"
#include "SpriteRendererComponent.hpp"
#include "imgui.h"
#include <cmath>

namespace Indium
{
    // --------------------------------------------------------------------
    // FlipComponent
    //
    // Mirrors the entity horizontally and/or vertically.  When a
    // SpriteRendererComponent is present the flip is applied the canonical
    // raylib way — by negating the source rectangle width/height, which
    // never distorts the sprite.  For non-sprite entities it flips the
    // entity's scale sign instead.
    //
    // Modes:
    //   Manual         — flipX / flipY checkboxes drive the state
    //   AutoByVelocity — face the entity's horizontal velocity each frame
    //
    // The flip is applied in lateUpdate() so it runs AFTER the sprite
    // renderer (and any animator) has set this frame's source rect.
    // --------------------------------------------------------------------
    struct FlipComponent : Component
    {
        enum class Mode { Manual, AutoByVelocity };

        Mode  mode  = Mode::Manual;
        bool  flipX = false;
        bool  flipY = false;
        float deadzone = 1.0f;  // ignore |velocity.x| below this (AutoByVelocity)

        void update(float, Vector2, Scene*) override {}

        void lateUpdate(float, Vector2, Scene*) override
        {
            if (!owner) return;

            if (mode == Mode::AutoByVelocity)
            {
                if (owner->velocity.x >  deadzone) flipX = false;
                else if (owner->velocity.x < -deadzone) flipX = true;
            }

            applyFlip_();
        }

        void applyFlip_()
        {
            if (auto* sr = owner->getComponent<SpriteRendererComponent>())
            {
                // Canonical raylib flip: negate source width/height sign.
                float w = std::fabs(sr->sourceRec.width);
                float h = std::fabs(sr->sourceRec.height);
                sr->sourceRec.width  = flipX ? -w : w;
                sr->sourceRec.height = flipY ? -h : h;
            }
            else
            {
                // Fallback: flip the entity scale sign (idempotent via abs).
                float sx = std::fabs(owner->scale.x);
                float sy = std::fabs(owner->scale.y);
                owner->scale.x = flipX ? -sx : sx;
                owner->scale.y = flipY ? -sy : sy;
            }
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            const char* modes[] = { "Manual", "Auto By Velocity" };
            int m = (int)mode;
            ImGui::Text("Mode");
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##FlipMode", &m, modes, 2)) { if (snapshotCb) snapshotCb(); mode = (Mode)m; }
            ImGui::PopItemWidth();

            if (mode == Mode::Manual)
            {
                ImGui::Checkbox("Flip X", &flipX);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::SameLine();
                ImGui::Checkbox("Flip Y", &flipY);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            }
            else
            {
                ImGui::Text("Velocity Deadzone");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat("##FlipDead", &deadzone, 0.1f, 0.0f, 100.0f, "%.1f");
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
                ImGui::TextDisabled("Faces horizontal movement automatically.");
            }
        }

        std::string getName() const override { return "Flip"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<FlipComponent>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["mode"]     = (int)mode;
            j["flipX"]    = flipX;
            j["flipY"]    = flipY;
            j["deadzone"] = deadzone;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("mode"))     mode     = (Mode)j["mode"].get<int>();
            if (j.contains("flipX"))    flipX    = j["flipX"].get<bool>();
            if (j.contains("flipY"))    flipY    = j["flipY"].get<bool>();
            if (j.contains("deadzone")) deadzone = j["deadzone"].get<float>();
        }
    };
}
