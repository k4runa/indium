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
            Vector2 gPos = getGlobalPosition();
            DrawCircleV(gPos, radius, color);
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
            if(c) return CheckCollisionCircles(getGlobalPosition(), radius, c->getGlobalPosition(), c->radius);

            // Fallback: Use standard AABB collision for mixed types
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

        /** @brief Calculates the smallest bounding rectangle that contains the circle. */
        ::Rectangle getBounds() const override
        {
            Vector2 gPos = getGlobalPosition();
            return { gPos.x - radius, gPos.y - radius, radius * 2.0f, radius * 2.0f };
        }

        /** @brief Checks if a point is within the circle's radius. */
        bool Contains(Vector2 point) const override
        {
            return CheckCollisionPointCircle(point, getGlobalPosition(), radius);
        }

        /** @brief Exposes radial properties to the Editor Inspector. */
        void inspect() override
        {
            Entity::inspect();

            if (ImGui::CollapsingHeader("Circle", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent(8.0f);

                ImGui::Text("Radius");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat("##Radius", &radius, 0.5f, 1.0f, 1000.0f);
                ImGui::PopItemWidth();

                ImGui::Unindent(8.0f);
            }
        }

        /** @brief Creates a deep copy of the Circle, including its unique properties. */
        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Circle>(*this);
        }

        std::string getType() const override
        {
            return "Circle";
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Entity::serialize();
            j["radius"] = radius;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Entity::deserialize(j);
            if (j.contains("radius")) radius = j["radius"];
        }
    };
}
