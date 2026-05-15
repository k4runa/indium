#pragma once

#include "raylib.h"
#include "raymath.h"
#include "../../core/Entity.hpp"
#include "imgui.h"
#include "memory"
#include "vector"

namespace Indium
{
    /**
     * @brief A circular primitive entity.
     *
     * The Circle entity provides specialized collision detection (Circle-vs-Circle)
     * and a simplified property set focused on radial dimensions.
     */
    struct Circle : Entity
    {
        /** @brief The distance from the center to the edge of the circle. */
        float radius = 50.0f;

        /** @brief Renders the circle using Raylib's optimized DrawCircleV. */
        void draw() const override
        {
            DrawCircleV(position, radius, color);
        }

        /**
         * @brief Specialized collision logic for the Circle.
         *
         * This method uses an optimized radial distance check if the 'other' entity
         * is also a Circle. For all other types, it falls back to Axis-Aligned
         * Bounding Box (AABB) checks.
         */
        bool collidesWith(Entity* other) override
        {
            Circle* c = dynamic_cast<Circle*>(other);

            // Optimization: Circle-vs-Circle collision is faster and more accurate than AABB
            if(c) return CheckCollisionCircles(position, radius, c->position, c->radius);

            // Fallback: Use standard AABB collision for mixed types
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

        /** @brief Calculates the smallest bounding rectangle that contains the circle. */
        ::Rectangle getBounds() override
        {
            return { position.x - radius, position.y - radius, radius * 2.0f, radius * 2.0f };
        }

        /** @brief Checks if a point is within the circle's radius. */
        bool Contains(Vector2 point) override
        {
            return CheckCollisionPointCircle(point, position, radius);
        }

        /** @brief Exposes radial and spatial properties to the Editor Inspector. */
        void inspect() override
        {
            Entity::inspect();

            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Circle Properties");

            ImGui::DragFloat2("Position", &position.x, 1.0f);
            ImGui::DragFloat("Radius", &radius, 0.5f, 1.0f, 1000.0f);

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

        /** @brief Creates a deep copy of the Circle, including its unique properties. */
        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Circle>(*this);
        }
    };
}
