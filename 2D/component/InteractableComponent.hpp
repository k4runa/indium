#pragma once
#include <string>
#include <cstring>
#include <vector>
#include <filesystem>
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../core/ScriptManager.hpp"
#include "../../core/Screen.hpp"
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
        std::string cutsceneId;               // optional cutscene (<project>/cutscenes/<id>.json)
        std::string eventTag;                 // optional NarrativeEvent tag published on interact
        std::string giveItem;                 // optional item id granted on interact (ItemManager)
        int         giveCount  = 1;           // how many of giveItem to grant
        std::string takeItem;                 // optional item id removed on interact (no-op if player lacks it)
        int         takeCount  = 1;           // how many of takeItem to remove
        bool        lootContainer = false;    // on interact, pour the owner's InventoryComponent into the player inventory
        bool        showDebug  = true;        // draw the radius gizmo

        void update(float, Vector2, Scene*) override {}

        void draw() const override
        {
            if (!showDebug || !owner || !Screen::DebugGizmos()) return;
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

            // --- Dialogue picker: combo populated from <project>/dialogue/*.json,
            //     with a free-text fallback so authors can type ahead of file creation. ---
            ImGui::TextDisabled("Dialogue Id (optional)");
            std::vector<std::string> dialogues;
            {
                const std::string projPath = ScriptManager::Get().GetActiveProjectPath();
                if (!projPath.empty())
                {
                    std::filesystem::path dlgDir = std::filesystem::path(projPath) / "dialogue";
                    std::error_code ec;
                    if (std::filesystem::exists(dlgDir, ec))
                    {
                        for (const auto& entry : std::filesystem::directory_iterator(dlgDir, ec))
                        {
                            if (!entry.is_regular_file()) continue;
                            if (entry.path().extension() != ".json") continue;
                            dialogues.push_back(entry.path().stem().string());
                        }
                    }
                }
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##dlgCombo", dialogueId.empty() ? "(None)" : dialogueId.c_str()))
            {
                if (ImGui::Selectable("(None)", dialogueId.empty()))
                {
                    if (snapshotCb) snapshotCb();
                    dialogueId.clear();
                }
                for (const auto& d : dialogues)
                {
                    bool selected = (d == dialogueId);
                    if (ImGui::Selectable(d.c_str(), selected))
                    {
                        if (snapshotCb) snapshotCb();
                        dialogueId = d;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            // Manual entry for dialogues that don't exist yet (typed ahead of file)
            field("Or type id manually", "##dlg", dialogueId);

            field("Cutscene Id (optional)", "##cut", cutsceneId);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Plays <project>/cutscenes/<id>.json on interact.");

            field("Event Tag (optional)",   "##evt",     eventTag);

            // --- Item grant / cost on interact (ItemManager, items/<id>.json) ---
            ImGui::Spacing();
            field("Give Item (id, optional)", "##giveitem", giveItem);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Item id granted to the player on interact.");
            ImGui::TextDisabled("Give Count");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragInt("##givecount", &giveCount, 0.1f, 1, 9999);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            field("Take Item (id, optional)", "##takeitem", takeItem);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Item id removed from the player on interact.");
            ImGui::TextDisabled("Take Count");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragInt("##takecount", &takeCount, 0.1f, 1, 9999);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            ImGui::Spacing();
            ImGui::Checkbox("Loot container on interact", &lootContainer);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pours this entity's Inventory component into the player on interact.");

            ImGui::Checkbox("Show radius gizmo", &showDebug);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<InteractableComponent>();
            c->enabled    = enabled;
            c->prompt     = prompt;
            c->radius     = radius;
            c->setFlag    = setFlag;
            c->toggleFlag = toggleFlag;
            c->dialogueId = dialogueId;
            c->cutsceneId = cutsceneId;
            c->eventTag   = eventTag;
            c->giveItem   = giveItem;
            c->giveCount  = giveCount;
            c->takeItem   = takeItem;
            c->takeCount  = takeCount;
            c->lootContainer = lootContainer;
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
            j["cutsceneId"] = cutsceneId;
            j["eventTag"]   = eventTag;
            j["giveItem"]   = giveItem;
            j["giveCount"]  = giveCount;
            j["takeItem"]   = takeItem;
            j["takeCount"]  = takeCount;
            j["lootContainer"] = lootContainer;
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
            if (j.contains("cutsceneId")) cutsceneId = j["cutsceneId"].get<std::string>();
            if (j.contains("eventTag"))   eventTag   = j["eventTag"].get<std::string>();
            if (j.contains("giveItem"))   giveItem   = j["giveItem"].get<std::string>();
            if (j.contains("giveCount"))  giveCount  = j["giveCount"].get<int>();
            if (j.contains("takeItem"))   takeItem   = j["takeItem"].get<std::string>();
            if (j.contains("takeCount"))  takeCount  = j["takeCount"].get<int>();
            if (j.contains("lootContainer")) lootContainer = j["lootContainer"].get<bool>();
            if (j.contains("showDebug"))  showDebug  = j["showDebug"].get<bool>();
        }
    };
}
