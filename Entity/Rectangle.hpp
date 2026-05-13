#pragma once

#include "raylib.h"
#include "raymath.h"
#include "Entity.hpp"
#include "../include/imgui.h"
#include "memory"


namespace Indium
{
    /**
     * @brief A simple rectangle entity.
     */
    struct Rectangle : Entity
    {
        void draw() const override
        {
            DrawRectangleV(position, scale, color);
        }

        bool Contains(Vector2 point) override
        {
            return CheckCollisionPointRec(point, { position.x, position.y, scale.x, scale.y });
        }

        void inspect() override
        {
            Entity::inspect();
            
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Rectangle Properties");

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

        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Rectangle>(*this);
        }
    };


}
