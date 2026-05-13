#pragma once
#include "raylib.h"
#include <string>
#include <memory>
#include "../include/imgui.h"

namespace Indium
{
    // Forward declaration — breaks circular dependency without void*
    struct Entity;

    /**
     * @brief Base class for all components that can be attached to entities.
     */
    struct Component
    {
        Entity* owner = nullptr;

        /** @brief Called every frame with delta time and world boundaries */
        virtual void update(float dt, Vector2 worldSize) = 0;

        /** @brief Draw the component's properties in the ImGui Inspector */
        virtual void inspect() {}

        virtual std::string getName() const = 0;
        virtual std::unique_ptr<Component> clone() const = 0;
        virtual ~Component() = default;
    };
}
