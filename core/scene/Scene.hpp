#pragma once
#include "raylib.h"
#include "raymath.h"
#include "memory"
#include "vector"
#include "iostream"
#include "map"
#include <algorithm>
#include <numeric>
#include <set>
#include <utility>
#include <unordered_map>
#include "../Entity.hpp"
#include "../StoryState.hpp"
#include "../Time.hpp"
#include "../../include/nlohmann/json.hpp"
#include "../../2D/component/RigidbodyComponent.hpp"
#include "../../2D/component/Collider2D.hpp"

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

        /** @brief Active rigidbody collision pairs this physics step. Managed by RigidbodyComponent::ResolveScene. */
        std::set<std::pair<int,int>>         _activeCollisionPairs;

        /** @brief Scene name to load at end of frame. Set by NativeScript::LoadScene(), consumed by Editor. */
        std::string                          _pendingSceneLoad;

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
            // Reuse the member buffer to avoid per-frame heap allocation.
            // Never reorder entities itself — the editor uses index-based selection.
            drawOrder_.resize(entities.size());
            std::iota(drawOrder_.begin(), drawOrder_.end(), 0);
            std::sort(drawOrder_.begin(), drawOrder_.end(),
                [this](int a, int b) {
                    return entities[a]->computeSortKey() < entities[b]->computeSortKey();
                });

            for (int i : drawOrder_)
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
                snapshot.push_back(e->clone());
            snapshotNextEntityId_ = nextEntityId;
            snapshotWorldSize_    = worldSize;
            snapshotEntityCounts_ = entityCounts;
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
            _activeCollisionPairs.clear();
            _pendingSceneLoad.clear();
            Time::elapsed = 0.0f;
            tagIndex_.clear();
            nextEntityId  = snapshotNextEntityId_;
            worldSize     = snapshotWorldSize_;
            entityCounts  = snapshotEntityCounts_;
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
            ensureTagIndex_();
            auto it = tagIndex_.find(tag);
            if (it == tagIndex_.end()) return nullptr;
            for (Entity* e : it->second)
                if (e->activeInHierarchy()) return e;
            return nullptr;
        }

        /** @brief All entities with the given tag regardless of active state (index-backed). */
        std::vector<Entity*> FindAllWithTag(const std::string& tag) const
        {
            ensureTagIndex_();
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
            ensureTagIndex_();
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

        // ------------------------------------------------------------------
        // Physics2D Query API
        // ------------------------------------------------------------------

        struct RaycastHit2D
        {
            Entity* entity   = nullptr;
            Vector2 point    = {0, 0};
            Vector2 normal   = {0, 1};
            float   distance = 0.0f;
            explicit operator bool() const { return entity != nullptr; }
        };

        /** @brief Casts a ray and returns the closest entity hit.
         *  @param origin     World-space start point.
         *  @param direction  Direction vector (does not need to be normalized).
         *  @param maxDist    Maximum distance along the ray to test.
         *  @param layerMask  Bitmask of layers to include (-1 = all layers).
         */
        RaycastHit2D Raycast(Vector2 origin, Vector2 direction, float maxDist, int layerMask = -1) const
        {
            Vector2 dir = Vector2Normalize(direction);
            RaycastHit2D best;
            float bestDist = maxDist;

            for (const auto& e : entities)
            {
                if (!e->activeInHierarchy()) continue;
                if (layerMask != -1 && !(layerMask & (1 << e->layer))) continue;

                auto* col = e->getComponent<Collider2D>();
                Vector2 gPos = e->getGlobalPosition();
                float t = -1.0f;
                Vector2 hitNormal = {0, 1};

                if (col && col->isCircleShape())
                {
                    float r = col->getCircleRadius();
                    Vector2 d = Vector2Subtract(origin, gPos);
                    float b = Vector2DotProduct(d, dir);
                    float c = Vector2DotProduct(d, d) - r * r;
                    float disc = b * b - c;
                    if (disc >= 0.0f)
                    {
                        t = -b - sqrtf(disc);
                        if (t < 0.0f) t = -b + sqrtf(disc);
                        if (t >= 0.0f)
                        {
                            Vector2 hp = { origin.x + dir.x * t, origin.y + dir.y * t };
                            hitNormal  = Vector2Normalize(Vector2Subtract(hp, gPos));
                        }
                        else t = -1.0f;
                    }
                }
                else
                {
                    ::Rectangle b = col ? col->getBounds() : e->getBounds();
                    float tmin = 0.0f, tmax = maxDist;
                    if (fabsf(dir.x) > 1e-6f)
                    {
                        float t1 = (b.x - origin.x) / dir.x;
                        float t2 = (b.x + b.width - origin.x) / dir.x;
                        if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
                        tmin = fmaxf(tmin, t1);
                        tmax = fminf(tmax, t2);
                    }
                    else if (origin.x < b.x || origin.x > b.x + b.width) continue;
                    if (fabsf(dir.y) > 1e-6f)
                    {
                        float t1 = (b.y - origin.y) / dir.y;
                        float t2 = (b.y + b.height - origin.y) / dir.y;
                        if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
                        tmin = fmaxf(tmin, t1);
                        tmax = fminf(tmax, t2);
                    }
                    else if (origin.y < b.y || origin.y > b.y + b.height) continue;
                    if (tmin <= tmax) { t = tmin; hitNormal = {0, -1}; }
                }

                if (t >= 0.0f && t < bestDist)
                {
                    bestDist       = t;
                    best.entity    = e.get();
                    best.distance  = t;
                    best.point     = { origin.x + dir.x * t, origin.y + dir.y * t };
                    best.normal    = hitNormal;
                }
            }
            return best;
        }

        /** @brief Returns all active entities whose collider overlaps a circle. */
        std::vector<Entity*> OverlapCircle(Vector2 center, float radius, int layerMask = -1) const
        {
            std::vector<Entity*> result;
            for (const auto& e : entities)
            {
                if (!e->activeInHierarchy()) continue;
                if (layerMask != -1 && !(layerMask & (1 << e->layer))) continue;
                auto* col = e->getComponent<Collider2D>();
                if (col && col->isCircleShape())
                {
                    float dist = Vector2Distance(center, e->getGlobalPosition());
                    if (dist <= radius + col->getCircleRadius()) result.push_back(e.get());
                }
                else
                {
                    ::Rectangle b = col ? col->getBounds() : e->getBounds();
                    if (CheckCollisionCircleRec(center, radius, b)) result.push_back(e.get());
                }
            }
            return result;
        }

        /** @brief Returns all active entities whose collider overlaps a box. */
        std::vector<Entity*> OverlapBox(Vector2 center, Vector2 size, int layerMask = -1) const
        {
            ::Rectangle query = { center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y };
            std::vector<Entity*> result;
            for (const auto& e : entities)
            {
                if (!e->activeInHierarchy()) continue;
                if (layerMask != -1 && !(layerMask & (1 << e->layer))) continue;
                auto* col = e->getComponent<Collider2D>();
                if (col && col->isCircleShape())
                {
                    if (CheckCollisionCircleRec(e->getGlobalPosition(), col->getCircleRadius(), query))
                        result.push_back(e.get());
                }
                else
                {
                    ::Rectangle b = col ? col->getBounds() : e->getBounds();
                    if (CheckCollisionRecs(query, b)) result.push_back(e.get());
                }
            }
            return result;
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
            float scaledDt  = dt * Time::scale;
            Time::delta     = scaledDt;
            Time::elapsed  += scaledDt;

            // 1. Process pending entities spawned during runtime
            if (!startQueue.empty())
            {
                std::vector<std::unique_ptr<Entity>> queueToProcess;
                std::swap(queueToProcess, startQueue);

                for (auto& e : queueToProcess)
                {
                    for (auto& c : e->components)
                        c->awake(this);
                    for (auto& c : e->components)
                        c->start(this);
                    entities.push_back(std::move(e));
                }
                tagIndexDirty_ = true;
            }

            // 2. Fixed timestep loop — accumulator is scaled so physics slows with Time::scale.
            //    FIXED_TIMESTEP itself stays constant for numerical stability.
            fixedAccumulator += scaledDt;
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
                e->update(scaledDt, worldSize, this);
            }

            // 3.5. Late update pass (camera follow, IK, post-update corrections)
            for (auto& e : entities)
            {
                e->lateUpdate(scaledDt, worldSize, this);
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
                tagIndexDirty_ = true;
            }
        }

    private:
        /** @brief Tag → entity map. Rebuilt lazily on first query after a structural change. */
        mutable std::unordered_map<std::string, std::vector<Entity*>> tagIndex_;

        /** @brief Set to true whenever entities are added or removed; cleared after rebuild. */
        mutable bool tagIndexDirty_ = true;

        /** @brief Reused draw-order index buffer — avoids per-frame heap allocation. */
        std::vector<int> drawOrder_;

        /** @brief Scalar fields captured by Save() and restored by Restore(). */
        int                        snapshotNextEntityId_ = 1;
        Vector2                    snapshotWorldSize_    = { 1920, 1080 };
        std::map<std::string, int> snapshotEntityCounts_;

        void ensureTagIndex_() const
        {
            if (!tagIndexDirty_) return;
            tagIndex_.clear();
            for (const auto& e : entities)
                tagIndex_[e->tag].push_back(e.get());
            tagIndexDirty_ = false;
        }

        void rebuildTagIndex_()
        {
            tagIndex_.clear();
            for (const auto& e : entities)
                tagIndex_[e->tag].push_back(e.get());
            tagIndexDirty_ = false;
        }
    };
}
