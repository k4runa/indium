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
        void update(float dt, Vector2 worldSize, Scene* scene) override
        {
            if (!owner) return;
            Entity* ent = owner;

            // Apply movement based on current velocity
            ent->position.x += speedX * dt;
            ent->position.y += speedY * dt;

            float worldW = worldSize.x;
            float worldH = worldSize.y;

            // Retrieve the entity's visual bounds for precise collision with edges
            ::Rectangle bounds = ent->getBounds();

            // Horizontal Reflection Logic
            if (bounds.x < 0)
            {
                ent->position.x += (0 - bounds.x); // Resolve penetration
                speedX = -speedX;
            }
            else if (bounds.x + bounds.width > worldW)
            {
                ent->position.x -= (bounds.x + bounds.width - worldW); // Resolve penetration
                speedX = -speedX;
            }

            // Vertical Reflection Logic
            if (bounds.y < 0)
            {
                ent->position.y += (0 - bounds.y); // Resolve penetration
                speedY = -speedY;
            }
            else if (bounds.y + bounds.height > worldH)
            {
                ent->position.y -= (bounds.y + bounds.height - worldH); // Resolve penetration
                speedY = -speedY;
            }
        }

        /** @brief Exposes speed parameters to the Editor Inspector. */
        void inspect() override
        {
            ImGui::DragFloat("Speed X", &speedX, 1.0f, -500.0f, 500.0f);
            ImGui::DragFloat("Speed Y", &speedY, 1.0f, -500.0f, 500.0f);
        }

        /** @brief Returns the human-readable identifier for the component. */
        std::string getName() const override { return "Bouncer"; }

        /** @brief Creates a deep copy of the BouncerComponent. */
        std::unique_ptr<Component> clone() const override
        {
            auto copy = std::make_unique<BouncerComponent>();
            copy->speedX = speedX;
            copy->speedY = speedY;
            return copy;
        }
    };
}
