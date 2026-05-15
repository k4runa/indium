#pragma once

#include "raylib.h"
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "imgui.h"

namespace Indium
{
    /**
     * @brief A component that allows an Entity to act as a Camera.
     *
     * When attached to an Entity, the engine can use this component's zoom and the
     * Entity's position to render the scene from this perspective during Play mode.
     */
    struct CameraComponent : Component
    {
        /** @brief Camera zoom level. Higher means closer. */
        float zoom = 1.0f;

        /** @brief If true, the engine will prefer this camera for rendering. */
        bool isPrimary = true;

        CameraComponent() = default;

        void update(float dt, Vector2 worldBounds, Scene* scene) override
        {
            // The camera component itself doesn't need to update logic every frame.
            // It just holds the data that the Editor or Scene will use for rendering.
        }

        void inspect() override
        {
            ImGui::DragFloat("Zoom", &zoom, 0.05f, 0.1f, 10.0f);
            ImGui::Checkbox("Primary Camera", &isPrimary);
        }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<CameraComponent>(*this);
        }

        std::string getName() const override
        {
            return "Camera Component";
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j;
            j["type"] = getName();
            j["zoom"] = zoom;
            j["isPrimary"] = isPrimary;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            if (j.contains("zoom")) zoom = j["zoom"];
            if (j.contains("isPrimary")) isPrimary = j["isPrimary"];
        }
    };
}
