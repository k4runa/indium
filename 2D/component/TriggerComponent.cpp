#include "TriggerComponent.hpp"
#include "../../core/EventBus.hpp"
#include "../../core/events/GameEvents.hpp"
#include "../../core/scene/Scene.hpp"
#include "../../core/StoryState.hpp"
#include "../../core/NativeScript.hpp"
#include "Collider2D.hpp"
#include "imgui.h"
#include <cstring>

namespace Indium
{
    void TriggerComponent::update(float, Vector2, Scene* scene)
    {
        if (!scene || !owner) return;

        const ::Rectangle zone = getZone();
        std::unordered_set<int> currentlyInside;

        // Broad-phase: only test entities whose AABB overlaps the zone cell(s)
        auto candidates = scene->GetEntityGrid().Query(zone);
        std::unordered_set<int> visitedIdx;
        for (int idx : candidates)
        {
            if (!visitedIdx.insert(idx).second) continue;
            const auto& entity = scene->entities[idx];
            if (entity.get() == owner) continue;
            if (entity->depthLayer != owner->depthLayer) continue;
            // Narrow-phase: circle colliders use exact circle-vs-rect test so
            // the bounding box corner can't ghost-trigger the zone.
            const auto* col = entity->getComponent<Collider2D>();
            const bool overlaps = (col && col->isCircleShape()) ? CheckCollisionCircleRec( Vector2Add(entity->getGlobalPosition(), col->offset), col->getCircleRadius(), zone) : CheckCollisionRecs(zone, entity->getBounds());

            if (overlaps)
            {
                // Fire Enter only on the first frame of overlap
                if (trackedIds_.find(entity->id) == trackedIds_.end())
                {
                    // A gated trigger stays dormant until its flag is set —
                    // skip tracking so the Enter can still fire once it is.
                    if (!requireFlag.empty() && !StoryState::Get().HasFlag(requireFlag)) continue;
                    Events::Publish(GameEvents::TriggerEnterEvent{owner, entity.get()});
                    if (!setFlagOnEnter.empty()) StoryState::Get().SetFlag(setFlagOnEnter);
                    // Notify scripts on the trigger owner
                    for (auto& c : owner->components) if (auto* ns = dynamic_cast<NativeScript*>(c.get())) ns->OnTriggerEnter2D(entity.get());
                    // Notify scripts on the entering entity
                    for (auto& c : entity->components) if (auto* ns = dynamic_cast<NativeScript*>(c.get())) ns->OnTriggerEnter2D(owner);
                }
                currentlyInside.insert(entity->id);
            }
        }

        // Entities that were inside last frame but not this frame → Exit
        for (int trackedId : trackedIds_)
        {
            if (currentlyInside.find(trackedId) == currentlyInside.end())
            {
                Entity* exiting = scene->FindEntity(trackedId);
                if (exiting)
                {
                    Events::Publish(GameEvents::TriggerExitEvent{owner, exiting});

                    // Notify scripts on the trigger owner
                    for (auto& c : owner->components) if (auto* ns = dynamic_cast<NativeScript*>(c.get())) ns->OnTriggerExit2D(exiting);
                    // Notify scripts on the exiting entity
                    for (auto& c : exiting->components) if (auto* ns = dynamic_cast<NativeScript*>(c.get())) ns->OnTriggerExit2D(owner);
                }
            }
        }

        trackedIds_ = std::move(currentlyInside);
    }

    void TriggerComponent::draw() const
    {
        if (!showDebug || !owner) return;

        const ::Rectangle zone = getZone();
        DrawRectangleLinesEx(zone, 1.5f, Color{0, 255, 128, 160});

        // Small crosshair at the zone centre
        Vector2 gPos = owner->getGlobalPosition();
        const Vector2 center = {
            gPos.x + offset.x,
            gPos.y + offset.y
        };
        DrawLineV({center.x - 6.0f, center.y}, {center.x + 6.0f, center.y}, Color{0, 255, 128, 160});
        DrawLineV({center.x, center.y - 6.0f}, {center.x, center.y + 6.0f}, Color{0, 255, 128, 160});
    }

    void TriggerComponent::inspect(std::function<void()> snapshotCb)
    {
        ImGui::Text("Size");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat2("##TrigSize", &size.x, 1.0f, 1.0f, 10000.0f);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();
        ImGui::Text("Offset");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat2("##TrigOffset", &offset.x, 1.0f);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();
        ImGui::Checkbox("Show Debug", &showDebug);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Story State");

        char setBuf[64] = {};
        strncpy(setBuf, setFlagOnEnter.c_str(), sizeof(setBuf) - 1);
        ImGui::Text("Set Flag On Enter");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##TrigSetFlag", setBuf, sizeof(setBuf))) setFlagOnEnter = setBuf;
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        char reqBuf[64] = {};
        strncpy(reqBuf, requireFlag.c_str(), sizeof(reqBuf) - 1);
        ImGui::Text("Require Flag");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##TrigReqFlag", reqBuf, sizeof(reqBuf))) requireFlag = reqBuf;
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Spacing();
        ImGui::TextDisabled("Tracked entities: %d", static_cast<int>(trackedIds_.size()));
    }

    std::string TriggerComponent::getName() const { return "Trigger"; }

    std::unique_ptr<Component> TriggerComponent::clone() const
    {
        auto copy = std::make_unique<TriggerComponent>(*this);
        copy->trackedIds_.clear(); // runtime tracking state is not cloned
        return copy;
    }

    nlohmann::json TriggerComponent::serialize() const
    {
        nlohmann::json j = Component::serialize();
        j["size"]           = {size.x, size.y};
        j["offset"]         = {offset.x, offset.y};
        j["showDebug"]      = showDebug;
        j["setFlagOnEnter"] = setFlagOnEnter;
        j["requireFlag"]    = requireFlag;
        return j;
    }

    void TriggerComponent::deserialize(const nlohmann::json& j)
    {
        Component::deserialize(j); // restore `enabled` (and any future base fields)
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
        if (j.contains("setFlagOnEnter")) setFlagOnEnter = j["setFlagOnEnter"].get<std::string>();
        if (j.contains("requireFlag"))    requireFlag    = j["requireFlag"].get<std::string>();
    }

    ::Rectangle TriggerComponent::getZone() const
    {
        Vector2 gPos = owner->getGlobalPosition();
        return ::Rectangle{
            gPos.x + offset.x - size.x * 0.5f,
            gPos.y + offset.y - size.y * 0.5f,
            size.x,
            size.y
        };
    }
}
