#pragma once
#include "../../core/Component.hpp"
#include "../../include/nlohmann/json.hpp"
#include "imgui.h"

namespace Indium
{
    // --------------------------------------------------------------------
    // AudioListenerComponent
    //
    // Marks an entity as the "ear" for spatial audio. Exactly one listener
    // should exist in the scene (typically on the camera or player entity).
    // AudioSourceComponent checks for this component each frame when its
    // isSpatial flag is enabled, and attenuates volume by distance.
    // --------------------------------------------------------------------
    struct AudioListenerComponent : Component
    {
        void update(float, Vector2, Scene*) override {}

        void inspect(std::function<void()> /*snapshotCb*/) override
        {
            ImGui::TextDisabled("Marks this entity as the spatial audio\nreference point (the \"ear\").");
            ImGui::Spacing();
            ImGui::TextDisabled("Attach to the Camera or Player entity.\nOnly one listener should be active.");
        }

        std::string getName() const override { return "AudioListener"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<AudioListenerComponent>(*this);
        }

        nlohmann::json serialize() const override { return Component::serialize(); }
        void deserialize(const nlohmann::json& j) override { Component::deserialize(j); }
    };
}
