#pragma once

#include "raylib.h"
#include "raymath.h"
#include "../../core/Entity.hpp"
#include "../component/TilemapComponent.hpp"
#include <memory>
#include <string>

namespace Indium
{
    /**
     * @brief Tilemap entity — a transform that owns a TilemapComponent.
     *
     * Mirrors the Sprite/Circle pattern: the grid, tileset and collision are all
     * handled by TilemapComponent; this subclass exists to auto-attach it on
     * construction and to preserve the "Tilemap" type string in scene JSON so the
     * factory can rebuild the right entity on load. Lets the editor offer "create a
     * tilemap" alongside Sprite rather than forcing add-component onto a bare entity.
     */
    struct Tilemap : Entity
    {
        Tilemap()
        {
            name = "New Tilemap";
            addComponent<TilemapComponent>();
        }

        // --- Editor selection geometry ---
        // The tilemap is top-left anchored at the entity position and sized by the
        // grid (cols/rows × tile size × tileScale), independent of entity scale —
        // matching how TilemapComponent::draw lays tiles out.

        ::Rectangle getBounds() const override
        {
            const auto* tm = getComponent<TilemapComponent>();
            Vector2 origin = getGlobalPosition();
            if (tm)
            {
                float w = (float)tm->cols * (float)tm->tileW * tm->tileScale;
                float h = (float)tm->rows * (float)tm->tileH * tm->tileScale;
                return { origin.x, origin.y, w, h };
            }
            return Entity::getBounds();
        }

        bool Contains(Vector2 point) const override
        {
            return CheckCollisionPointRec(point, getBounds());
        }

        std::string getType() const override { return "Tilemap"; }

        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Tilemap>(*this);
        }
    };
}
