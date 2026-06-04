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
#include "../spatial/SpatialGrid.hpp"

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

        /** @brief Filename stem of this scene (e.g. "level2"), without path or extension.
         *  Set by ProjectManager on every load/switch/create; not serialized (the file
         *  name is the source of truth). Used by SaveManager to record which scene a
         *  save slot belongs to so Load can return the player to it. */
        std::string                          name;

        /** @brief The simulation boundaries in world coordinates. */
        Vector2                              worldSize = { 1920, 1080 };

        // --- 2D Lighting ---

        /** @brief Master switch. When false, the world renders at full brightness and the
         *  Editor skips the light pass entirely (zero overhead). */
        bool                                 lightingEnabled = false;

        /** @brief Base illumination multiplied over the unlit scene. White = no darkening;
         *  a dark value (e.g. {40,40,55}) creates "night" that Light2DComponents punch through.
         *  Alpha is ignored. */
        Color                                ambientLight = { 40, 40, 55, 255 };

        /** @brief Editor-only: persisted so the viewport position is restored on scene reload. */
        Vector2                              editorCameraTarget = { 0, 0 };
        float                                editorCameraZoom   = 1.0f;

        /** @brief Active rigidbody collision pairs this physics step. Managed by RigidbodyComponent::ResolveScene. */
        std::set<std::pair<int,int>>         _activeCollisionPairs;

        /** @brief Scene name to load at end of frame. Set by NativeScript::LoadScene(), consumed by Editor. */
        std::string                          _pendingSceneLoad;

        /**
         * @brief Saved-game restore queued by SaveManager::Load, applied inside the
         * runtime ProjectManager::SwitchScene once the target scene is loaded.
         *
         * Restore must land AFTER the scene seeds its authored StoryState but BEFORE
         * component awake()/start(), so a script's OnStart observes the saved flags and
         * player position rather than the scene's authored defaults. SaveManager sets
         * these alongside _pendingSceneLoad; SwitchScene consumes and clears them.
         */
        bool                                 _hasPendingRestore = false;
        std::map<std::string, StoryValue>    _pendingStoryRestore;
        std::vector<std::pair<int, Vector2>> _pendingPositionRestore; // (entity id, local position)

        /**
         * @brief Authored starting values for the story blackboard.
         *
         * These per-scene flags/variables are seeded into the global
         * StoryState singleton when Play begins (see StoryState::Seed).
         */
        std::map<std::string, StoryValue>    storyState;

        // --- 2.5D Parallax (per depthLayer scroll-rate multiplier) ---

        /** @brief Master switch. When false, every layer renders at factor 1.0
         *  and Draw() takes the single-pass fast path — identical to pre-parallax
         *  behavior. */
        bool                                 parallaxEnabled = false;

        /** @brief Per-layer overrides. Layers absent from this map fall back to
         *  DefaultParallaxFactor(). Layer 0 is always 1.0 regardless of override. */
        std::map<int, float>                 parallaxByLayer;

        /** @brief World point where every layer coincides with its authored position.
         *  Parallax only diverges as the camera moves AWAY from this anchor, so it
         *  should be the camera's resting/starting view (e.g. the primary camera's
         *  authored position). Default {0,0} suits levels built from the world origin. */
        Vector2                              parallaxAnchor = { 0.0f, 0.0f };

        /** @brief Formula used for layers without an explicit override. Asymmetric
         *  by design: backgrounds compress (many layers fit between you and the
         *  horizon), foregrounds spread fast. */
        static float DefaultParallaxFactor(int depthLayer)
        {
            if (depthLayer == 0) return 1.0f;
            if (depthLayer  < 0) return std::max(0.0f, 1.0f + 0.25f * (float)depthLayer);
            return 1.0f + 0.5f * (float)depthLayer;
        }

        /** @brief Effective parallax factor for a layer.
         *  Off when parallaxEnabled is false. Layer 0 is locked at 1.0. */
        [[nodiscard]] float ParallaxFactor(int depthLayer) const
        {
            if (!parallaxEnabled || depthLayer == 0) return 1.0f;
            auto it = parallaxByLayer.find(depthLayer);
            if (it != parallaxByLayer.end()) return it->second;
            return DefaultParallaxFactor(depthLayer);
        }

        /** @brief True if this layer has an explicit override (vs. the formula). */
        [[nodiscard]] bool HasParallaxOverride(int depthLayer) const
        {
            return parallaxByLayer.find(depthLayer) != parallaxByLayer.end();
        }

        /** @brief Writes an override for a layer. Layer 0 is ignored (always 1.0). */
        void SetParallaxFactor(int depthLayer, float factor)
        {
            if (depthLayer == 0) return;
            parallaxByLayer[depthLayer] = factor;
        }

        /** @brief Removes any override for the layer (it falls back to the formula). */
        void ResetParallaxFactor(int depthLayer) { parallaxByLayer.erase(depthLayer); }

        /** @brief Enable / disable the master switch at runtime. */
        void SetParallaxEnabled(bool e) { parallaxEnabled = e; }

        /** @brief Sets the world point where all layers coincide (see parallaxAnchor). */
        void SetParallaxAnchor(Vector2 anchor) { parallaxAnchor = anchor; }

        /**
         * @brief Loads parallax config from a scene JSON, resetting to defaults for
         * any absent key. Always resets first so switching from a parallax scene to a
         * plain one (whose JSON omits the block) doesn't inherit stale settings.
         *
         * Shared by deserialize() and the editor's Undo/Redo so all three stay in sync.
         */
        void LoadParallaxFromJson(const nlohmann::json& j)
        {
            parallaxEnabled = j.value("parallaxEnabled", false);

            parallaxAnchor = { 0.0f, 0.0f };
            if (j.contains("parallaxAnchor") && j["parallaxAnchor"].is_array() && j["parallaxAnchor"].size() == 2)
            {
                parallaxAnchor.x = j["parallaxAnchor"][0].get<float>();
                parallaxAnchor.y = j["parallaxAnchor"][1].get<float>();
            }

            parallaxByLayer.clear();
            if (j.contains("parallaxByLayer") && j["parallaxByLayer"].is_object())
            {
                for (auto it = j["parallaxByLayer"].begin(); it != j["parallaxByLayer"].end(); ++it)
                {
                    try { parallaxByLayer[std::stoi(it.key())] = it.value().get<float>(); }
                    catch (...) { /* skip malformed key */ }
                }
            }
            parallaxByLayer.erase(0); // layer 0 is always 1.0; never store an override for it
        }

        /**
         * @brief Maps a base camera target to a layer's parallax-shifted target.
         *
         * Single source of truth for the parallax transform, shared by Scene::Draw
         * and the editor's gizmo overlays so they can never drift. The layer's
         * camera target is anchored: layers coincide at `parallaxAnchor` and diverge
         * proportionally to ParallaxFactor as the camera moves away from it.
         *
         *   layer_target = anchor + (cameraTarget - anchor) * factor
         *
         * For layer 0 or when parallax is disabled the factor is 1.0, so this
         * returns cameraTarget unchanged.
         */
        [[nodiscard]] Vector2 ApplyParallax(Vector2 cameraTarget, int depthLayer) const
        {
            const float f = ParallaxFactor(depthLayer);
            return { parallaxAnchor.x + (cameraTarget.x - parallaxAnchor.x) * f,
                     parallaxAnchor.y + (cameraTarget.y - parallaxAnchor.y) * f };
        }

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
         * Owns its own BeginMode2D/EndMode2D scopes. When parallax is active
         * (both `applyParallax` and `parallaxEnabled`), the sorted entity list
         * is rendered in per-depthLayer batches with the camera's `target`
         * scaled by ParallaxFactor(layer) — sort order is preserved because
         * depthLayer is already the coarsest sort key.
         *
         * When parallax is off, takes the single-pass fast path (identical to
         * the pre-parallax behavior).
         *
         * @param cam            The base scene camera (editor camera or game camera).
         * @param applyParallax  Editor passes false to keep the Scene tab WYSIWYG;
         *                       Play/Pause/Game tab always pass true.
         */
        void Draw(const Camera2D& cam, bool applyParallax = true)
        {
            // Reuse the member buffer to avoid per-frame heap allocation.
            // Never reorder entities itself — the editor uses index-based selection.
            drawOrder_.resize(entities.size());
            std::iota(drawOrder_.begin(), drawOrder_.end(), 0);
            std::sort(drawOrder_.begin(), drawOrder_.end(),
            [this](int a, int b) 
            {
                return entities[a]->computeSortKey() < entities[b]->computeSortKey();
            });

            const bool useParallax = applyParallax && parallaxEnabled;

            // Fast path: one camera scope, all entities. Identical to pre-parallax.
            if (!useParallax)
            {
                BeginMode2D(cam);
                for (int i : drawOrder_)
                {
                    if (!entities[i]->activeInHierarchy()) continue;
                    entities[i]->draw();
                    for (const auto& comp : entities[i]->components) if (comp->enabled) comp->draw();
                }
                EndMode2D();
                return;
            }

            // Parallax path: walk the sorted list in depthLayer batches, swapping
            // the camera between batches. Within a batch sort order is intact.
            const int N = (int)drawOrder_.size();
            int i = 0;
            while (i < N)
            {
                // Skip any inactive entities at the front of the current batch
                while (i < N && !entities[drawOrder_[i]]->activeInHierarchy()) ++i;
                if (i >= N) break;

                const int batchLayer = entities[drawOrder_[i]]->depthLayer;
                Camera2D layerCam = cam;
                layerCam.target   = ApplyParallax(cam.target, batchLayer);

                BeginMode2D(layerCam);
                while (i < N)
                {
                    Entity* e = entities[drawOrder_[i]].get();
                    if (!e->activeInHierarchy()) { ++i; continue; }
                    if (e->depthLayer != batchLayer) break; // next batch handled outside
                    e->draw();
                    for (const auto& comp : e->components) if (comp->enabled) comp->draw();
                    ++i;
                }
                EndMode2D();
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
            for (auto& e : entities) snapshot.push_back(e->clone());
            snapshotNextEntityId_     = nextEntityId;
            snapshotWorldSize_        = worldSize;
            snapshotEntityCounts_     = entityCounts;
            snapshotParallaxEnabled_  = parallaxEnabled;
            snapshotParallaxByLayer_  = parallaxByLayer;
            snapshotParallaxAnchor_   = parallaxAnchor;
            snapshotLightingEnabled_  = lightingEnabled;
            snapshotAmbientLight_     = ambientLight;
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
            for (auto& e : entities) for (auto& c : e->components) c->destroy(this);

            entities.clear();
            startQueue.clear();
            destroyQueue.clear();
            fixedAccumulator = 0.0f;
            _activeCollisionPairs.clear();
            _pendingSceneLoad.clear();
            _hasPendingRestore = false;
            _pendingStoryRestore.clear();
            _pendingPositionRestore.clear();
            Time::elapsed = 0.0f;
            tagIndex_.clear();
            nextEntityId    = snapshotNextEntityId_;
            worldSize       = snapshotWorldSize_;
            entityCounts    = snapshotEntityCounts_;
            parallaxEnabled = snapshotParallaxEnabled_;
            parallaxByLayer = snapshotParallaxByLayer_;
            parallaxAnchor  = snapshotParallaxAnchor_;
            lightingEnabled = snapshotLightingEnabled_;
            ambientLight    = snapshotAmbientLight_;
            for (auto& e : snapshot) entities.push_back(e->clone());
            RebuildHierarchy(); // also rebuilds tag index and hierarchy cache
        }

        /**
         * @brief Relinks parent/child raw pointers based on parentId values.
         * Must be called after deserializing or restoring from snapshot.
         */
        void RebuildHierarchy()
        {
            // First clear all children lists
            for (auto& e : entities) { e->parent = nullptr; e->children.clear(); }

            // Link based on parentId
            for (auto& e : entities)
            {
                if (e->parentId != -1)
                {
                    Entity* p = FindEntity(e->parentId);
                    if (p) { e->parent = p; p->children.push_back(e.get()); }
                    else   { e->parentId = -1; }
                }
            }

            // Rebuild the O(1) activeInHierarchy cache top-down
            for (auto& e : entities) if (!e->parent) e->rebuildHierarchyCacheDown_();
            rebuildTagIndex_();
        }

        Entity* FindEntity(int id) const
        {
            for (const auto& e : entities) if (e->id == id) return e.get();
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
            for (Entity* e : it->second) if (e->activeInHierarchy()) return e;
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
            for (const auto& e : entities) if (e->layer == layerIndex) result.push_back(e.get());
            return result;
        }

        /** @brief All entities currently active in the hierarchy (O(n)). */
        std::vector<Entity*> FindActiveEntities() const
        {
            std::vector<Entity*> result;
            for (const auto& e : entities) if (e->activeInHierarchy()) result.push_back(e.get());
            return result;
        }

        /** @brief Zero-allocation tag query — calls fn(Entity*) for each matching entity. */
        template<typename Fn>
        void ForEachWithTag(const std::string& tag, Fn&& fn) const
        {
            ensureTagIndex_();
            auto it = tagIndex_.find(tag);
            if (it != tagIndex_.end()) for (Entity* e : it->second) fn(e);
        }

        /** @brief Zero-allocation layer query — calls fn(Entity*) for each entity on the layer. */
        template<typename Fn>
        void ForEachByLayer(int layerIndex, Fn&& fn) const
        {
            for (const auto& e : entities) if (e->layer == layerIndex) fn(e.get());
        }

        /** @brief Zero-allocation active query — calls fn(Entity*) for every active entity. */
        template<typename Fn>
        void ForEachActive(Fn&& fn) const
        {
            for (const auto& e : entities) if (e->activeInHierarchy()) fn(e.get());
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
                    float r    = col->getCircleRadius();
                    Vector2 d  = Vector2Subtract(origin, gPos);
                    float b    = Vector2DotProduct(d, dir);
                    float c    = Vector2DotProduct(d, d) - r * r;
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
                    if (CheckCollisionCircleRec(e->getGlobalPosition(), col->getCircleRadius(), query)) result.push_back(e.get());
                }
                else
                {
                    ::Rectangle b = col ? col->getBounds() : e->getBounds();
                    if (CheckCollisionRecs(query, b)) result.push_back(e.get());
                }
            }
            return result;
        }

        /** @brief Spatial index rebuilt each frame — used by triggers and physics for broad-phase queries. */
        const SpatialGrid& GetEntityGrid() const { return entityGrid_; }

        /** @brief Schedules an entity (and its children) for destruction at the end of the frame. */
        void DestroyEntity(int id)
        {
            for (int existing : destroyQueue) if (existing == id) return;
            destroyQueue.push_back(id);
        }

        /**
         * @brief Populates this scene from a previously serialized JSON object.
         *
         * Symmetric counterpart to serialize(). Accepts a factory by template so
         * Scene.hpp does not need to include EntityFactory.hpp (which transitively
         * includes Scene.hpp, forming a cycle).
         */
        template<typename Factory>
        void deserialize(const nlohmann::json& j, Factory& factory)
        {
            if (j.contains("worldSize"))
            {
                worldSize.x = j["worldSize"][0];
                worldSize.y = j["worldSize"][1];
            }
            if (j.contains("editorCamera"))
            {
                editorCameraTarget.x = j["editorCamera"][0];
                editorCameraTarget.y = j["editorCamera"][1];
                editorCameraZoom     = j["editorCamera"][2];
            }
            if (j.contains("nextEntityId"))
                nextEntityId = j["nextEntityId"].get<int>();
            if (j.contains("entities"))
            {
                for (const auto& ej : j["entities"])
                {
                    auto entity = factory.LoadEntity(ej);
                    if (entity) entities.push_back(std::move(entity));
                }
                RebuildHierarchy();
                factory.RebuildEntityCounts(*this);
            }
            if (j.contains("storyState")) storyState = StoryValueMapFromJson(j["storyState"]);

            // Lighting — always reset first so switching from a lit scene to an unlit
            // one (whose JSON omits the block) doesn't inherit stale settings.
            lightingEnabled = j.value("lightingEnabled", false);
            ambientLight    = { 40, 40, 55, 255 };
            if (j.contains("ambientLight") && j["ambientLight"].is_array() && j["ambientLight"].size() >= 3)
            {
                ambientLight.r = j["ambientLight"][0].get<unsigned char>();
                ambientLight.g = j["ambientLight"][1].get<unsigned char>();
                ambientLight.b = j["ambientLight"][2].get<unsigned char>();
            }

            // Parallax — always reload (resets to defaults when the block is absent)
            // so switching from a parallax scene to a plain one clears stale settings.
            LoadParallaxFromJson(j);
        }
        

        /**
         * @brief Serializes the current active entities to a JSON object.
         */
        nlohmann::json serialize() const
        {
            nlohmann::json j;
            j["worldSize"]    = { worldSize.x, worldSize.y };
            j["editorCamera"] = { editorCameraTarget.x, editorCameraTarget.y, editorCameraZoom };

            // Lighting (only emit when used so untouched scenes keep clean JSON)
            if (lightingEnabled)
            {
                j["lightingEnabled"] = lightingEnabled;
                j["ambientLight"]    = { ambientLight.r, ambientLight.g, ambientLight.b };
            }

            nlohmann::json ents = nlohmann::json::array();
            for (const auto& e : entities)
            {
                ents.push_back(e->serialize());
            }
            j["entities"]     = ents;
            j["nextEntityId"] = nextEntityId;
            j["storyState"]   = StoryValueMapToJson(storyState);

            // Parallax (only emit when used so untouched scenes keep clean JSON)
            if (parallaxEnabled || !parallaxByLayer.empty() || parallaxAnchor.x != 0.0f || parallaxAnchor.y != 0.0f)
            {
                j["parallaxEnabled"] = parallaxEnabled;
                j["parallaxAnchor"]  = { parallaxAnchor.x, parallaxAnchor.y };
                nlohmann::json pj = nlohmann::json::object();
                for (const auto& [layer, factor] : parallaxByLayer) pj[std::to_string(layer)] = factor;
                j["parallaxByLayer"] = pj;
            }

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
                    for (auto& c : e->components) c->awake(this);
                    for (auto& c : e->components) c->start(this);
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
                for (auto& e : entities) e->fixedUpdate(FIXED_TIMESTEP, worldSize, this);
                RigidbodyComponent::ResolveScene(this, FIXED_TIMESTEP);
                fixedAccumulator -= FIXED_TIMESTEP;
                ++steps;
            }
            // Clamp leftover accumulator to avoid a spiral of death on heavy frames
            if (fixedAccumulator > FIXED_TIMESTEP * MAX_FIXED_STEPS) fixedAccumulator = 0.0f;

            // Rebuild spatial index from post-physics positions, then update all entities
            entityGrid_.Clear();
            for (int i = 0; i < (int)entities.size(); ++i)
            {
                if (!entities[i]->activeInHierarchy()) continue;
                auto* col = entities[i]->getComponent<Collider2D>();
                ::Rectangle bounds = col ? col->getBounds() : entities[i]->getBounds();
                entityGrid_.Insert(i, bounds);
            }

            for (auto& e : entities) { e->update(scaledDt, worldSize, this); }

            // Late update pass (camera follow, IK, post-update corrections)
            for (auto& e : entities) { e->lateUpdate(scaledDt, worldSize, this); }

            // 4. Flush destroy queue — safe to remove here, outside the iteration above
            if (!destroyQueue.empty())
            {
                // Collect IDs of every entity to remove, including their children recursively
                std::vector<int> toRemove;
                std::function<void(Entity*)> collectSubtree = [&](Entity* ent)
                {
                    toRemove.push_back(ent->id);
                    for (Entity* child : ent->children) collectSubtree(child);
                };

                for (int id : destroyQueue)
                {
                    Entity* ent = FindEntity(id);
                    if (ent) collectSubtree(ent);
                }
                destroyQueue.clear();

                // Free leaf-first — the reverse of collectSubtree's parent-first order.
                // Removing a parent before its children would leave each child's `parent`
                // pointer dangling, causing a use-after-free both at the unlink below and
                // in any component destroy() that dereferences its parent. Reverse order
                // guarantees a node's whole ancestor chain is still alive when it is freed.
                for (auto rit = toRemove.rbegin(); rit != toRemove.rend(); ++rit)
                {
                    int removeId = *rit;
                    auto iter = std::find_if(entities.begin(), entities.end(), [removeId](const std::unique_ptr<Entity>& e)
                    {
                        return e->id == removeId;
                    });
                    
                    if (iter == entities.end()) continue;

                    Entity* ent = iter->get();

                    // Notify components before the entity is destroyed (allows OnDestroy overrides)
                    for (auto& comp : ent->components) comp->destroy(this);

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
        /** @brief Spatial index rebuilt each frame before the variable update step. */
        SpatialGrid entityGrid_;

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
        bool                       snapshotParallaxEnabled_ = false;
        std::map<int, float>       snapshotParallaxByLayer_;
        Vector2                    snapshotParallaxAnchor_  = { 0.0f, 0.0f };
        bool                       snapshotLightingEnabled_ = false;
        Color                      snapshotAmbientLight_    = { 40, 40, 55, 255 };

        void rebuildTagIndex_() const
        {
            tagIndex_.clear();
            for (const auto& e : entities) tagIndex_[e->tag].push_back(e.get());
            tagIndexDirty_ = false;
        }
        
        void ensureTagIndex_() const
        {
            if (!tagIndexDirty_) return;
            rebuildTagIndex_();
        }
    };
}