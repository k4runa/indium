#pragma once
#include "raylib.h"
#include "raymath.h"
#include "iostream"
#include "memory"
#include "vector"
#include "../Component/Component.hpp"
#include "../include/imgui.h"

namespace Indium
{
    /**
     * @brief Base class for all objects in the game world.
     */
    struct Entity
    {
        int id{};
        std::string name;
        Vector2     position{0, 0};
        Vector2     scale{1, 1};
        Color       color{WHITE};
        Vector2     velocity{0, 0};

        // Components attached to this entity
        std::vector<std::unique_ptr<Component>> components;

        Entity() = default;
        Entity(const Entity& other) 
            : name(other.name), position(other.position), scale(other.scale), 
              color(other.color), velocity(other.velocity) {}
        
        virtual ~Entity() = default;

        /** @brief Draw the entity using Raylib */
        virtual void draw() const = 0;

        /** @brief Update logic for the entity and its components */
        virtual void update(float dt)
        {
            for (auto& c : components)
            {
                c->update(dt);
            }
        }

        /** @brief Check if a point (mouse) is inside the entity bounds */
        virtual bool Contains(Vector2 point) = 0;

        /** @brief Create a deep copy of the entity */
        virtual std::unique_ptr<Entity> clone() = 0;

        /** @brief Display entity properties in the ImGui Inspector */
        virtual void inspect()
        {
            char buf[64];
            strncpy(buf, name.c_str(), sizeof(buf));
            if (ImGui::InputText("Name", buf, sizeof(buf)))
            {
                name = buf;
            }
            ImGui::Separator();
        }
    };
}

