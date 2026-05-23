/**********************************************************************************************
*
*   Plane - Flat surface primitive entity (compatibility shim)
*
*   Rendering and collision are now handled by ShapeRendererComponent and BoxCollider2D.
*   This subclass preserves the "Plane" type string for scene JSON backward compatibility.
*
**********************************************************************************************/

#pragma once

#include "../../core/Entity.hpp"
#include "../component/Collider2D.hpp"
#include "../component/ShapeRendererComponent.hpp"
#include "imgui.h"
#include "raylib.h"
#include <memory>
#include <vector>

namespace Indium
{
    struct Plane : Entity
    {
        Plane()
        {
            addComponent<BoxCollider2D>();
            addComponent<ShapeRendererComponent>(); // default: ShapeType::Rectangle
        }

        // --- Backward-compat geometry (editor selection) ---

        ::Rectangle getBounds() const override
        {
            const auto* col = getComponent<BoxCollider2D>();
            if (col) return col->getBounds();
            Vector2 gPos = getGlobalPosition();
            Vector2 gScl = getGlobalScale();
            return { gPos.x - gScl.x * 0.5f, gPos.y - gScl.y * 0.5f, gScl.x, gScl.y };
        }

        std::vector<Vector2> getVertices() const override
        {
            const auto* col = getComponent<BoxCollider2D>();
            if (col) return col->getVertices();
            return {};
        }

        bool Contains(Vector2 point) const override
        {
            const auto* col = getComponent<BoxCollider2D>();
            if (col) return col->contains(point);
            return CheckCollisionPointRec(point, getBounds());
        }

        void inspect(std::function<void()> snapshotCb = {}) override
        {
            Entity::inspect(snapshotCb);
        }

        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Plane>(*this);
        }

        std::string getType() const override { return "Plane"; }
    };
}
