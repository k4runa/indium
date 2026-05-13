#pragma once
#include "raylib.h"
#include "raymath.h"
#include "memory"
#include "vector"
#include "iostream"
#include "map"
#include "../Entity/Entity.hpp"

namespace Indium
{
    /**
     * @brief Represents a game level/scene containing entities.
     */
    struct Scene
    {
        std::vector<std::unique_ptr<Entity>> entities;
        std::map<std::string, int>           entityCounts;

        // Snapshot for Play/Stop functionality
        std::vector<std::unique_ptr<Entity>> snapshot;

        /** @brief Draw all entities in the scene */
        void Draw()
        {
            for (auto& e : entities)
            {
                e->draw();
            }
        }

        /** @brief Save the current state to the snapshot vector */
        void Save()
        {
            snapshot.clear();
            for (auto& e : entities)
            {
                snapshot.push_back(e->clone());
            }
        }

        /** @brief Restore the state from the snapshot vector */
        void Restore()
        {
            entities.clear();
            for (auto& e : snapshot)
            {
                entities.push_back(e->clone());
            }
        }

        /** @brief Update all entities in the scene */
        void Update(float dt)
        {
            for (auto& e : entities)
            {
                e->update(dt);
            }
        }
    };
}
