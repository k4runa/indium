#pragma once

#include "raylib.h"
#include "raymath.h"
#include "Entity.hpp"
#include "../include/imgui.h"
#include "memory"
#include "vector"


namespace Indium
{
    /**
     * @brief A simple circle entity.
     */
    struct Circle : Entity
    {
        float radius = 50.0f;

        void draw() const override
        {
            DrawCircleV(position, radius, color);
        }

        ::Rectangle getBounds() const override
        {
            return { position.x - radius, position.y - radius, radius * 2.0f, radius * 2.0f };
        }

        bool Contains(Vector2 point) override
        {
            return CheckCollisionPointCircle(point, position, radius);
        }

        void inspect() override
        {
            Entity::inspect();

            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Circle Properties");

            ImGui::DragFloat2("Position", &position.x, 1.0f);
            ImGui::DragFloat("Radius", &radius, 0.5f, 1.0f, 1000.0f);

            // Convert Raylib Color to ImVec4 for ImGui
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

        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Circle>(*this);
        }
    };
}
