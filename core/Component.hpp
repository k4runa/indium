#pragma once
#include "raylib.h"
#include <string>
#include <memory>
#include "imgui.h"

namespace Indium
{
    /**
     * @brief Forward declarations for circular dependencies.
     *
     * We use forward declarations here to break the circular dependency between Component,
     * Entity, and Scene. Since these structures refer to each other through pointers,
     * the compiler only needs to know they exist, not their full definition at this stage.
     */
    struct Entity;
    struct Scene;

    /**
     * @brief The base class for all modular logic within the Indium engine.
     *
     * The Component class follows a Composition-over-Inheritance pattern. Instead of
     * creating complex entity hierarchies, functionality (like physics, AI, or rendering)
     * is encapsulated into components and attached to a generic Entity.
     */
    struct Component
    {
        /**
         * @brief A pointer to the Entity that owns this component.
         *
         * This allows the component to access and modify the owner's properties,
         * such as position, rotation, or other sibling components.
         */
        Entity* owner = nullptr;

        /**
         * @brief The logic update cycle for the component.
         *
         * This is called once per frame.
         * @param dt The time elapsed since the last frame (Delta Time).
         * @param worldSize The current boundaries of the simulation area.
         * @param scene A reference to the active scene, allowing interaction with other entities.
         */
        virtual void update(float dt, Vector2 worldSize, Scene* scene) = 0;

        /**
         * @brief Renders the component's internal state into the Editor UI.
         *
         * Use this to expose variables to the ImGui Inspector. This allows for
         * real-time debugging and value tweaking without recompiling.
         */
        virtual void inspect() {}

        /** @brief Returns a human-readable name for the component type. */
        virtual std::string getName() const = 0;

        /**
         * @brief Creates a deep copy of this component.
         *
         * This is essential for the engine's "Play/Stop" functionality, allowing
         * the scene to be snapshotted and restored to its original state.
         */
        virtual std::unique_ptr<Component> clone() const = 0;

        /** @brief Virtual destructor ensuring proper cleanup of derived components. */
        virtual ~Component() = default;
    };
}
