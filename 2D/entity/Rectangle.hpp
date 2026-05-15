#pragma once

#include "raylib.h"
#include "raymath.h"
#include "../../core/Entity.hpp"
#include "imgui.h"
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

        std::vector<Vector2> getVertices() override
        {
            std::vector<Vector2> vertices(4);

            float hw = scale.x / 2.0f;
            float hh = scale.y / 2.0f;

            float rad = rotation * DEG2RAD;
            float c = cosf(rad);
            float s = sinf(rad);

            Vector2 corners[4] = {
                {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
            };

            for (int i = 0; i < 4; i++) {
                vertices[i].x = position.x + (corners[i].x * c - corners[i].y * s);
                vertices[i].y = position.y + (corners[i].x * s + corners[i].y * c);
            }

            return vertices;
        }

        bool collidesWith(Entity* other) override
        {
            // We use this as a broad-phase AABB check now
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

        ::Rectangle getBounds() override
        {
            std::vector<Vector2> verts = getVertices();
            float minX = INFINITY, minY = INFINITY, maxX = -INFINITY, maxY = -INFINITY;
            for (const auto& v : verts) {
                minX = fminf(minX, v.x);
                minY = fminf(minY, v.y);
                maxX = fmaxf(maxX, v.x);
                maxY = fmaxf(maxY, v.y);
            }
            return {minX, minY, maxX - minX, maxY - minY};
        }

        bool Contains(Vector2 point) override
        {
            float hw = scale.x / 2.0f;
            float hh = scale.y / 2.0f;

            // Noktayı merkeze (orijine) göre ötele
            float dx = point.x - position.x;
            float dy = point.y - position.y;

            // Ters rotasyon uygula (Böylece kutuyu AABB'ye çevirmiş oluruz)
            float rad = -rotation * DEG2RAD;
            float c = cosf(rad);
            float s = sinf(rad);

            float rx = dx * c - dy * s;
            float ry = dx * s + dy * c;

            // Hizalanmış (AABB) sınırlar içinde mi kontrol et
            return (rx >= -hw && rx <= hw && ry >= -hh && ry <= hh);
        }
        void inspect() override
        {
            Entity::inspect();

            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Rectangle Properties");

            ImGui::SliderFloat("Rotation", &rotation, -360.0f, 360.0f);
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
