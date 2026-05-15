#pragma once
#include "raylib.h"
#include <string>
#include <memory>
#include "imgui.h"
#include "../include/nlohmann/json.hpp"

namespace Indium
{
    /**
     * @brief Forward declarations for circular dependencies.
     */
    struct Entity;
    struct Scene;

    /**
     * @brief The base class for all modular logic within the Indium engine.
     */
    struct Component
    {
        Entity* owner = nullptr;

        virtual void update(float dt, Vector2 worldSize, Scene* scene) = 0;
        virtual void inspect() {}
        virtual std::string getName() const = 0;
        virtual std::unique_ptr<Component> clone() const = 0;

        /** @brief Serializes the component data to JSON. */
        virtual nlohmann::json serialize() const
        {
            nlohmann::json j;
            j["type"] = getName();
            return j;
        }

        /** @brief Deserializes the component data from JSON. */
        virtual void deserialize(const nlohmann::json& j) {}

        virtual ~Component() = default;
    };
}
