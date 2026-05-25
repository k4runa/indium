#pragma once
#include <string>
#include <cstring>
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "raylib.h"
#include "imgui.h"

namespace Indium
{
    /**
     * @brief Marks an entity as something the player can interact with.
     *
     * Pure data plus an editor gizmo. A PlayerInteractorComponent finds the nearest
     * active Interactable within its `radius`, shows `prompt`, and on the Interact
     * action triggers the effects: set a StoryState flag, start a dialogue, and/or
     * fire a NarrativeEvent. Fills the gap left by TriggerComponent (automatic AABB
     * zones only) with an explicit "press to interact" model.
     */
    struct InteractableComponent : Component
    {
        std::string prompt     = "Press E";   // shown when the player is in range
        float       radius     = 90.0f;       // world-units the player must be within
        std::string setFlag;                  // optional StoryState flag changed on interact
        bool        toggleFlag = false;       // if true, each interact flips setFlag on/off (else just sets it true)
        std::string dialogueId;               // optional dialogue (<project>/dialogue/<id>.json)
        std::string eventTag;                 // optional NarrativeEvent tag published on interact
        bool        showDebug  = true;        // draw the radius gizmo

        void update(float, Vector2, Scene*) override {}

        void draw() const override
        {
            if (!showDebug || !owner) return;
            DrawCircleLinesV(owner->getGlobalPosition(), radius, Color{ 90, 220, 160, 120 });
        }

        std::string getName() const override { return "Interactable"; }

        void inspect(std::function<void()> snapshotCb) override
        {
            char buf[256];
            auto field = [&](const char* label, const char* id, std::string& value)
            {
                ImGui::TextDisabled("%s", label);
                strncpy(buf, value.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText(id, buf, sizeof(buf))) value = buf;
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            };

            field("Prompt", "##prompt", prompt);

            ImGui::TextDisabled("Radius");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##radius", &radius, 1.0f, 1.0f, 4000.0f, "%.0f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            field("Set Flag (on interact)", "##setflag", setFlag);
            ImGui::Checkbox("Toggle flag on/off each interact", &toggleFlag);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            field("Dialogue Id (optional)", "##dlg",     dialogueId);
            field("Event Tag (optional)",   "##evt",     eventTag);

            ImGui::Checkbox("Show radius gizmo", &showDebug);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<InteractableComponent>();
            c->prompt     = prompt;
            c->radius     = radius;
            c->setFlag    = setFlag;
            c->toggleFlag = toggleFlag;
            c->dialogueId = dialogueId;
            c->eventTag   = eventTag;
            c->showDebug  = showDebug;
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["prompt"]     = prompt;
            j["radius"]     = radius;
            j["setFlag"]    = setFlag;
            j["toggleFlag"] = toggleFlag;
            j["dialogueId"] = dialogueId;
            j["eventTag"]   = eventTag;
            j["showDebug"]  = showDebug;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("prompt"))     prompt     = j["prompt"].get<std::string>();
            if (j.contains("radius"))     radius     = j["radius"].get<float>();
            if (j.contains("setFlag"))    setFlag    = j["setFlag"].get<std::string>();
            if (j.contains("toggleFlag")) toggleFlag = j["toggleFlag"].get<bool>();
            if (j.contains("dialogueId")) dialogueId = j["dialogueId"].get<std::string>();
            if (j.contains("eventTag"))   eventTag   = j["eventTag"].get<std::string>();
            if (j.contains("showDebug"))  showDebug  = j["showDebug"].get<bool>();
        }
    };
}
