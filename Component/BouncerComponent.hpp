#pragma once

#include "Component.hpp"
#include "../Entity/Entity.hpp"
#include "raylib.h"



namespace Indium
{
    /**
     * @brief Makes an entity bounce around the screen edges.
     *
     * A simple test component to verify the component system works.
     * Adds velocity-based movement and bounces off screen boundaries.
     */
    struct BouncerComponent : Component
    {
        float speedX = 150.0f;
        float speedY = 100.0f;

        void update(float dt, Vector2 worldSize) override
        {
            if (!owner) return;
            Entity* ent = owner;

            // Move the entity
            ent->position.x += speedX * dt;
            ent->position.y += speedY * dt;

            // Boundaries are passed from the scene
            float worldW = worldSize.x;
            float worldH = worldSize.y;

            // Use the new bounding box system
            ::Rectangle bounds = ent->getBounds();

            // Bounce X
            if (bounds.x < 0)
            {
                ent->position.x += (0 - bounds.x); // Push back in
                speedX = -speedX;
            }
            else if (bounds.x + bounds.width > worldW)
            {
                ent->position.x -= (bounds.x + bounds.width - worldW); // Push back in
                speedX = -speedX;
            }

            // Bounce Y
            if (bounds.y < 0)
            {
                ent->position.y += (0 - bounds.y); // Push back in
                speedY = -speedY;
            }
            else if (bounds.y + bounds.height > worldH)
            {
                ent->position.y -= (bounds.y + bounds.height - worldH); // Push back in
                speedY = -speedY;
            }
        }

        void inspect() override
        {
            ImGui::DragFloat("Speed X", &speedX, 1.0f, -500.0f, 500.0f);
            ImGui::DragFloat("Speed Y", &speedY, 1.0f, -500.0f, 500.0f);
        }

        std::string getName() const override { return "Bouncer"; }

        std::unique_ptr<Component> clone() const override
        {
            auto copy = std::make_unique<BouncerComponent>();
            copy->speedX = speedX;
            copy->speedY = speedY;
            return copy;
        }
    };
}
