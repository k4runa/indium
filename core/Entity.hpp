#pragma once
#include "raylib.h"
#include "raymath.h"
#include "iostream"
#include "memory"
#include "vector"
#include "Component.hpp"
#include "TagRegistry.hpp"
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
        std::vector<Entity*> children; // all childrens

        /** @brief Human-readable name displayed in the Editor Hierarchy. */
        std::string name;

        /** @brief Self-active flag. Use setActive() to change so components receive OnEnable/OnDisable. */
        bool        isActive = true;

        /** @brief Marks the entity as never-moving (hint for baking / collision optimisation). */
        bool        isStatic = false;

        /** @brief Gameplay tag for group queries (e.g. "Player", "Enemy"). */
        std::string tag = "Untagged";

        /** @brief Logical layer index (0-31) for scripting / collision filtering. */
        int         layer = 0;

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

        /** @brief Sorting order for draw priority. Used when depthMode is Manual. */
        int         sortingOrder = 0;

        // --- 2.5D Depth System ---

        enum class DepthMode { Manual, YSort };

        /** @brief Coarse layer index. Entities on different layers never interleave.
         *  Negative = background, 0 = world, positive = foreground. */
        int         depthLayer = 0;

        /** @brief How the draw order within a layer is determined. */
        DepthMode   depthMode = DepthMode::Manual;

        /** @brief World-space Y offset applied on top of position.y when computing
         *  the YSort key. Use to sort by a character's feet rather than center. */
        float       yPivotOffset = 0.0f;

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
              isActive(other.isActive), isStatic(other.isStatic),
              tag(other.tag), layer(other.layer),
              position(other.position), scale(other.scale),
              color(other.color), velocity(other.velocity), rotation(other.rotation),
              sortingOrder(other.sortingOrder),
              depthLayer(other.depthLayer), depthMode(other.depthMode),
              yPivotOffset(other.yPivotOffset)
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

        /**
         * @brief Signals to the Editor that a component removal is pending.
         * Set by inspect() when the user clicks "Remove Component".
         * The Editor reads and clears this before calling removeComponent,
         * so it can TakeSnapshot() first.
         */
        int pendingRemoveComponentIndex = -1;

        /**
         * @brief Returns true only when this entity AND every ancestor are active.
         * O(1) — backed by a cached value that's kept in sync by setActive/setParent.
         */
        [[nodiscard]] bool activeInHierarchy() const { return cachedHierarchyActive_; }

        /**
         * @brief Sets isActive and fires onEnable/onDisable through the full hierarchy.
         * Prefer this over writing isActive directly so lifecycle callbacks reach children.
         */
        void setActive(bool active)
        {
            if (isActive == active) return;
            bool wasHierActive = cachedHierarchyActive_;
            isActive = active;
            rebuildHierarchyCacheDown_();
            if (wasHierActive != cachedHierarchyActive_) propagateCallbacksDown_(cachedHierarchyActive_);
        }

        /**
         * @brief Reparents this entity, preserving lifecycle callbacks.
         * Handles unlinking from the old parent, linking to the new one, cache
         * rebuild, and onEnable/onDisable propagation if the effective active
         * state changes as a result.
         */
        void setParent(Entity* newParent)
        {
            if (parent == newParent) return;

            if (parent)
            {
                auto& sibs = parent->children;
                sibs.erase(std::remove(sibs.begin(), sibs.end(), this), sibs.end());
            }

            bool wasHierActive = cachedHierarchyActive_;
            parent   = newParent;
            parentId = newParent ? newParent->id : -1;
            if (newParent) newParent->children.push_back(this);

            rebuildHierarchyCacheDown_();
            if (wasHierActive != cachedHierarchyActive_) propagateCallbacksDown_(cachedHierarchyActive_);
        }

        /** @brief Returns the first attached component of type T, or nullptr if not found. */
        template<typename T>
        T* getComponent()
        {
            for (auto& comp : components)
            {
                if (T* result = dynamic_cast<T*>(comp.get())) return result;
            }
            return nullptr;
        }

        template<typename T>
        const T* getComponent() const
        {
            for (const auto& comp : components)
            {
                if (const T* result = dynamic_cast<const T*>(comp.get())) return result;
            }
            return nullptr;
        }

        /** @brief Returns the numeric key used to sort this entity in Scene::Draw().
         *
         *  Each layer occupies a fixed-size band so entities on different layers
         *  never interleave regardless of their Y position or sortingOrder value.
         *
         *  Manual:  key = layer * BAND + sortingOrder
         *  YSort:   key = layer * BAND + position.y + yPivotOffset
         */
        [[nodiscard]] float computeSortKey() const
        {
            constexpr float BAND = 1'000'000.0f;
            if (depthMode == DepthMode::YSort) { return depthLayer * BAND + position.y + yPivotOffset; }
            return depthLayer * BAND + static_cast<float>(sortingOrder);
        }

        /** @brief Returns the axis-aligned bounding box (AABB) of the entity in world space.
         *  Prefer attaching a BoxCollider2D / CircleCollider2D for precise bounds.
         *  This fallback uses position + scale. */
        virtual ::Rectangle getBounds() const
        {
            Vector2 gPos = getGlobalPosition();
            Vector2 gScl = getGlobalScale();
            return { gPos.x - gScl.x * 0.5f, gPos.y - gScl.y * 0.5f, gScl.x, gScl.y };
        }

        /** @brief Returns the 4 vertices of the entity in world space (for polygons/OBB). */
        virtual std::vector<Vector2> getVertices() const { return {}; }

        /** @brief Broad-phase collision check. Prefer using Collider2D components
         *  for physics; this fallback uses AABB bounds. */
        virtual bool collidesWith(Entity* other)
        {
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

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
            if (index >= 0 && index < (int)components.size()) { components.erase(components.begin() + index); }
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
                float c   = cosf(rad);
                float s   = sinf(rad);

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

        /** @brief Rendering is handled by ShapeRendererComponent / SpriteRendererComponent.
         *  This no-op base remains for backward compatibility. */
        virtual void draw() const {}

        /** @brief Updates the entity and triggers the update cycle for all attached components. */
        virtual void update(float dt, Vector2 worldSize, Scene* scene)
        {
            if (!activeInHierarchy()) return;
            for (auto& c : components) if (c->enabled) c->update(dt, worldSize, scene);
        }

        /** @brief Runs fixed-rate physics/logic for all attached components. */
        virtual void fixedUpdate(float fixedDt, Vector2 worldSize, Scene* scene)
        {
            if (!activeInHierarchy()) return;
            for (auto& c : components) if (c->enabled) c->fixedUpdate(fixedDt, worldSize, scene);
        }

        /** @brief Runs after all entities have updated this frame. */
        virtual void lateUpdate(float dt, Vector2 worldSize, Scene* scene)
        {
            if (!activeInHierarchy()) return;
            for (auto& c : components) if (c->enabled) c->lateUpdate(dt, worldSize, scene);
        }

        /** @brief Checks if a world-space point is contained within the entity's bounds.
         *  Subclasses may override for precise shape-specific hit testing. */
        virtual bool Contains(Vector2 point) const
        {
            return CheckCollisionPointRec(point, getBounds());
        }

        /** @brief Returns a unique_ptr to a new Entity that is an exact copy of this one. */
        virtual std::unique_ptr<Entity> clone()
        {
            return std::make_unique<Entity>(*this);
        }

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
        virtual void inspect(std::function<void()> snapshotCb = {})
        {
            // --- Compact Entity Header ---

            // Row 1: [Active] [___Name___________] [Static]
            bool activeRef = isActive;
            if (ImGui::Checkbox("##Active", &activeRef))
            {
                if (snapshotCb) snapshotCb();
                setActive(activeRef);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Active");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 72.0f);
            char buf[64];
            strncpy(buf, name.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("##Name", buf, sizeof(buf))) name = buf;
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::SameLine();
            ImGui::Checkbox("Static", &isStatic);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            // Row 2: Tag [▼]   Layer [  ]
            {
                const auto& registryTags = TagRegistry::Get().GetTags();
                std::vector<const char*> tagPtrs;
                tagPtrs.reserve(registryTags.size());
                for (const auto& t : registryTags) tagPtrs.push_back(t.c_str());
                int tagIdx = 0;
                for (int t = 0; t < (int)tagPtrs.size(); ++t)
                {
                    if (tag == tagPtrs[t]) { tagIdx = t; break; }
                }
                ImGui::TextUnformatted("Tag");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.44f);
                if (ImGui::Combo("##Tag", &tagIdx, tagPtrs.data(), (int)tagPtrs.size()))
                {
                    if (snapshotCb) snapshotCb();
                    tag = tagPtrs[tagIdx];
                }
            }
            ImGui::SameLine();
            ImGui::TextUnformatted("Layer");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            ImGui::DragInt("##Layer", &layer, 1, 0, 31);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            // Type badge
            ImGui::Spacing();
            ImGui::TextDisabled("[%s]", getType().c_str());

            ImGui::Spacing();

            // --- Transform Section ---
            if (ImGui::CollapsingHeader("Transform"))
            {
                ImGui::Indent(8.0f);

                ImGui::Text("Position");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat2("##Position", &position.x, 1.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();

                ImGui::Text("Rotation");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat("##Rotation", &rotation, 1.0f, -360.0f, 360.0f, "%.1f\xC2\xB0");
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();

                ImGui::Text("Scale");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat2("##Scale", &scale.x, 0.5f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();

                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Text("Depth Layer");
                ImGui::PushItemWidth(-1);
                ImGui::DragInt("##DepthLayer", &depthLayer, 1);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();

                ImGui::Text("Depth Mode");
                ImGui::PushItemWidth(-1);
                {
                    const char* modes[] = { "Manual", "Y-Sort" };
                    int modeIdx = static_cast<int>(depthMode);
                    if (ImGui::Combo("##DepthMode", &modeIdx, modes, 2))
                    {
                        if (snapshotCb) snapshotCb();
                        depthMode = static_cast<DepthMode>(modeIdx);
                    }
                }
                ImGui::PopItemWidth();

                if (depthMode == DepthMode::Manual)
                {
                    ImGui::Text("Sorting Order");
                    ImGui::PushItemWidth(-1);
                    ImGui::DragInt("##SortingOrder", &sortingOrder, 1);
                    if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                    ImGui::PopItemWidth();
                }
                else
                {
                    ImGui::Text("Y Pivot Offset");
                    ImGui::PushItemWidth(-1);
                    ImGui::DragFloat("##YPivotOffset", &yPivotOffset, 1.0f, 0.0f, 0.0f, "%.1f px");
                    if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                    ImGui::PopItemWidth();
                }

                ImGui::Unindent(8.0f);
            }

            // --- Material Section ---
            if (ImGui::CollapsingHeader("Material"))
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
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

                ImGui::Unindent(8.0f);
            }

            // --- Components Section ---
            int removeIndex = -1;
            for (int i = 0; i < (int)components.size(); i++)
            {
                ImGui::PushID(components[i].get());

                bool open = ImGui::CollapsingHeader(components[i]->getName().c_str(), ImGuiTreeNodeFlags_AllowOverlap);

                if (ImGui::BeginPopupContextItem("comp_ctx"))
                {
                    if (ImGui::MenuItem("Remove Component")) removeIndex = i;
                    ImGui::EndPopup();
                }

                // Enabled checkbox overlaid on the right side of the header
                ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight() + 10.0f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.5f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
                bool compEnabled = components[i]->enabled;
                if (ImGui::Checkbox("##ce", &compEnabled)) { components[i]->setEnabled(compEnabled); }
                ImGui::PopStyleVar();
                if (open)
                {
                    ImGui::Indent(8.0f);
                    components[i]->inspect(snapshotCb);
                    ImGui::Unindent(8.0f);
                }

                ImGui::PopID();
            }

            if (removeIndex != -1) pendingRemoveComponentIndex = removeIndex;
        }

        /** @brief Returns the type of the entity for serialization (e.g., "Sprite", "Rectangle"). */
        virtual std::string getType() const { return "Entity"; }

        /** @brief Serializes the base entity data and its components to JSON. */
        virtual nlohmann::json serialize() const
        {
            nlohmann::json j;
            j["type"]         = getType();
            j["id"]           = id;
            j["parentId"]     = parent ? parent->id : -1;
            j["name"]         = name;
            j["isActive"]     = isActive;
            j["isStatic"]     = isStatic;
            j["tag"]          = tag;
            j["layer"]        = layer;
            j["position"]     = { position.x, position.y };
            j["scale"]        = { scale.x, scale.y };
            j["rotation"]     = rotation;
            j["sortingOrder"] = sortingOrder;
            j["depthLayer"]   = depthLayer;
            j["depthMode"]    = static_cast<int>(depthMode);
            j["yPivotOffset"] = yPivotOffset;
            j["color"]        = { color.r, color.g, color.b, color.a };

            nlohmann::json comps = nlohmann::json::array();
            for (const auto& c : components) { comps.push_back(c->serialize()); }
            j["components"] = comps;

            return j;
        }

        virtual void deserialize(const nlohmann::json& j)
        {
            if (j.contains("name"))     name     = j["name"].get<std::string>();
            if (j.contains("id"))       id       = j["id"].get<int>();
            if (j.contains("parentId")) parentId = j["parentId"].get<int>();
            if (j.contains("isActive")) isActive = j["isActive"].get<bool>();
            if (j.contains("isStatic")) isStatic = j["isStatic"].get<bool>();
            if (j.contains("tag"))      tag      = j["tag"].get<std::string>();
            if (j.contains("layer"))    layer    = j["layer"].get<int>();
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
            if (j.contains("rotation"))      rotation     = j["rotation"].get<float>();
            if (j.contains("sortingOrder"))  sortingOrder = j["sortingOrder"].get<int>();
            if (j.contains("depthLayer"))    depthLayer   = j["depthLayer"].get<int>();
            if (j.contains("depthMode"))     depthMode    = static_cast<DepthMode>(j["depthMode"].get<int>());
            if (j.contains("yPivotOffset"))  yPivotOffset = j["yPivotOffset"].get<float>();
            if (j.contains("color"))
            {
                color.r = j["color"][0];
                color.g = j["color"][1];
                color.b = j["color"][2];
                color.a = j["color"][3];
            }
        }

    private:
        /** @brief Cached result of the full ancestor-chain active check. Updated by setActive/setParent/RebuildHierarchy. */
        bool cachedHierarchyActive_ = true;

        /** @brief Rebuilds cachedHierarchyActive_ for this node and all descendants. */
        void rebuildHierarchyCacheDown_()
        {
            cachedHierarchyActive_ = isActive && (parent ? parent->cachedHierarchyActive_ : true);
            for (Entity* child : children) child->rebuildHierarchyCacheDown_();
        }

        /**
         * @brief Fires onEnable/onDisable on this node's components (parent-first),
         * then recurses into children whose own isActive flag is true.
         * Children that are self-inactive don't propagate — they were already dormant.
         */
        void propagateCallbacksDown_(bool enabling)
        {
            for (auto& c : components) enabling ? c->onEnable() : c->onDisable();
            for (Entity* child : children) if (child->isActive) { child->propagateCallbacksDown_(enabling); }
        }

        // Scene needs to rebuild the cache after deserialization / Restore.
        friend struct Scene;
    };
}
