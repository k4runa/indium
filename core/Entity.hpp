#pragma once
#include "raylib.h"
#include "raymath.h"
#include "iostream"
#include "memory"
#include "vector"
#include "Component.hpp"
#include "imgui.h"

namespace Indium
{
    /**
     * @brief Forward declaration of Scene to allow components to interact with the world.
     */
    struct Scene;

    /**
     * @brief The primary object container in the Indium engine.
     *
     * Everything that exists in the game world is an Entity. By itself, an Entity
     * is a lightweight container with basic spatial properties (position, scale, rotation).
     * Behavior is added by attaching Components.
     */
    struct Entity
    {
        /** @brief Unique identifier for the entity (useful for networking or serialization). */
        int         id{};

        /** @brief Human-readable name displayed in the Editor Hierarchy. */
        std::string name;

        /** @brief World-space position of the entity. */
        Vector2     position{0, 0};

        /** @brief Size multiplier of the entity. */
        Vector2     scale{1, 1};

        /** @brief Primary color tint for rendering. */
        Color       color{WHITE};

        /** @brief Velocity vector for movement-based components. */
        Vector2     velocity{0, 0};

        /** @brief Current rotation in degrees. */
        float       rotation = 0.0f;

        /**
         * @brief Internal list of logic modules attached to this entity.
         *
         * We use std::unique_ptr to manage component ownership automatically.
         * When an Entity is destroyed, all its components are cleaned up as well.
         */
        std::vector<std::unique_ptr<Component>> components;

        /** @brief Default constructor for creating empty entities. */
        Entity() = default;

        /**
         * @brief Copy constructor performing a deep-clone of components.
         *
         * This is used by the Scene system to create snapshots. It ensures that
         * the new entity has its own instances of every component, rather than
         * just copying pointers.
         */
        Entity(const Entity& other)
            : name(other.name), position(other.position), scale(other.scale),
              color(other.color), velocity(other.velocity), rotation(other.rotation)
        {
            for (auto& c : other.components)
            {
                auto cloned = c->clone();
                cloned->owner = this;
                components.push_back(std::move(cloned));
            }
        }

        /** @brief Virtual destructor to ensure derived entity types (like Circle, Rectangle) cleanup properly. */
        virtual ~Entity() = default;

        /** @brief Returns the axis-aligned bounding box (AABB) of the entity in world space. */
        virtual ::Rectangle getBounds() = 0;

        /** @brief Returns the 4 vertices of the entity in world space (for polygons/OBB). */
        virtual std::vector<Vector2> getVertices() { return {}; }

        /** @brief Implementation of collision detection logic against another entity. */
        virtual bool collidesWith(Entity* other) = 0;

        /**
         * @brief Dynamically attaches a component to the entity.
         *
         * @tparam T The component type to add.
         * @tparam Args Constructor arguments for the component.
         * @return A raw pointer to the newly created component for immediate configuration.
         */
        template<typename T, typename... Args>
        T* addComponent(Args&&... args)
        {
            auto comp = std::make_unique<T>(std::forward<Args>(args)...);
            comp->owner = this;
            T* ptr = comp.get();
            components.push_back(std::move(comp));
            return ptr;
        }

        /** @brief Safely removes a component by its index in the internal vector. */
        void removeComponent(int index)
        {
            if (index >= 0 && index < (int)components.size())
            {
                components.erase(components.begin() + index);
            }
        }

        /** @brief Pure virtual draw method. Derived classes must implement their specific rendering logic. */
        virtual void draw() const = 0;

        /** @brief Updates the entity and triggers the update cycle for all attached components. */
        virtual void update(float dt, Vector2 worldSize, Scene* scene)
        {
            for (auto& c : components)
            {
                c->update(dt, worldSize, scene);
            }
        }

        /** @brief Checks if a world-space point is contained within the entity's visual bounds. */
        virtual bool Contains(Vector2 point) = 0;

        /** @brief Returns a unique_ptr to a new Entity that is an exact copy of this one. */
        virtual std::unique_ptr<Entity> clone() = 0;

        /**
         * @brief Renders the Entity's state and its components into the ImGui Inspector panel.
         *
         * This provides a recursive inspection: it draws the basic Entity fields,
         * and then calls inspect() on every attached component.
         */
        virtual void inspect()
        {
            char buf[64];
            strncpy(buf, name.c_str(), sizeof(buf));
            if (ImGui::InputText("Name", buf, sizeof(buf)))
            {
                name = buf;
            }
            ImGui::Separator();

            int removeIndex = -1;
            for (int i = 0; i < (int)components.size(); i++)
            {
                ImGui::PushID(i);

                bool open = ImGui::CollapsingHeader(
                    components[i]->getName().c_str(),
                    ImGuiTreeNodeFlags_DefaultOpen
                );

                // Right-click context menu to remove components from the inspector
                if (ImGui::BeginPopupContextItem("comp_ctx"))
                {
                    if (ImGui::MenuItem("Remove Component"))
                        removeIndex = i;
                    ImGui::EndPopup();
                }

                if (open)
                {
                    components[i]->inspect();
                }

                ImGui::PopID();
            }

            if (removeIndex != -1)
                removeComponent(removeIndex);
        }
    };
}
