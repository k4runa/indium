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
        int         id{};
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
              color(other.color), velocity(other.velocity)
        {
            // Deep-clone all components
            for (auto& c : other.components)
            {
                auto cloned = c->clone();
                cloned->owner = this;
                components.push_back(std::move(cloned));
            }
        }

        virtual ~Entity() = default;

        /** @brief Attach a component to this entity */
        template<typename T, typename... Args>
        T* addComponent(Args&&... args)
        {
            auto comp = std::make_unique<T>(std::forward<Args>(args)...);
            comp->owner = this;
            T* ptr = comp.get();
            components.push_back(std::move(comp));
            return ptr;
        }

        /** @brief Remove a component by index */
        void removeComponent(int index)
        {
            if (index >= 0 && index < (int)components.size())
            {
                components.erase(components.begin() + index);
            }
        }

        /** @brief Draw the entity using Raylib */
        virtual void draw() const = 0;

        /** @brief Returns the world-space bounding box of the entity */
        virtual ::Rectangle getBounds() const { return { position.x, position.y, scale.x, scale.y }; }

        /** @brief Update logic for the entity and its components */
        virtual void update(float dt, Vector2 worldSize)
        {
            for (auto& c : components)
            {
                c->update(dt, worldSize);
            }
        }

        /** @brief Check if a point (mouse) is inside the entity bounds */
        virtual bool Contains(Vector2 point) = 0;

        /** @brief Create a deep copy of the entity */
        virtual std::unique_ptr<Entity> clone() = 0;

        /** @brief Display entity properties and components in the ImGui Inspector */
        virtual void inspect()
        {
            char buf[64];
            strncpy(buf, name.c_str(), sizeof(buf));
            if (ImGui::InputText("Name", buf, sizeof(buf)))
            {
                name = buf;
            }
            ImGui::Separator();

            // Show attached components
            int removeIndex = -1;
            for (int i = 0; i < (int)components.size(); i++)
            {
                ImGui::PushID(i);

                bool open = ImGui::CollapsingHeader(
                    components[i]->getName().c_str(),
                    ImGuiTreeNodeFlags_DefaultOpen
                );

                // Right-click context menu to remove
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
