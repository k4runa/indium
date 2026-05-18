#pragma once

#include "raylib.h"
#include "raymath.h"
#include "../../core/Entity.hpp"
#include "../component/Collider2D.hpp"
#include "../component/ShapeRendererComponent.hpp"
#include "imgui.h"
#include <memory>

namespace Indium
{
    /**
     * @brief Rectangle entity — compatibility shim.
     *
     * Rendering and collision are now handled by ShapeRendererComponent and
     * BoxCollider2D. This subclass preserves the "Rectangle" type string in
     * scene JSON for backward compatibility.
     */
    struct Rectangle : Entity
    {
        Rectangle()
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

        std::string getType() const override { return "Rectangle"; }

        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Rectangle>(*this);
        }
    };
}
