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

#include "../../core/Entity.hpp"
#include "imgui.h"
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
            Vector2 gPos = getGlobalPosition();
            Vector2 gScale = getGlobalScale();
            float gRot = getGlobalRotation();
            DrawRectanglePro(
                {gPos.x, gPos.y, gScale.x, gScale.y},
                {gScale.x / 2.0f, gScale.y / 2.0f},
                gRot,
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
            Vector2 gPos = getGlobalPosition();
            Vector2 gScale = getGlobalScale();
            float gRot = getGlobalRotation();

            float hw = gScale.x / 2.0f;
            float hh = gScale.y / 2.0f;

            float dx = point.x - gPos.x;
            float dy = point.y - gPos.y;

            float rad = -gRot * DEG2RAD;
            float c = cosf(rad);
            float s = sinf(rad);

            float rx = dx * c - dy * s;
            float ry = dx * s + dy * c;

            return (rx >= -hw && rx <= hw && ry >= -hh && ry <= hh);
        }

        /** @brief Standard Axis-Aligned Bounding Box (AABB) collision check. */
        bool collidesWith(Entity* other) override
        {
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

        std::vector<Vector2> getVertices() override
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

        /** @brief Returns the plane's spatial bounds in world space. */
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

        /** @brief Exposes spatial and color properties to the Editor Inspector. */
        void inspect() override
        {
            Entity::inspect();
        }

        /** @brief Creates a deep copy of the Plane. */
        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Plane>(*this);
        }

        std::string getType() const override
        {
            return "Plane";
        }
    };
}

