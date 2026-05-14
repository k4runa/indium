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
           DrawRectanglePro({position.x, position.y, scale.x, scale.y},{scale.x / 2 , scale.y / 2},rotation, color);
        }

        bool collidesWith(Entity* other) override
        {
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

        ::Rectangle getBounds() override
        {
            return {position.x - scale.x / 2.0f, position.y - scale.y / 2.0f, scale.x, scale.y};
        }

        bool Contains(Vector2 point) override
        {
            Vector2 center = { position.x, position.y };
            float hw = scale.x / 2;
            float hh = scale.y / 2;
            float maxR = fmaxf(hw, hh);
            return CheckCollisionPointCircle(point, center, maxR);
        }
        void inspect() override
        {
            Entity::inspect();

            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Rectangle Properties");

            ImGui::DragFloat("Rotation",&rotation,0.5f,-360.0f,360.0f);
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
