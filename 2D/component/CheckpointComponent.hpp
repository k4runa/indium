#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../core/scene/Scene.hpp"
#include "../../core/StoryState.hpp"
#include "../../core/SaveManager.hpp"
#include "../../core/EventBus.hpp"
#include "../../core/events/GameEvents.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "imgui.h"
#include <string>
#include <cstring>

namespace Indium
{
    // --------------------------------------------------------------------
    // CheckpointComponent
    //
    // A zone that "activates" when an entity with the target tag enters it.
    // On activation it can:
    //   - set a story flag (so respawn / progress logic can read it)
    //   - auto-save to a SaveManager slot
    //   - publish a NarrativeEvent (quests / scripts can react)
    //
    // Activates once by default. Records the last activated checkpoint's
    // world position in StoryState ("<flag>_x" / "<flag>_y") so a respawn
    // system can read where to put the player.
    // --------------------------------------------------------------------
    struct CheckpointComponent : Component
    {
        Vector2     size       = { 100.0f, 150.0f };
        Vector2     offset     = { 0.0f, 0.0f };
        std::string targetTag  = "Player";
        std::string setFlag    = "checkpoint";  // story flag set on reach ("" = none)
        int         saveSlot   = SaveManager::kAutosaveSlot; // 0 = autosave slot, -1 = don't auto-save
        bool        once       = true;          // only fire the first time
        bool        showDebug  = true;

        bool activated_ = false;                // runtime

        ::Rectangle Zone() const
        {
            Vector2 p = owner ? owner->getGlobalPosition() : Vector2{0,0};
            return { p.x + offset.x - size.x * 0.5f, p.y + offset.y - size.y * 0.5f, size.x, size.y };
        }

        void start(Scene*) override { activated_ = false; }

        void update(float, Vector2, Scene* scene) override
        {
            if (!owner || !scene) return;
            if (once && activated_) return;

            ::Rectangle zone = Zone();
            bool hit = false;
            for (const auto& e : scene->entities)
            {
                if (e.get() == owner || !e->activeInHierarchy()) continue;
                if (!targetTag.empty() && e->tag != targetTag) continue;
                if (CheckCollisionRecs(zone, e->getBounds())) { hit = true; break; }
            }

            if (hit && !activated_)
            {
                activated_ = true;
                if (!setFlag.empty())
                {
                    StoryState::Get().SetFlag(setFlag);
                    Vector2 p = owner->getGlobalPosition();
                    StoryState::Get().Set(setFlag + "_x", StoryValue{ p.x });
                    StoryState::Get().Set(setFlag + "_y", StoryValue{ p.y });
                }
                if (saveSlot >= 0) SaveManager::Save(*scene, saveSlot);
                Events::Publish(GameEvents::NarrativeEvent{ setFlag.empty() ? "checkpoint" : setFlag, owner });
            }
            else if (!hit && !once)
            {
                activated_ = false; // allow re-trigger when re-entered
            }
        }

        void draw() const override
        {
            if (!showDebug || !owner) return;
            ::Rectangle z = Zone();
            Color col = activated_ ? Color{ 80, 255, 120, 180 } : Color{ 255, 210, 70, 160 };
            DrawRectangleLinesEx(z, 2.0f, col);
            DrawText(activated_ ? "checkpoint*" : "checkpoint",
                     (int)z.x + 4, (int)z.y + 4, 10, col);
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::Text("Size");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##CPSize", &size.x, 1.0f, 1.0f, 100000.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Text("Offset");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##CPOff", &offset.x, 1.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Separator();
            char tagBuf[64] = {};
            strncpy(tagBuf, targetTag.c_str(), sizeof(tagBuf) - 1);
            ImGui::Text("Target Tag");
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##CPTag", tagBuf, sizeof(tagBuf))) { if (snapshotCb) snapshotCb(); targetTag = tagBuf; }
            ImGui::PopItemWidth();

            char flagBuf[64] = {};
            strncpy(flagBuf, setFlag.c_str(), sizeof(flagBuf) - 1);
            ImGui::Text("Set Flag");
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##CPFlag", flagBuf, sizeof(flagBuf))) { if (snapshotCb) snapshotCb(); setFlag = flagBuf; }
            ImGui::PopItemWidth();

            ImGui::Text("Auto-Save Slot (0 = autosave, -1 = off)");
            ImGui::PushItemWidth(-1);
            ImGui::DragInt("##CPSlot", &saveSlot, 0.1f, -1, 32);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Checkbox("Trigger Once", &once);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::SameLine();
            ImGui::Checkbox("Show Debug", &showDebug);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            if (activated_) ImGui::TextColored(ImVec4(0.4f,1,0.5f,1), "Activated");
        }

        std::string getName() const override { return "Checkpoint"; }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<CheckpointComponent>(*this);
            c->activated_ = false;
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["size"]      = { size.x, size.y };
            j["offset"]    = { offset.x, offset.y };
            j["targetTag"] = targetTag;
            j["setFlag"]   = setFlag;
            j["saveSlot"]  = saveSlot;
            j["once"]      = once;
            j["showDebug"] = showDebug;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("size"))      { size.x = j["size"][0]; size.y = j["size"][1]; }
            if (j.contains("offset"))    { offset.x = j["offset"][0]; offset.y = j["offset"][1]; }
            if (j.contains("targetTag")) targetTag = j["targetTag"].get<std::string>();
            if (j.contains("setFlag"))   setFlag   = j["setFlag"].get<std::string>();
            if (j.contains("saveSlot"))  saveSlot  = j["saveSlot"].get<int>();
            if (j.contains("once"))      once      = j["once"].get<bool>();
            if (j.contains("showDebug")) showDebug = j["showDebug"].get<bool>();
        }
    };
}
