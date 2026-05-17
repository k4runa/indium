#pragma once
#include "raylib.h"
#include "raymath.h"
#include "memory"
#include "vector"
#include "iostream"
#include "map"
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include "../Entity.hpp"
#include "../StoryState.hpp"
#include "../../include/nlohmann/json.hpp"
#include "../../2D/component/RigidbodyComponent.hpp"

namespace Indium

{
    /**
     * @brief A container representing a game level or simulation world.
     *
     * The Scene manages the lifecycle of all entities. It handles their updates,
     * rendering, and provides a snapshot system to switch between "Editor"
     * and "Play" modes without losing the original state.
     */
    struct Scene
    {
        /** @brief The list of active entities currently being updated and rendered. */
        std::vector<std::unique_ptr<Entity>> entities;

        /** @brief Queue for safely spawning entities during the update loop. */
        std::vector<std::unique_ptr<Entity>> startQueue;

        /** @brief IDs of entities scheduled for destruction at end of frame. Safe to call during Update. */
        std::vector<int> destroyQueue;

        static constexpr float FIXED_TIMESTEP  = 1.0f / 60.0f;
        static constexpr int   MAX_FIXED_STEPS = 5;
        float fixedAccumulator = 0.0f;

        /** @brief Counter for assigning unique IDs to entities within this scene. */
        int                                  nextEntityId = 1;

        /** @brief Helper map for tracking entity counts (e.g., for default naming: "Circle 1", "Circle 2"). */
        std::map<std::string, int>           entityCounts;

        /** @brief The simulation boundaries in world coordinates. */
        Vector2                              worldSize = { 1920, 1080 };

        /** @brief Editor-only: persisted so the viewport position is restored on scene reload. */
        Vector2                              editorCameraTarget = { 0, 0 };
        float                                editorCameraZoom   = 1.0f;

        /**
         * @brief Authored starting values for the story blackboard.
         *
         * These per-scene flags/variables are seeded into the global
         * StoryState singleton when Play begins (see StoryState::Seed).
         */
        std::map<std::string, StoryValue>    storyState;

        /**
         * @brief A temporary storage for the scene state.
         *
         * When the user presses "Play", the current state of 'entities' is cloned
         * into this snapshot. When they press "Stop", the engine restores
         * 'entities' from this snapshot, effectively resetting the simulation.
         */
        std::vector<std::unique_ptr<Entity>> snapshot;

        /**
         * @brief Iterates through all entities and calls their draw methods.
         *
         * This should be called within a Raylib BeginMode2D/EndMode2D block
         * or a BeginTextureMode block.
         */
        void Draw()
        {
            // Sort a temporary index array — never reorder the entities vector itself,
            // as the editor uses index-based selection that would silently break.
            std::vector<int> order(entities.size());
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(),
                [this](int a, int b) {
                    return entities[a]->computeSortKey() < entities[b]->computeSortKey();
                });

            for (int i : order)
            {
                if (!entities[i]->activeInHierarchy()) continue;
                entities[i]->draw();
                for (const auto& comp : entities[i]->components)
                    if (comp->enabled) comp->draw();
            }
        }

        /**
         * @brief Captures the current state of the world.
         *
         * Creates a deep copy of every entity and component. This allows the
         * physics or AI to modify the world during "Play" mode while preserving
         * the original layout.
         */
        void Save()
        {
            snapshot.clear();
            for (auto& e : entities)
            {
                snapshot.push_back(e->clone());
            }
        }

        /**
         * @brief Restores the world to a previously saved state.
         *
         * Clears all active entities and replaces them with clones from the snapshot.
         */
        void Restore()
        {
            // Give every live component a chance to clean up (unsubscribe events, release
            // handles, etc.) before the entities are destroyed. This mirrors the explicit
            // destroy() call made by DestroyEntity and ensures the full lifecycle is
            // respected on every Play→Stop transition.
            for (auto& e : entities)
                for (auto& c : e->components)
                    c->destroy(this);

            entities.clear();
            startQueue.clear();
            destroyQueue.clear();
            fixedAccumulator = 0.0f;
            tagIndex_.clear();
            for (auto& e : snapshot)
                entities.push_back(e->clone());
            RebuildHierarchy(); // also rebuilds tag index and hierarchy cache
        }

        /**
         * @brief Relinks parent/child raw pointers based on parentId values.
         * Must be called after deserializing or restoring from snapshot.
         */
        void RebuildHierarchy()
        {
            // First clear all children lists
            for (auto& e : entities)
            {
                e->parent = nullptr;
                e->children.clear();
            }

            // Link based on parentId
            for (auto& e : entities)
            {
                if (e->parentId != -1)
                {
                    Entity* p = FindEntity(e->parentId);
                    if (p)
                    {
                        e->parent = p;
                        p->children.push_back(e.get());
                    }
                    else
                    {
                        e->parentId = -1;
                    }
                }
            }

            // Rebuild the O(1) activeInHierarchy cache top-down
            for (auto& e : entities)
                if (!e->parent)
                    e->rebuildHierarchyCacheDown_();

            rebuildTagIndex_();
        }

        Entity* FindEntity(int id) const
        {
            for (const auto& e : entities)
                if (e->id == id) return e.get();
            return nullptr;
        }

        // ------------------------------------------------------------------
        // Query API
        // FindWith*/FindBy* return vectors (allocation, single use).
        // ForEach* accept a callback — zero allocation, best for hot paths.
        // Tag index is rebuilt once per Update() frame, so queries are O(k)
        // where k is the number of entities with that tag.
        // ------------------------------------------------------------------

        /** @brief First active entity with the given tag (index-backed, O(k)). */
        Entity* FindWithTag(const std::string& tag) const
        {
            auto it = tagIndex_.find(tag);
            if (it == tagIndex_.end()) return nullptr;
            for (Entity* e : it->second)
                if (e->activeInHierarchy()) return e;
            return nullptr;
        }

        /** @brief All entities with the given tag regardless of active state (index-backed). */
        std::vector<Entity*> FindAllWithTag(const std::string& tag) const
        {
            auto it = tagIndex_.find(tag);
            return it != tagIndex_.end() ? it->second : std::vector<Entity*>{};
        }

        /** @brief All entities on the given logical layer (O(n)). */
        std::vector<Entity*> FindByLayer(int layerIndex) const
        {
            std::vector<Entity*> result;
            for (const auto& e : entities)
                if (e->layer == layerIndex) result.push_back(e.get());
            return result;
        }

        /** @brief All entities currently active in the hierarchy (O(n)). */
        std::vector<Entity*> FindActiveEntities() const
        {
            std::vector<Entity*> result;
            for (const auto& e : entities)
                if (e->activeInHierarchy()) result.push_back(e.get());
            return result;
        }

        /** @brief Zero-allocation tag query — calls fn(Entity*) for each matching entity. */
        template<typename Fn>
        void ForEachWithTag(const std::string& tag, Fn&& fn) const
        {
            auto it = tagIndex_.find(tag);
            if (it != tagIndex_.end())
                for (Entity* e : it->second) fn(e);
        }

        /** @brief Zero-allocation layer query — calls fn(Entity*) for each entity on the layer. */
        template<typename Fn>
        void ForEachByLayer(int layerIndex, Fn&& fn) const
        {
            for (const auto& e : entities)
                if (e->layer == layerIndex) fn(e.get());
        }

        /** @brief Zero-allocation active query — calls fn(Entity*) for every active entity. */
        template<typename Fn>
        void ForEachActive(Fn&& fn) const
        {
            for (const auto& e : entities)
                if (e->activeInHierarchy()) fn(e.get());
        }

        /** @brief Schedules an entity (and its children) for destruction at the end of the frame. */
        void DestroyEntity(int id)
        {
            for (int existing : destroyQueue)
                if (existing == id) return;
            destroyQueue.push_back(id);
        }

        /**
         * @brief Serializes the current active entities to a JSON object.
         */
        nlohmann::json serialize() const
        {
            nlohmann::json j;
            j["worldSize"] = { worldSize.x, worldSize.y };
            j["editorCamera"] = { editorCameraTarget.x, editorCameraTarget.y, editorCameraZoom };

            nlohmann::json ents = nlohmann::json::array();
            for (const auto& e : entities)
            {
                ents.push_back(e->serialize());
            }
            j["entities"] = ents;
            j["nextEntityId"] = nextEntityId;
            j["storyState"] = StoryValueMapToJson(storyState);

            // Note: We don't serialize entityCounts or snapshots.
            return j;
        }

        /**
         * @brief Instantiates a new entity during runtime safely.
         * Places it in a queue to be initialized and added before the next update loop.
         */
        Entity* Instantiate(std::unique_ptr<Entity> entity)
        {
            Entity* ptr = entity.get();
            startQueue.push_back(std::move(entity));
            return ptr;
        }

        /**
         * @brief Triggers the update logic for the entire world.
         *
         * @param dt The time elapsed since the last frame (Delta Time).
         */
        void Update(float dt)
        {
            // 1. Process pending entities spawned during runtime
            if (!startQueue.empty())
            {
                std::vector<std::unique_ptr<Entity>> queueToProcess;
                std::swap(queueToProcess, startQueue);

                for (auto& e : queueToProcess)
                {
                    for (auto& c : e->components)
                    {
                        c->start(this);
                    }
                    entities.push_back(std::move(e));
                }
            }

            // 2. Fixed timestep loop — physics runs at a constant rate independent of frame rate
            fixedAccumulator += dt;
            int steps = 0;
            while (fixedAccumulator >= FIXED_TIMESTEP && steps < MAX_FIXED_STEPS)
            {
                for (auto& e : entities)
                    e->fixedUpdate(FIXED_TIMESTEP, worldSize, this);
                RigidbodyComponent::ResolveScene(this, FIXED_TIMESTEP);
                fixedAccumulator -= FIXED_TIMESTEP;
                ++steps;
            }
            // Clamp leftover accumulator to avoid a spiral of death on heavy frames
            if (fixedAccumulator > FIXED_TIMESTEP * MAX_FIXED_STEPS)
                fixedAccumulator = 0.0f;

            // 3. Update all active entities (variable rate — input, animation, camera, scripts)
            for (auto& e : entities)
            {
                e->update(dt, worldSize, this);
            }

            // 4. Flush destroy queue — safe to remove here, outside the iteration above
            if (!destroyQueue.empty())
            {
                // Collect IDs of every entity to remove, including their children recursively
                std::vector<int> toRemove;
                std::function<void(Entity*)> collectSubtree = [&](Entity* ent) {
                    toRemove.push_back(ent->id);
                    for (Entity* child : ent->children)
                        collectSubtree(child);
                };

                for (int id : destroyQueue)
                {
                    Entity* ent = FindEntity(id);
                    if (ent) collectSubtree(ent);
                }
                destroyQueue.clear();

                for (int removeId : toRemove)
                {
                    auto iter = std::find_if(entities.begin(), entities.end(),
                        [removeId](const std::unique_ptr<Entity>& e) { return e->id == removeId; });
                    if (iter == entities.end()) continue;

                    Entity* ent = iter->get();

                    // Notify components before the entity is destroyed (allows OnDestroy overrides)
                    for (auto& comp : ent->components)
                        comp->destroy(this);

                    if (ent->parent)
                    {
                        auto& sibs = ent->parent->children;
                        sibs.erase(std::remove(sibs.begin(), sibs.end(), ent), sibs.end());
                    }
                    entities.erase(iter);
                }
            }

            // 5. Rebuild tag index once per frame after all structural changes
            rebuildTagIndex_();
        }

    private:
        /** @brief Per-frame tag → entity map. Rebuilt at end of Update(). */
        std::unordered_map<std::string, std::vector<Entity*>> tagIndex_;

        void rebuildTagIndex_()
        {
            tagIndex_.clear();
            for (const auto& e : entities)
                tagIndex_[e->tag].push_back(e.get());
        }
    };
}
