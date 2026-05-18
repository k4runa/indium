#pragma once

#include "raylib.h"
#include "raymath.h"
#include "../../core/Entity.hpp"
#include "../component/Collider2D.hpp"
#include "../component/SpriteRendererComponent.hpp"
#include "imgui.h"
#include <memory>
#include <vector>
#include <string>

namespace Indium
{
    /**
     * @brief Sprite entity — compatibility shim.
     *
     * Texture rendering is now handled by SpriteRendererComponent and
     * collision by BoxCollider2D. This subclass preserves the "Sprite"
     * type string in scene JSON for backward compatibility.
     */
    struct Sprite : Entity
    {
        Sprite()
        {
            name = "New Sprite";
            addComponent<SpriteRendererComponent>();
            addComponent<BoxCollider2D>();
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

        bool Contains(Vector2 point) const override
        {
            const auto* col = getComponent<BoxCollider2D>();
            if (col) return col->contains(point);
            return CheckCollisionPointRec(point, getBounds());
        }

        std::string getType() const override { return "Sprite"; }

        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Sprite>(*this);
        }

        nlohmann::json serialize() const override
        {
            return Entity::serialize(); // SpriteRendererComponent serializes texture path
        }

        void deserialize(const nlohmann::json& j) override
        {
            Entity::deserialize(j);
            // Backward compat: old scenes stored texturePath / sourceRec at entity level
            auto* renderer = getComponent<SpriteRendererComponent>();
            if (!renderer) return;

            if (j.contains("texturePath") && !j["texturePath"].get<std::string>().empty())
            {
                renderer->Load(j["texturePath"].get<std::string>());
                // Restore saved sourceRec — Load() overwrites it with full texture bounds
                if (j.contains("sourceRec"))
                {
                    renderer->sourceRec.x      = j["sourceRec"][0];
                    renderer->sourceRec.y      = j["sourceRec"][1];
                    renderer->sourceRec.width  = j["sourceRec"][2];
                    renderer->sourceRec.height = j["sourceRec"][3];
                }
            }
        }
    };
}
