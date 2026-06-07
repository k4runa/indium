#pragma once

#include "raylib.h"
#include "Circle.hpp"
#include "Plane.hpp"
#include "Rectangle.hpp"
#include "Tilemap.hpp"
#include "../sprite/Sprite.hpp"
#include "../component/RigidbodyComponent.hpp"
#include "../component/BouncerComponent.hpp"
#include "../component/CameraComponent.hpp"
#include "../component/TriggerComponent.hpp"
#include "../component/AnimatorComponent.hpp"
#include "../component/TweenComponent.hpp"
#include "../component/AnimatorStateMachineComponent.hpp"
#include "../component/Collider2D.hpp"
#include "../component/ShapeRendererComponent.hpp"
#include "../component/SpriteRendererComponent.hpp"
#include "../component/AudioSourceComponent.hpp"
#include "../component/TextRendererComponent.hpp"
#include "../component/ParticleSystemComponent.hpp"
#include "../component/TilemapComponent.hpp"
#include "../component/Light2DComponent.hpp"
#include "../component/InteractableComponent.hpp"
#include "../component/PlayerInteractorComponent.hpp"
#include "../component/InventoryComponent.hpp"
#include "../component/AudioListenerComponent.hpp"
#include "../component/Joint2D.hpp"
#include "../component/SortingGroup.hpp"
#include "../component/PathFollowerComponent.hpp"
#include "../component/FlipComponent.hpp"
#include "../component/TimerComponent.hpp"
#include "../component/LineRendererComponent.hpp"
#include "../component/AreaEffect2DComponent.hpp"
#include "../component/NavigationAgent2DComponent.hpp"
#include "../component/PostProcessComponent.hpp"
#include "../component/TrailRendererComponent.hpp"
#include "../component/SpawnPointComponent.hpp"
#include "../component/CheckpointComponent.hpp"
#include "../component/PhysicsMaterial2DComponent.hpp"
#include "../component/NavigationRegion2DComponent.hpp"
#include "../component/DecalComponent.hpp"
#include "../component/SpriteSheetComponent.hpp"
#include "../../core/ScriptManager.hpp"
#include "../../core/PlaceholderComponent.hpp"

namespace Indium
{
    /**
     * @brief Configuration for a Sprite entity.
     */
    struct SpriteConfig
    {
        Vector2 defaultPosition = { 400, 400 };
    };

    /**
     * @brief Configuration for a Circle entity.
     */
    struct CircleConfig
    {
        Color   defaultColor    = WHITE;
        float   defaultRadius   = 50.0f;
        Vector2 defaultPosition = { 400, 400 };
    };

    /**
     * @brief Configuration for a Rectangle entity.
     */
    struct RectangleConfig
    {
        Color   defaultColor    = WHITE;
        Vector2 defaultPosition = { 400, 400 };
        Vector2 defaultScale    = { 200, 100 };
    };

    /**
     * @brief Configuration for a Plane entity.
     */
    struct PlaneConfig
    {
        Color   defaultColor    = WHITE;
        Vector2 defaultPosition = { 400, 400 };
        Vector2 defaultScale    = { 500, 5 };
    };

    /**
     * @brief Factory class to handle the creation of various entities.
     */
    class EntityFactory
    {
    private:
        RectangleConfig rectangleConfig;
        CircleConfig    circleConfig;
        PlaneConfig     planeConfig;
        SpriteConfig    spriteConfig;

    public:
        /** @brief Creates a new Sprite entity and adds it to the scene count */
        std::unique_ptr<Sprite> CreateSprite(Scene& scene)
        {
            auto s = std::make_unique<Sprite>();
            s->id       = scene.nextEntityId++;
            s->name     = "Image " + std::to_string(scene.entityCounts["Sprite"]++);
            s->position = spriteConfig.defaultPosition;
            // Give it a real, clickable size up front. Without a texture the renderer
            // draws a 100x100 placeholder, but the BoxCollider2D (useEntityScale=true)
            // derives its bounds from scale — which defaults to {1,1}, making the hit
            // box 1px and the sprite unselectable. Match the placeholder so drawing,
            // bounds and picking all agree. SetTexture() later overwrites this with
            // the texture's real dimensions.
            s->scale    = { 100.0f, 100.0f };
            return s;
        }

        /** @brief Creates a new Circle entity and adds it to the scene count */
        std::unique_ptr<Circle> CreateCircle(Scene& scene)
        {
            auto c = std::make_unique<Circle>();
            c->id       = scene.nextEntityId++;
            c->name     = "Circle " + std::to_string(scene.entityCounts["Circle"]++);
            c->color    = circleConfig.defaultColor;
            c->position = circleConfig.defaultPosition;
            if (auto* col = c->getComponent<CircleCollider2D>()) col->radius = circleConfig.defaultRadius;

            return c;
        }

        /** @brief Creates a new Rectangle entity and adds it to the scene count */
        std::unique_ptr<Rectangle> CreateRectangle(Scene& scene)
        {
            auto r = std::make_unique<Rectangle>();
            r->id       = scene.nextEntityId++;
            r->name     = "Rectangle " + std::to_string(scene.entityCounts["Rectangle"]++);
            r->color    = rectangleConfig.defaultColor;
            r->position = rectangleConfig.defaultPosition;
            r->scale    = rectangleConfig.defaultScale;

            return r;
        }

        /** @brief Creates a new Tilemap entity (a transform owning a TilemapComponent) */
        std::unique_ptr<Tilemap> CreateTilemap(Scene& scene)
        {
            auto t = std::make_unique<Tilemap>();
            t->id       = scene.nextEntityId++;
            t->name     = "Tilemap " + std::to_string(scene.entityCounts["Tilemap"]++);
            t->position = { 400, 400 };
            return t;
        }

        /** @brief Creates a new Camera entity (transparent Rectangle + CameraComponent) */
        std::unique_ptr<Rectangle> CreateCamera(Scene& scene)
        {
            auto r = std::make_unique<Rectangle>();
            r->id       = scene.nextEntityId++;
            r->name     = "Camera " + std::to_string(scene.entityCounts["Camera"]++);
            r->color    = Color{255, 255, 255, 0};
            r->position = { 400, 300 };
            r->scale    = { 40, 24 };
            r->addComponent<CameraComponent>();
            return r;
        }

        /** @brief Creates a new Plane entity and adds it to the scene count */
        std::unique_ptr<Plane> CreatePlane(Scene& scene)
        {
            auto p = std::make_unique<Plane>();
            p->id       = scene.nextEntityId++;
            p->name     = "Surface " + std::to_string(scene.entityCounts["Plane"]++);
            p->color    = planeConfig.defaultColor;
            p->position = planeConfig.defaultPosition;
            p->scale    = planeConfig.defaultScale;

            return p;
        }

        /** @brief Creates an empty entity — no renderer, no collider.
         *  Use as a parent container or script-only node. */
        std::unique_ptr<Rectangle> CreateEmpty(Scene& scene)
        {
            auto e = std::make_unique<Rectangle>();
            e->id       = scene.nextEntityId++;
            e->name     = "Empty " + std::to_string(scene.entityCounts["Empty"]++);
            e->color    = Color{0, 0, 0, 0};
            e->position = {0, 0};
            e->scale    = {32.0f, 32.0f};
            // Remove the auto-added BoxCollider2D and ShapeRenderer so it is truly empty
            e->components.clear();
            return e;
        }

        /** @brief Creates an entity pre-configured for text rendering. */
        std::unique_ptr<Rectangle> CreateText(Scene& scene)
        {
            auto t = std::make_unique<Rectangle>();
            t->id       = scene.nextEntityId++;
            t->name     = "Text " + std::to_string(scene.entityCounts["Text"]++);
            t->color    = Color{0, 0, 0, 0};
            t->position = {0, 0};
            t->scale    = {200.0f, 50.0f};
            t->components.clear();
            t->addComponent<TextRendererComponent>();
            return t;
        }

        /** @brief Creates an entity pre-configured as a 2D light source. */
        std::unique_ptr<Rectangle> CreateLight(Scene& scene)
        {
            auto l = std::make_unique<Rectangle>();
            l->id       = scene.nextEntityId++;
            l->name     = "Light " + std::to_string(scene.entityCounts["Light"]++);
            l->color    = Color{0, 0, 0, 0};
            l->position = {0, 0};
            l->scale    = {32.0f, 32.0f};
            l->components.clear();
            l->addComponent<Light2DComponent>();
            return l;
        }

        /** @brief Creates an entity pre-configured as a particle system. */
        std::unique_ptr<Rectangle> CreateParticleSystem(Scene& scene)
        {
            auto ps = std::make_unique<Rectangle>();
            ps->id       = scene.nextEntityId++;
            ps->name     = "Particle System " + std::to_string(scene.entityCounts["ParticleSystem"]++);
            ps->color    = Color{0, 0, 0, 0};
            ps->position = {0, 0};
            ps->scale    = {32.0f, 32.0f};
            ps->components.clear();
            ps->addComponent<ParticleSystemComponent>();
            return ps;
        }

        // Internal helper: an invisible, collider-free node carrying one component.
        template<typename T>
        std::unique_ptr<Rectangle> makeArchetype_(Scene& scene, const char* key,
                                                  const char* prefix, Vector2 size)
        {
            auto e = std::make_unique<Rectangle>();
            e->id       = scene.nextEntityId++;
            e->name     = std::string(prefix) + std::to_string(scene.entityCounts[key]++);
            e->color    = Color{0, 0, 0, 0};
            e->position = {0, 0};
            e->scale    = size;
            e->components.clear();
            e->addComponent<T>();
            return e;
        }

        /** @brief Empty entity carrying a Trigger zone. */
        std::unique_ptr<Rectangle> CreateTriggerZone(Scene& scene)
        { return makeArchetype_<TriggerComponent>(scene, "Trigger", "Trigger ", {100.0f, 100.0f}); }

        /** @brief Empty entity carrying an Audio Source. */
        std::unique_ptr<Rectangle> CreateAudioSource(Scene& scene)
        { return makeArchetype_<AudioSourceComponent>(scene, "Audio", "Audio ", {32.0f, 32.0f}); }

        /** @brief Empty entity marking a spawn location. */
        std::unique_ptr<Rectangle> CreateSpawnPoint(Scene& scene)
        { return makeArchetype_<SpawnPointComponent>(scene, "SpawnPoint", "Spawn Point ", {32.0f, 32.0f}); }

        /** @brief Empty entity carrying a Checkpoint zone. */
        std::unique_ptr<Rectangle> CreateCheckpoint(Scene& scene)
        { return makeArchetype_<CheckpointComponent>(scene, "Checkpoint", "Checkpoint ", {100.0f, 150.0f}); }

        /**
         * @brief Scans loaded entity names and sets entityCounts to the correct next index.
         *
         * Must be called after populating scene.entities from disk so that newly created
         * entities don't reuse names that already exist in the scene.
         */
        void RebuildEntityCounts(Scene& scene)
        {
            // Maps entity type tag → the name prefix used by Create* methods
            static const std::pair<const char*, const char*> prefixTable[] = {
                {"Circle",    "Circle "},
                {"Rectangle", "Rectangle "},
                {"Plane",     "Surface "},
                {"Sprite",    "Image "},
                {"Tilemap",   "Tilemap "},
            };

            for (const auto& e : scene.entities)
            {
                const std::string& type = e->getType();
                const std::string& name = e->name;

                for (const auto& [typeKey, prefix] : prefixTable)
                {
                    if (type != typeKey) continue;
                    size_t prefixLen = strlen(prefix);
                    if (name.size() <= prefixLen) break;
                    if (name.compare(0, prefixLen, prefix) != 0) break;

                    try
                    {
                        int num = std::stoi(name.substr(prefixLen));
                        int& count = scene.entityCounts[typeKey];
                        if (num + 1 > count) count = num + 1;
                    } catch (...) {}
                    break;
                }

                // Rectangle-based named entities — track each prefix separately
                static const std::pair<const char*, const char*> rectPrefixes[] = {
                    {"Camera",          "Camera "},
                    {"Empty",           "Empty "},
                    {"Text",            "Text "},
                    {"Light",           "Light "},
                    {"ParticleSystem",  "Particle System "},
                    {"Tilemap",         "Tilemap "},
                    {"Trigger",         "Trigger "},
                    {"Audio",           "Audio "},
                    {"SpawnPoint",      "Spawn Point "},
                    {"Checkpoint",      "Checkpoint "},
                };
                for (const auto& [key, prefix] : rectPrefixes)
                {
                    size_t pfxLen = strlen(prefix);
                    if (name.size() > pfxLen && name.compare(0, pfxLen, prefix) == 0)
                    {
                        try
                        {
                            int num = std::stoi(name.substr(pfxLen));
                            int& count = scene.entityCounts[key];
                            if (num + 1 > count) count = num + 1;
                        } catch (...) {}
                        break;
                    }
                }
            }
        }

        /** @brief Instantiates the correct Entity type based on JSON data */
        std::unique_ptr<Entity> LoadEntity(const nlohmann::json& j)
        {
            if (!j.contains("type")) return nullptr;

            std::string type = j["type"].get<std::string>();
            std::unique_ptr<Entity> entity;

            if (type == "Sprite")         entity = std::make_unique<Sprite>();
            else if (type == "Rectangle") entity = std::make_unique<Rectangle>();
            else if (type == "Circle")    entity = std::make_unique<Circle>();
            else if (type == "Plane")     entity = std::make_unique<Plane>();
            else if (type == "Tilemap")   entity = std::make_unique<Tilemap>();
            else return nullptr;

            entity->deserialize(j);

            // Deserialize Components.
            // For types that are auto-added by entity constructors (BoxCollider2D,
            // CircleCollider2D, ShapeRenderer, SpriteRenderer), we deserialize
            // INTO the existing component rather than adding a duplicate.
            if (j.contains("components"))
            {
                // Helper: find first existing component by getName() string
                auto findComp = [&](const std::string& name) -> Component* { for (auto& comp : entity->components)if (comp->getName() == name) return comp.get(); return nullptr; };

                // Helper: deserialize-or-create for auto-constructor types
                auto deserializeOrCreate = [&](const std::string& name, auto makeNew, const nlohmann::json& cj)
                {
                    Component* existing = findComp(name);
                    if (existing) { existing->deserialize(cj); }
                    else          { auto c = makeNew(); c->deserialize(cj); entity->addComponent(std::move(c)); }
                };

                for (const auto& cj : j["components"])
                {
                    if (!cj.contains("type")) continue;
                    std::string cType = cj["type"].get<std::string>();

                    if (cType == "Rigidbody")
                    {
                        auto c = std::make_unique<RigidbodyComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "Bouncer")
                    {
                        auto c = std::make_unique<BouncerComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "Camera Component")
                    {
                        auto c = std::make_unique<CameraComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "Trigger")
                    {
                        auto c = std::make_unique<TriggerComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "Animator")
                    {
                        auto c = std::make_unique<AnimatorComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "Tween")
                    {
                        auto c = std::make_unique<TweenComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "AnimatorStateMachine")
                    {
                        auto c = std::make_unique<AnimatorStateMachineComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "BoxCollider2D")    deserializeOrCreate("BoxCollider2D", []{ return std::make_unique<BoxCollider2D>(); }, cj);
                    else if (cType == "CircleCollider2D") deserializeOrCreate("CircleCollider2D", []{ return std::make_unique<CircleCollider2D>(); }, cj);
                    else if (cType == "ShapeRenderer")    deserializeOrCreate("ShapeRenderer", []{ return std::make_unique<ShapeRendererComponent>(); }, cj);
                    else if (cType == "SpriteRenderer")   deserializeOrCreate("SpriteRenderer", []{ return std::make_unique<SpriteRendererComponent>(); }, cj);
                    else if (cType == "AudioSource")
                    {
                        auto c = std::make_unique<AudioSourceComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "TextRenderer")
                    {
                        auto c = std::make_unique<TextRendererComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "ParticleSystem")
                    {
                        auto c = std::make_unique<ParticleSystemComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    // TilemapComponent is auto-added by the Tilemap entity ctor, so
                    // deserialize INTO the existing one rather than adding a duplicate;
                    // for other entity types (manual Add Component) it creates one.
                    else if (cType == "Tilemap")          deserializeOrCreate("Tilemap", []{ return std::make_unique<TilemapComponent>(); }, cj);
                    else if (cType == "Light2D")
                    {
                        auto c = std::make_unique<Light2DComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "Interactable")
                    {
                        auto c = std::make_unique<InteractableComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "PlayerInteractor")
                    {
                        auto c = std::make_unique<PlayerInteractorComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "Inventory")
                    {
                        auto c = std::make_unique<InventoryComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "PolygonCollider2D")
                        deserializeOrCreate("PolygonCollider2D", []{ return std::make_unique<PolygonCollider2D>(); }, cj);
                    else if (cType == "EdgeCollider2D")
                        deserializeOrCreate("EdgeCollider2D",    []{ return std::make_unique<EdgeCollider2D>(); },    cj);
                    else if (cType == "AudioListener")
                    {
                        auto c = std::make_unique<AudioListenerComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "DistanceJoint2D")
                    {
                        auto c = std::make_unique<DistanceJoint2D>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "HingeJoint2D")
                    {
                        auto c = std::make_unique<HingeJoint2D>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "SortingGroup")
                    {
                        auto c = std::make_unique<SortingGroup>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "SpringJoint2D")
                    {
                        auto c = std::make_unique<SpringJoint2D>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "PathFollower")
                    {
                        auto c = std::make_unique<PathFollowerComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "Flip")
                    {
                        auto c = std::make_unique<FlipComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "Timer")
                    {
                        auto c = std::make_unique<TimerComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "LineRenderer")
                    {
                        auto c = std::make_unique<LineRendererComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "AreaEffect2D")
                    {
                        auto c = std::make_unique<AreaEffect2DComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "NavigationAgent2D")
                    {
                        auto c = std::make_unique<NavigationAgent2DComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "PostProcess")
                    {
                        auto c = std::make_unique<PostProcessComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "TrailRenderer")
                    {
                        auto c = std::make_unique<TrailRendererComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "SpawnPoint")
                    {
                        auto c = std::make_unique<SpawnPointComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "Checkpoint")
                    {
                        auto c = std::make_unique<CheckpointComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "PhysicsMaterial2D")
                    {
                        auto c = std::make_unique<PhysicsMaterial2DComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "NavigationRegion2D")
                    {
                        auto c = std::make_unique<NavigationRegion2DComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "Decal")
                    {
                        auto c = std::make_unique<DecalComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "SpriteSheet")
                    {
                        auto c = std::make_unique<SpriteSheetComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
                    else if (cType == "NativeScript")
                    {
                        std::string sName = cj.contains("scriptName") ? cj["scriptName"].get<std::string>() : "";
                        Component* scriptComp = ScriptManager::Get().InstantiateScript(sName);
                        if (scriptComp)
                        {
                            scriptComp->deserialize(cj);
                            entity->addComponent(std::unique_ptr<Component>(scriptComp));
                        }
                        else
                        {
                            TraceLog(LOG_WARNING, "FACTORY: Script '%s' not found — preserved as placeholder", sName.c_str());
                            auto ph = std::make_unique<PlaceholderComponent>();
                            ph->deserialize(cj);
                            entity->addComponent(std::move(ph));
                        }
                    }
                    else
                    {
                        // Fallback: Try loading from dynamically compiled scripts
                        Component* scriptComp = ScriptManager::Get().InstantiateScript(cType);
                        if (scriptComp)
                        {
                            scriptComp->deserialize(cj);
                            entity->addComponent(std::unique_ptr<Component>(scriptComp));
                        }
                        else
                        {
                            TraceLog(LOG_WARNING, "FACTORY: Unknown component type '%s' — preserved as placeholder", cType.c_str());
                            auto ph = std::make_unique<PlaceholderComponent>();
                            ph->deserialize(cj);
                            entity->addComponent(std::move(ph));
                        }
                    }
                }
            }

            return entity;
        }
    };

}
