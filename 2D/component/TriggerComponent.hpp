#pragma once
#include <unordered_set>
#include "../../core/Component.hpp"
#include "../../core/EventBus.hpp"
#include "../../core/events/GameEvents.hpp"
#include "../../core/scene/Scene.hpp"
#include "raylib.h"
#include "imgui.h"

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

        void update(float dt, Vector2 worldSize, Scene* scene) override
        {
            if (!scene || !owner) return;

            const ::Rectangle zone = getZone();
            std::unordered_set<int> currentlyInside;

            for (const auto& entity : scene->entities)
            {
                if (entity.get() == owner) continue;
                if (entity->depthLayer != owner->depthLayer) continue;
                if (CheckCollisionRecs(zone, entity->getBounds()))
                {
                    currentlyInside.insert(entity->id);

                    // Fire Enter only on the first frame of overlap
                    if (trackedIds_.find(entity->id) == trackedIds_.end())
                        Events::Publish(GameEvents::TriggerEnterEvent{owner, entity.get()});
                }
            }

            // Entities that were inside last frame but not this frame → Exit
            for (int trackedId : trackedIds_)
            {
                if (currentlyInside.find(trackedId) == currentlyInside.end())
                    Events::Publish(GameEvents::TriggerExitEvent{owner, scene->FindEntity(trackedId)});
            }

            trackedIds_ = std::move(currentlyInside);
        }

        // Draws the debug outline in world space (called from Scene::Draw via Component::draw).
        void draw() const override
        {
            if (!showDebug || !owner) return;

            const ::Rectangle zone = getZone();
            DrawRectangleLinesEx(zone, 1.5f, Color{0, 255, 128, 160});

            // Small crosshair at the zone centre
            const Vector2 center = {
                owner->position.x + offset.x,
                owner->position.y + offset.y
            };
            DrawLineV({center.x - 6.0f, center.y}, {center.x + 6.0f, center.y}, Color{0, 255, 128, 160});
            DrawLineV({center.x, center.y - 6.0f}, {center.x, center.y + 6.0f}, Color{0, 255, 128, 160});
        }

        void inspect() override
        {
            ImGui::Text("Size");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##TrigSize", &size.x, 1.0f, 1.0f, 10000.0f);
            ImGui::PopItemWidth();

            ImGui::Text("Offset");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##TrigOffset", &offset.x, 1.0f);
            ImGui::PopItemWidth();

            ImGui::Checkbox("Show Debug", &showDebug);

            ImGui::Spacing();
            ImGui::TextDisabled("Tracked entities: %d", static_cast<int>(trackedIds_.size()));
        }

        std::string getName() const override { return "Trigger"; }

        std::unique_ptr<Component> clone() const override
        {
            auto copy = std::make_unique<TriggerComponent>(*this);
            copy->trackedIds_.clear(); // runtime tracking state is not cloned
            return copy;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["size"]      = {size.x, size.y};
            j["offset"]    = {offset.x, offset.y};
            j["showDebug"] = showDebug;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            if (j.contains("size"))
            {
                size.x = j["size"][0];
                size.y = j["size"][1];
            }
            if (j.contains("offset"))
            {
                offset.x = j["offset"][0];
                offset.y = j["offset"][1];
            }
            if (j.contains("showDebug")) showDebug = j["showDebug"].get<bool>();
        }

    private:
        std::unordered_set<int> trackedIds_;

        [[nodiscard]] ::Rectangle getZone() const
        {
            return ::Rectangle{
                owner->position.x + offset.x - size.x * 0.5f,
                owner->position.y + offset.y - size.y * 0.5f,
                size.x,
                size.y
            };
        }
    };
}
