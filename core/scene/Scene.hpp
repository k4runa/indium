#pragma once
#include "raylib.h"
#include "raymath.h"
#include "memory"
#include "vector"
#include "iostream"
#include "map"
#include "../Entity.hpp"

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
            for (auto& e : entities)
            {
                e->draw();
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
            for (auto& e : snapshot)
            {
                entities.push_back(e->clone());
            }
        }

        /**
         * @brief Triggers the update logic for the entire world.
         *
         * @param dt The time elapsed since the last frame (Delta Time).
         */
        void Update(float dt)
        {
            for (auto& e : entities)
            {
                e->update(dt, worldSize, this);
            }
        }
    };
}
