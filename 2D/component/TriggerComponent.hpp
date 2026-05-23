#pragma once
#include <string>
#include <unordered_set>
#include "../../core/Component.hpp"
#include "raylib.h"

namespace Indium
{
    /**
     * @brief Invisible zone that fires TriggerEnterEvent / TriggerExitEvent on the EventBus.
     *
     * Attach to any entity. When another entity's AABB overlaps the zone, a
     * TriggerEnterEvent is published. When it leaves, TriggerExitEvent is published.
     * Events only fire during Play mode because Scene::Update is gated there.
     *
     * Usage (subscriber side):
     *   handle = Events::Subscribe<GameEvents::TriggerEnterEvent>([](auto& e) {
     *       TraceLog(LOG_INFO, "%s entered trigger", e.other->name.c_str());
     *   });
     */
    struct TriggerComponent : Component
    {
        Vector2 size      = {100.0f, 100.0f};
        Vector2 offset    = {0.0f, 0.0f};
        bool    showDebug = true;

        /** @brief Story flag set to true when an entity enters the zone (empty = none). */
        std::string setFlagOnEnter;

        /** @brief If set, enter events only fire while this story flag is true. */
        std::string requireFlag;

        void update(float dt, Vector2 worldSize, Scene* scene) override;
        void draw() const override;
        void inspect(std::function<void()> snapshotCb) override;
        std::string getName() const override;
        std::unique_ptr<Component> clone() const override;
        nlohmann::json serialize() const override;
        void deserialize(const nlohmann::json& j) override;

    private:
        std::unordered_set<int> trackedIds_;

        [[nodiscard]] ::Rectangle getZone() const;
    };
}
