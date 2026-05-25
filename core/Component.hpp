#pragma once
#include "raylib.h"
#include <string>
#include <memory>
#include <functional>
#include "imgui.h"
#include "../include/nlohmann/json.hpp"

namespace Indium
{
    /**
     * @brief Forward declarations for circular dependencies.
     */
    struct Entity;
    struct Scene;

    /**
     * @brief The base class for all modular logic within the Indium engine.
     */
    struct Component
    {
        Entity* owner   = nullptr;
        bool    enabled = true;

        void setEnabled(bool e)
        {
            if (enabled == e) return;
            enabled = e;
            if (enabled) onEnable();
            else         onDisable();
        }

        virtual void update(float dt, Vector2 worldSize, Scene* scene) = 0;

        /** @brief Called at a fixed rate (FIXED_TIMESTEP) regardless of frame rate.
         *  Physics, velocity integration, and collision resolution go here. */
        virtual void fixedUpdate(float fixedDt, Vector2 worldSize, Scene* scene) {}

        /** @brief Called after all update() calls for this frame. Camera follow, IK, etc. */
        virtual void lateUpdate(float dt, Vector2 worldSize, Scene* scene) {}

        virtual void awake(Scene* scene = nullptr) {}
        virtual void start(Scene* scene = nullptr) {}
        virtual void destroy(Scene* scene = nullptr) {}

        /** @brief Called when the owning entity (or an ancestor) transitions from inactive → active. */
        virtual void onEnable() {}

        /** @brief Called when the owning entity (or an ancestor) transitions from active → inactive. */
        virtual void onDisable() {}
        virtual void inspect(std::function<void()> snapshotCb = {}) {}
        virtual void draw() const {}

        /** @brief Screen-space UI pass, called each frame during Play/Pause after the
         *  world is drawn. NativeScript routes this to the user OnGUI() hook; engine
         *  components (e.g. PlayerInteractor) may override it to draw a HUD / prompt. */
        virtual void onGUI(Scene* scene) {}
        virtual std::string getName() const = 0;
        virtual std::unique_ptr<Component> clone() const = 0;

        /** @brief Serializes the component data to JSON. */
        virtual nlohmann::json serialize() const
        {
            nlohmann::json j;
            j["type"]    = getName();
            j["enabled"] = enabled;
            return j;
        }

        /** @brief Deserializes the component data from JSON. */
        virtual void deserialize(const nlohmann::json& j)
        {
            if (j.contains("enabled")) enabled = j["enabled"].get<bool>();
        }

        virtual ~Component() = default;
    };
}
