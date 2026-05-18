#pragma once

#include "raylib.h"
#include "raymath.h"
#include "../../core/Entity.hpp"
#include "../component/Collider2D.hpp"
#include "../component/ShapeRendererComponent.hpp"
#include "imgui.h"
#include <memory>
#include <vector>

namespace Indium
{
    /**
     * @brief Circle entity — compatibility shim.
     *
     * Shape and collision are now handled by CircleCollider2D and
     * ShapeRendererComponent. This subclass exists solely to preserve the
     * "Circle" type string in scene JSON files and to auto-attach the
     * appropriate components on construction.
     */
    struct Circle : Entity
    {
        Circle()
        {
            addComponent<CircleCollider2D>();
            addComponent<ShapeRendererComponent>()->shapeType = ShapeRendererComponent::ShapeType::Circle;
        }

        // --- Backward-compat geometry helpers (editor selection still uses these) ---

        ::Rectangle getBounds() const override
        {
            const auto* col = getComponent<CircleCollider2D>();
            if (col) return col->getBounds();
            Vector2 gPos = getGlobalPosition();
            return { gPos.x - 50.0f, gPos.y - 50.0f, 100.0f, 100.0f };
        }

        bool Contains(Vector2 point) const override
        {
            const auto* col = getComponent<CircleCollider2D>();
            if (col) return col->contains(point);
            return CheckCollisionPointCircle(point, getGlobalPosition(), 50.0f);
        }

        std::string getType() const override { return "Circle"; }

        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Circle>(*this);
        }

        nlohmann::json serialize() const override
        {
            return Entity::serialize(); // components handle radius
        }

        void deserialize(const nlohmann::json& j) override
        {
            Entity::deserialize(j);
            // Backward compat: old scenes stored radius at entity level
            if (j.contains("radius"))
            {
                auto* col = getComponent<CircleCollider2D>();
                if (col) col->radius = j["radius"].get<float>();
            }
        }
    };
}
