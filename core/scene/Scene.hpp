#pragma once
#include "raylib.h"
#include "raymath.h"
#include "memory"
#include "vector"
#include "iostream"
#include "map"
#include <algorithm>
#include <numeric>
#include "../Entity.hpp"
#include "../../include/nlohmann/json.hpp"

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

        /** @brief IDs of entities scheduled for destruction at end of frame. */
        std::vector<int> destroyQueue;

        /** @brief Counter for assigning unique IDs to entities within this scene. */
        int                                  nextEntityId = 1;

        /** @brief Helper map for tracking entity counts (e.g., for default naming: "Circle 1", "Circle 2"). */
        std::map<std::string, int>           entityCounts;

        /** @brief The simulation boundaries in world coordinates. */
        Vector2                              worldSize = { 1920, 1080 };

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
                entities[i]->draw();
                for (const auto& comp : entities[i]->components)
                    comp->draw();
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
            entities.clear();
            startQueue.clear(); // Clear any pending runtime objects
            for (auto& e : snapshot)
            {
                entities.push_back(e->clone());
            }
            RebuildHierarchy();
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
                        // Parent missing, become root
                        e->parentId = -1;
                    }
                }
            }
        }

        Entity* FindEntity(int id) const
        {
            for (const auto& e : entities)
            {
                if (e->id == id) return e.get();
            }
            return nullptr;
        }

        /** @brief Schedules an entity for destruction at the end of the current frame. Safe to call during update. */
        void DestroyEntity(int id)
        {
            destroyQueue.push_back(id);
        }

        /**
         * @brief Serializes the current active entities to a JSON object.
         */
        nlohmann::json serialize() const
        {
            nlohmann::json j;
            j["worldSize"] = { worldSize.x, worldSize.y };

            nlohmann::json ents = nlohmann::json::array();
            for (const auto& e : entities)
            {
                ents.push_back(e->serialize());
            }
            j["entities"] = ents;
            j["nextEntityId"] = nextEntityId;

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
                        c->start();
                    }
                    entities.push_back(std::move(e));
                }
            }

            // 2. Update all active entities
            for (auto& e : entities)
            {
                e->update(dt, worldSize, this);
            }

            // 3. Flush destroy queue — safe to remove here, outside the iteration above
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
                    if (ent->parent)
                    {
                        auto& sibs = ent->parent->children;
                        sibs.erase(std::remove(sibs.begin(), sibs.end(), ent), sibs.end());
                    }
                    entities.erase(iter);
                }
            }
        }
    };
}
