#pragma once

#include "raylib.h"
#include "Circle.hpp"
#include "Plane.hpp"
#include "Rectangle.hpp"
#include "../sprite/Sprite.hpp"
#include "../component/RigidbodyComponent.hpp"
#include "../component/BouncerComponent.hpp"
#include "../component/CameraComponent.hpp"
#include "../component/TriggerComponent.hpp"
#include "../component/AnimatorComponent.hpp"
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

                // Camera entities are Rectangles named "Camera N" — track separately
                static constexpr size_t camPfxLen = 7; // strlen("Camera ")
                if (name.size() > camPfxLen && name.compare(0, camPfxLen, "Camera ") == 0)
                {
                    try
                    {
                        int num = std::stoi(name.substr(camPfxLen));
                        int& count = scene.entityCounts["Camera"];
                        if (num + 1 > count) count = num + 1;
                    } catch (...) {}
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
                    else if (cType == "Tilemap")
                    {
                        auto c = std::make_unique<TilemapComponent>();
                        c->deserialize(cj);
                        entity->addComponent(std::move(c));
                    }
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
