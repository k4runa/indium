/**********************************************************************************************
*
*   EntityFactory - Standardized object instantiation
*
*   Utility for creating entities with consistent default properties and 
*   unique identifiers within a scene.
*
*   Copyright (c) 2026
*
**********************************************************************************************/

#pragma once

#include "raylib.h"
#include "../Entity/Circle.hpp"
#include "../Entity/Plane.hpp"
#include "../Entity/Rectangle.hpp"
#include "../Scene/Scene.hpp"
#include <memory>
#include <string>

namespace Indium
{
    /**
     * @brief Template configuration for spawning Circle entities.
     * 
     * This allows the factory to maintain default values for newly created 
     * objects, ensuring consistency across the engine.
     */
    struct CircleConfig
    {
        Color   defaultColor    = WHITE;
        float   defaultRadius   = 50.0f;
        Vector2 defaultPosition = { 400, 400 };
    };

    /**
     * @brief Template configuration for spawning Rectangle entities.
     */
    struct RectangleConfig
    {
        Color   defaultColor    = WHITE;
        Vector2 defaultPosition = { 400, 400 };
        Vector2 defaultScale    = { 200, 100 };
    };

    /**
     * @brief Template configuration for spawning Plane entities.
     */
    struct PlaneConfig
    {
        Color   defaultColor    = WHITE;
        Vector2 defaultPosition = { 400, 400 };
        Vector2 defaultScale    = { 500, 5 };
    };

    /**
     * @brief Centralized factory for standardized entity creation.
     * 
     * The EntityFactory abstracts the complexities of entity initialization. 
     * It handles unique naming, default property assignment, and ensures 
     * that all entities are created in a valid initial state.
     */
    class EntityFactory
    {
    private:
        RectangleConfig rectangleConfig;
        CircleConfig    circleConfig;
        PlaneConfig     planeConfig;

    public:
        /** 
         * @brief Spawns a new Circle entity with default settings.
         * 
         * @param scene The target scene, used to generate a unique name (e.g., "Circle 1").
         * @return A unique_ptr to the fully initialized Circle.
         */
        std::unique_ptr<Circle> CreateCircle(Scene& scene)
        {
            auto c = std::make_unique<Circle>();
            c->name     = "Circle " + std::to_string(scene.entityCounts["Circle"]++);
            c->color    = circleConfig.defaultColor;
            c->position = circleConfig.defaultPosition;
            c->radius   = circleConfig.defaultRadius;

            return c;
        }

        /** 
         * @brief Spawns a new Rectangle entity with default settings.
         * 
         * @param scene The target scene.
         * @return A unique_ptr to the fully initialized Rectangle.
         */
        std::unique_ptr<Rectangle> CreateRectangle(Scene& scene)
        {
            auto r = std::make_unique<Rectangle>();
            r->name     = "Rectangle " + std::to_string(scene.entityCounts["Rectangle"]++);
            r->color    = rectangleConfig.defaultColor;
            r->position = rectangleConfig.defaultPosition;
            r->scale    = rectangleConfig.defaultScale;

            return r;
        }

        /** 
         * @brief Spawns a new Plane entity with default settings.
         * 
         * @param scene The target scene.
         * @return A unique_ptr to the fully initialized Plane.
         */
        std::unique_ptr<Plane> CreatePlane(Scene& scene)
        {
            auto p = std::make_unique<Plane>();
            p->name     = "Plane " + std::to_string(scene.entityCounts["Plane"]++);
            p->color    = planeConfig.defaultColor;
            p->position = planeConfig.defaultPosition;
            p->scale    = planeConfig.defaultScale;

            return p;
        }
    };
}
