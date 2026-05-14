/**********************************************************************************************
*
*   Plane - Flat surface primitive entity
*
*   A specialized thin rectangle used for representing ground planes, walls, 
*   and static boundaries.
*
*   Copyright (c) 2026
*
**********************************************************************************************/

#pragma once

#include "Entity.hpp"
#include "../include/imgui.h"
#include "raylib.h"
#include <memory>
#include <vector>

namespace Indium
{
    /**
     * @brief A flat plane primitive entity.
     *
     * The Plane is a specialized thin rectangle typically used for floors,
     * walls, or boundaries. It shares the same transformation properties as the
     * Rectangle but is optimized for "surface" use cases.
     */
    struct Plane : Entity
    {
        /** @brief Renders the plane using rotated rectangle primitives. */
        void draw() const override
        {
            DrawRectanglePro(
                {position.x, position.y, scale.x, scale.y},
                {scale.x / 2.0f, scale.y / 2.0f},
                rotation,
                color
            );
        }

        /**
         * @brief Checks if a point is within the plane's selectable area.
         *
         * Uses a circular approximation for easier selection in the editor.
         */
        bool Contains(Vector2 point) override
        {
            Vector2 center = { position.x, position.y };
            float hw = scale.x / 2.0f;
            float hh = scale.y / 2.0f;
            float maxR = fmaxf(hw, hh);
            return CheckCollisionPointCircle(point, center, maxR);
        }

        /** @brief Standard Axis-Aligned Bounding Box (AABB) collision check. */
        bool collidesWith(Entity* other) override
        {
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

        /** @brief Returns the plane's spatial bounds in world space. */
        ::Rectangle getBounds() override
        {
            return {position.x, position.y, scale.x, scale.y};
        }

        /** @brief Exposes spatial and color properties to the Editor Inspector. */
        void inspect() override
        {
            Entity::inspect();

            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Plane Properties");
            ImGui::DragFloat("Rotation", &rotation, 0.5f, -360.0f, 360.0f);
            ImGui::DragFloat2("Position", &position.x, 1.0f);
            ImGui::DragFloat2("Scale", &scale.x, 1.0f);

            // Interface for color selection
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

        /** @brief Creates a deep copy of the Plane. */
        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Plane>(*this);
        }
    };
}
