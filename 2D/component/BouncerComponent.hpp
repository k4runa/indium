/**********************************************************************************************
*
*   BouncerComponent - Automated movement with boundary reflection
*
*   A specialized logic module that adds velocity-based movement to an entity
*   and ensures it remains within the defined simulation world by bouncing
*   off the edges.
*
*   Copyright (c) 2026
*
**********************************************************************************************/

#pragma once

#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "raylib.h"

namespace Indium
{
    struct Scene;

    /**
     * @brief A logic module that provides basic physics-free movement.
     *
     * The BouncerComponent is ideal for simple automated objects (e.g., UI
     * decorative elements or basic projectiles) that don't require full
     * rigid-body simulation.
     */
    struct BouncerComponent : Component
    {
        /** @brief Speed along the X-axis in pixels per second. */
        float speedX = 150.0f;

        /** @brief Speed along the Y-axis in pixels per second. */
        float speedY = 100.0f;

        /**
         * @brief Updates position and handles edge reflections.
         *
         * @param dt Time elapsed since the last frame.
         * @param worldSize The boundaries of the active simulation area.
         * @param scene Pointer to the current scene context.
         */
        void update(float dt, Vector2 worldSize, Scene* scene) override {}

        void fixedUpdate(float fixedDt, Vector2 worldSize, Scene* scene) override
        {
            if (!owner) return;
            owner->position.x += speedX * fixedDt;
            owner->position.y += speedY * fixedDt;

            if (owner->position.x < 0)             { owner->position.x = 0;            speedX =  fabsf(speedX); }
            if (owner->position.x > worldSize.x)   { owner->position.x = worldSize.x;  speedX = -fabsf(speedX); }
            if (owner->position.y < 0)             { owner->position.y = 0;            speedY =  fabsf(speedY); }
            if (owner->position.y > worldSize.y)   { owner->position.y = worldSize.y;  speedY = -fabsf(speedY); }
        }

        /** @brief Exposes speed parameters to the Editor Inspector. */
        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::DragFloat("Speed X", &speedX, 1.0f, -500.0f, 500.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::DragFloat("Speed Y", &speedY, 1.0f, -500.0f, 500.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        }

        /** @brief Returns the human-readable identifier for the component. */
        std::string getName() const override { return "Bouncer"; }

        /** @brief Creates a deep copy of the BouncerComponent. */
        std::unique_ptr<Component> clone() const override
        {
            auto copy = std::make_unique<BouncerComponent>();
            copy->enabled = enabled;
            copy->speedX  = speedX;
            copy->speedY  = speedY;
            return copy;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["speedX"] = speedX;
            j["speedY"] = speedY;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j); // restore `enabled` (and any future base fields)
            if (j.contains("speedX")) speedX = j["speedX"];
            if (j.contains("speedY")) speedY = j["speedY"];
        }
    };
}
