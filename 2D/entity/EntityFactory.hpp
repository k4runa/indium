#pragma once

#include "raylib.h"
#include "Circle.hpp"
#include "Plane.hpp"
#include "Rectangle.hpp"
#include "../sprite/Sprite.hpp"
#include "../../core/scene/Scene.hpp"
#include <memory>
#include <string>



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
            s->name     = "Sprite " + std::to_string(scene.entityCounts["Sprite"]++);
            s->position = spriteConfig.defaultPosition;
            return s;
        }

        /** @brief Creates a new Circle entity and adds it to the scene count */
        std::unique_ptr<Circle> CreateCircle(Scene& scene)
        {
            auto c = std::make_unique<Circle>();
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
            p->name     = "Plane " + std::to_string(scene.entityCounts["Plane"]++);
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
            return entity;
        }
    };

}
