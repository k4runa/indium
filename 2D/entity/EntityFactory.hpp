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
#include "../../core/ScriptManager.hpp"

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
            c->radius   = circleConfig.defaultRadius;

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

            // Deserialize Components
            if (j.contains("components"))
            {
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
                    else if (cType == "NativeScript")
                    {
                        std::string sName = cj.contains("scriptName") ? cj["scriptName"].get<std::string>() : "";
                        Component* scriptComp = ScriptManager::Get().InstantiateScript(sName);
                        if (scriptComp)
                        {
                            scriptComp->deserialize(cj);
                            entity->addComponent(std::unique_ptr<Component>(scriptComp));
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
                            TraceLog(LOG_WARNING, "FACTORY: Unknown component or script type: %s", cType.c_str());
                        }
                    }
                }
            }

            return entity;
        }
    };

}
