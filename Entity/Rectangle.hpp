/**********************************************************************************************
*
*   Rectangle - Rectangular primitive entity
*
*   A world object supporting rotation, scaling, and Axis-Aligned Bounding Box 
*   (AABB) collision logic.
*
*   Copyright (c) 2026
*
**********************************************************************************************/

#pragma once

#include "raylib.h"
#include "raymath.h"
#include "Entity.hpp"
#include "../include/imgui.h"
#include "memory"

namespace Indium
{
    /**
     * @brief A rectangular primitive entity.
     * 
     * The Rectangle entity supports rotation and scaling. It uses Raylib's 
     * DrawRectanglePro for advanced rendering, allowing it to be rotated around 
     * its center point.
     */
    struct Rectangle : Entity
    {
        /** 
         * @brief Renders the rectangle with support for rotation.
         * 
         * We pass {scale.x/2, scale.y/2} as the origin to ensure the rectangle 
         * rotates around its center rather than its top-left corner.
         */
        void draw() const override
        {
           DrawRectanglePro(
               {position.x, position.y, scale.x, scale.y}, 
               {scale.x / 2.0f, scale.y / 2.0f}, 
               rotation, 
               color
           );
        }

        /** @brief Standard Axis-Aligned Bounding Box (AABB) collision check. */
        bool collidesWith(Entity* other) override
        {
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

        /** @brief Returns the rectangle's spatial bounds in world space. */
        ::Rectangle getBounds() override
        {
            return {position.x, position.y, scale.x, scale.y};
        }

        /** 
         * @brief Checks if a point is within the rectangle's visual area.
         * 
         * Note: Currently uses a simplified circular approximation based on the 
         * maximum dimension for selection purposes.
         */
        bool Contains(Vector2 point) override
        {
            Vector2 center = { position.x, position.y };
            float hw = scale.x / 2.0f;
            float hh = scale.y / 2.0f;
            float maxR = fmaxf(hw, hh);
            return CheckCollisionPointCircle(point, center, maxR);
        }

        /** @brief Exposes transformation and color properties to the Editor Inspector. */
        void inspect() override
        {
            Entity::inspect();

            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Rectangle Properties");

            ImGui::DragFloat("Rotation", &rotation, 0.5f, -360.0f, 360.0f);
            ImGui::DragFloat2("Position", &position.x, 1.0f);
            ImGui::DragFloat2("Scale", &scale.x, 1.0f);

            float col[4] = {
                color.r / 255.0f,
                color.g / 255.0f,
                color.b / 255.0f,
                color.a / 255.0f
            };

            if (ImGui::ColorEdit4("Color", col))
            {
                color.r = (unsigned char)(col[0] * 255);
                color.g = (unsigned char)(col[1] * 255);
                color.b = (unsigned char)(col[2] * 255);
                color.a = (unsigned char)(col[3] * 255);
            }
        }

        /** @brief Creates a deep copy of the Rectangle. */
        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Rectangle>(*this);
        }
    };
}
