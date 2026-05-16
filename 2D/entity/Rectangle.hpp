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
            Vector2 gPos = getGlobalPosition();
            Vector2 gScale = getGlobalScale();
            float gRot = getGlobalRotation();
            DrawRectanglePro({gPos.x, gPos.y, gScale.x, gScale.y}, {gScale.x / 2 , gScale.y / 2}, gRot, color);
        }

        std::vector<Vector2> getVertices() const override
        {
            std::vector<Vector2> vertices(4);

            Vector2 gPos = getGlobalPosition();
            Vector2 gScale = getGlobalScale();
            float gRot = getGlobalRotation();

            float hw = gScale.x / 2.0f;
            float hh = gScale.y / 2.0f;

            float rad = gRot * DEG2RAD;
            float c = cosf(rad);
            float s = sinf(rad);

            Vector2 corners[4] = {
                {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
            };

            for (int i = 0; i < 4; i++) {
                vertices[i].x = gPos.x + (corners[i].x * c - corners[i].y * s);
                vertices[i].y = gPos.y + (corners[i].x * s + corners[i].y * c);
            }

            return vertices;
        }

        bool collidesWith(Entity* other) override
        {
            // We use this as a broad-phase AABB check now
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

        ::Rectangle getBounds() const override
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

        bool Contains(Vector2 point) const override
        {
            Vector2 gPos = getGlobalPosition();
            Vector2 gScale = getGlobalScale();
            float gRot = getGlobalRotation();

            float hw = gScale.x / 2.0f;
            float hh = gScale.y / 2.0f;

            // Noktayı merkeze (orijine) göre ötele
            float dx = point.x - gPos.x;
            float dy = point.y - gPos.y;

            // Ters rotasyon uygula (Böylece kutuyu AABB'ye çevirmiş oluruz)
            float rad = -gRot * DEG2RAD;
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
        }

        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Rectangle>(*this);
        }

        std::string getType() const override
        {
            return "Rectangle";
        }
    };



}
