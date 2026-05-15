#pragma once
#include "raylib.h"
#include "raymath.h"
#include "iostream"
#include "memory"
#include "vector"
#include "Component.hpp"
#include "imgui.h"
#include "../include/nlohmann/json.hpp"

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
        int         id = 0;
        int         parentId = -1; // For serialization

        Entity*     parent = nullptr;
        std::vector<Entity*> children;

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

        /** @brief Sorting order for draw priority. Lower values are drawn first (behind). */
        int         sortingOrder = 0;

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
            : id(other.id), parentId(other.parentId), name(other.name),
              position(other.position), scale(other.scale),
              color(other.color), velocity(other.velocity), rotation(other.rotation),
              sortingOrder(other.sortingOrder)
        {
            // Note: Parent and children pointers are NOT deep cloned automatically here,
            // they must be relinked by the Scene after a full clone operation.
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

        /**
         * @brief Attaches an already instantiated component to the entity.
         *
         * @param comp The component unique_ptr to take ownership of.
         * @return A raw pointer to the component.
         */
        Component* addComponent(std::unique_ptr<Component> comp)
        {
            if (!comp) return nullptr;
            comp->owner = this;
            Component* ptr = comp.get();
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

        // --- Transform Hierarchy Methods ---

        Vector2 getGlobalPosition() const
        {
            if (parent)
            {
                // Simple vector addition for parent-child position.
                // Advanced: apply parent rotation and scale to the offset.
                Vector2 pPos = parent->getGlobalPosition();

                // Account for parent rotation
                float rad = parent->getGlobalRotation() * DEG2RAD;
                float c = cosf(rad);
                float s = sinf(rad);

                Vector2 rotatedOffset = {
                    position.x * c - position.y * s,
                    position.x * s + position.y * c
                };

                return { pPos.x + rotatedOffset.x, pPos.y + rotatedOffset.y };
            }
            return position;
        }

        float getGlobalRotation() const
        {
            if (parent) return parent->getGlobalRotation() + rotation;
            return rotation;
        }

        Vector2 getGlobalScale() const
        {
            if (parent)
            {
                Vector2 pScale = parent->getGlobalScale();
                return { scale.x * pScale.x, scale.y * pScale.y };
            }
            return scale;
        }

        void setGlobalPosition(Vector2 globalPos)
        {
            if (parent)
            {
                Vector2 pPos = parent->getGlobalPosition();
                float rad = -parent->getGlobalRotation() * DEG2RAD;
                float c = cosf(rad);
                float s = sinf(rad);

                Vector2 offset = { globalPos.x - pPos.x, globalPos.y - pPos.y };
                position = {
                    offset.x * c - offset.y * s,
                    offset.x * s + offset.y * c
                };
            }
            else
            {
                position = globalPos;
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
        /**
         * @brief Renders the Entity's state and its components into the ImGui Inspector panel.
         *
         * Organized into Unity-style collapsible sections:
         * 1. Entity header (name + type badge)
         * 2. Transform (Position, Rotation, Scale)
         * 3. Material (Color)
         * 4. Attached Components
         */
        virtual void inspect()
        {
            // --- Entity Header ---
            char buf[64];
            strncpy(buf, name.c_str(), sizeof(buf));
            if (ImGui::InputText("Name", buf, sizeof(buf)))
            {
                name = buf;
            }

            // Type badge
            ImGui::Spacing();
            ImGui::TextDisabled("[%s]", getType().c_str());

            ImGui::Spacing();

            // --- Transform Section ---
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent(8.0f);

                ImGui::Text("Position");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat2("##Position", &position.x, 1.0f);
                ImGui::PopItemWidth();

                ImGui::Text("Rotation");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat("##Rotation", &rotation, 1.0f, -360.0f, 360.0f, "%.1f\xC2\xB0");
                ImGui::PopItemWidth();

                ImGui::Text("Scale");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat2("##Scale", &scale.x, 0.5f);
                ImGui::PopItemWidth();

                ImGui::Text("Sorting Order");
                ImGui::PushItemWidth(-1);
                ImGui::DragInt("##SortingOrder", &sortingOrder, 1);
                ImGui::PopItemWidth();

                ImGui::Unindent(8.0f);
            }

            // --- Material Section ---
            if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent(8.0f);

                float col[4] = {
                    color.r / 255.0f,
                    color.g / 255.0f,
                    color.b / 255.0f,
                    color.a / 255.0f
                };

                ImGui::Text("Color");
                if (ImGui::ColorEdit4("##Color", col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf))
                {
                    color.r = (unsigned char)(col[0] * 255);
                    color.g = (unsigned char)(col[1] * 255);
                    color.b = (unsigned char)(col[2] * 255);
                    color.a = (unsigned char)(col[3] * 255);
                }

                ImGui::Unindent(8.0f);
            }

            // --- Components Section ---
            int removeIndex = -1;
            for (int i = 0; i < (int)components.size(); i++)
            {
                ImGui::PushID(i);

                bool open = ImGui::CollapsingHeader(components[i]->getName().c_str(), ImGuiTreeNodeFlags_DefaultOpen);

                if (ImGui::BeginPopupContextItem("comp_ctx"))
                {
                    if (ImGui::MenuItem("Remove Component")) removeIndex = i;
                    ImGui::EndPopup();
                }

                if (open)
                {
                    ImGui::Indent(8.0f);
                    components[i]->inspect();
                    ImGui::Unindent(8.0f);
                }

                ImGui::PopID();
            }

            if (removeIndex != -1)
                removeComponent(removeIndex);
        }

        /** @brief Returns the type of the entity for serialization (e.g., "Sprite", "Rectangle"). */
        virtual std::string getType() const = 0;

        /** @brief Serializes the base entity data and its components to JSON. */
        virtual nlohmann::json serialize() const
        {
            nlohmann::json j;
            j["type"] = getType();
            j["id"] = id;
            j["parentId"] = parent ? parent->id : -1;
            j["name"] = name;
            j["position"] = { position.x, position.y };
            j["scale"] = { scale.x, scale.y };
            j["rotation"] = rotation;
            j["sortingOrder"] = sortingOrder;
            j["color"] = { color.r, color.g, color.b, color.a };

            nlohmann::json comps = nlohmann::json::array();
            for (const auto& c : components)
            {
                comps.push_back(c->serialize());
            }
            j["components"] = comps;

            return j;
        }

        virtual void deserialize(const nlohmann::json& j)
        {
            if (j.contains("name")) name = j["name"].get<std::string>();
            if (j.contains("id")) id = j["id"].get<int>();
            if (j.contains("parentId")) parentId = j["parentId"].get<int>();
            if (j.contains("position"))
            {
                position.x = j["position"][0];
                position.y = j["position"][1];
            }
            if (j.contains("scale"))
            {
                scale.x = j["scale"][0];
                scale.y = j["scale"][1];
            }
            if (j.contains("rotation")) rotation = j["rotation"].get<float>();
            if (j.contains("sortingOrder")) sortingOrder = j["sortingOrder"].get<int>();
            if (j.contains("color"))
            {
                color.r = j["color"][0];
                color.g = j["color"][1];
                color.b = j["color"][2];
                color.a = j["color"][3];
            }
        }
    };
}
