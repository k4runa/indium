#pragma once
#include <string>
#include <cstring>
#include <limits>
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../core/scene/Scene.hpp"
#include "../../core/InputManager.hpp"
#include "../../core/StoryState.hpp"
#include "../../core/DialogueManager.hpp"
#include "../../core/Screen.hpp"
#include "../../core/GUI.hpp"
#include "InteractableComponent.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"

namespace Indium
{
    /**
     * @brief Attach to the player. Each frame it finds the nearest active
     * InteractableComponent whose owner is within that interactable's radius, shows
     * its prompt (screen-space, via onGUI), and triggers it on the Interact action.
     *
     * The Interact action is read from InputManager (action name configurable). If no
     * such action is mapped, it falls back to the E key, so it works out of the box.
     */
    struct PlayerInteractorComponent : Component
    {
        std::string actionName = "Interact";   // InputManager action; the E key always works too
        std::string requireTag = "";           // optional: only look at entities with this tag ("" = any)

        void update(float, Vector2, Scene* scene) override
        {
            target_ = nullptr;
            prompt_.clear();
            if (!scene || !owner) return;
            if (DialogueManager::Get().IsActive()) return;   // suppress while talking

            const Vector2 self = owner->getGlobalPosition();
            float bestD2 = std::numeric_limits<float>::max();
            InteractableComponent* best = nullptr;

            for (const auto& e : scene->entities)
            {
                if (e.get() == owner || !e->activeInHierarchy()) continue;
                if (!requireTag.empty() && e->tag != requireTag) continue;
                auto* it = e->getComponent<InteractableComponent>();
                if (!it || !it->enabled) continue;

                const float d2 = Vector2DistanceSqr(self, e->getGlobalPosition());
                if (d2 <= it->radius * it->radius && d2 < bestD2)
                {
                    bestD2 = d2;
                    best   = it;
                }
            }

            if (!best) return;
            target_ = best;
            prompt_ = best->prompt;

            // E always works; the named action is an additional, rebindable trigger.
            const bool pressed = IsKeyPressed(KEY_E)
                               || (InputManager::Get().HasAction(actionName) && InputManager::Get().IsPressed(actionName));
            if (pressed) Trigger(best);
        }

        void onGUI(Scene*) override
        {
            if (prompt_.empty() || DialogueManager::Get().IsActive()) return;

            const int   size = 20;
            const float tw   = (float)MeasureText(prompt_.c_str(), size);
            const float pad  = 14.0f;
            ::Rectangle box  = { Screen::Width() * 0.5f - tw * 0.5f - pad,
                                 Screen::Height() * 0.72f,
                                 tw + pad * 2.0f,
                                 (float)size + pad };
            GUI::Box(box, Color{ 0, 0, 0, 185 }, Color{ 120, 120, 140, 255 }, 1.0f);
            GUI::LabelCentered(prompt_.c_str(), box, size, RAYWHITE);
        }

        std::string getName() const override { return "PlayerInteractor"; }

        void inspect(std::function<void()> snapshotCb) override
        {
            char buf[128];
            ImGui::TextDisabled("Interact Action (InputManager)");
            strncpy(buf, actionName.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##act", buf, sizeof(buf))) actionName = buf;
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::TextDisabled("The E key always works; the action above is an extra rebindable trigger.");

            ImGui::Spacing();
            ImGui::TextDisabled("Only Look For Tag (optional)");
            char tagBuf[128];
            strncpy(tagBuf, requireTag.c_str(), sizeof(tagBuf) - 1);
            tagBuf[sizeof(tagBuf) - 1] = '\0';
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##reqtag", tagBuf, sizeof(tagBuf))) requireTag = tagBuf;
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::TextDisabled("Blank = any entity that has an Interactable.");

            ImGui::Spacing();
            if (!prompt_.empty()) ImGui::Text("In range: %s", prompt_.c_str());
            else                  ImGui::TextDisabled("No interactable in range.");
        }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<PlayerInteractorComponent>();
            c->actionName = actionName;
            c->requireTag = requireTag;
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["actionName"] = actionName;
            j["requireTag"] = requireTag;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("actionName")) actionName = j["actionName"].get<std::string>();
            if (j.contains("requireTag")) requireTag = j["requireTag"].get<std::string>();
        }

    private:
        InteractableComponent* target_ = nullptr;   // refreshed every update(); not serialized
        std::string            prompt_;

        void Trigger(InteractableComponent* it)
        {
            if (!it) return;
            if (!it->setFlag.empty())
            {
                if (it->toggleFlag) StoryState::Get().Set(it->setFlag, !StoryState::Get().GetBool(it->setFlag));
                else                StoryState::Get().SetFlag(it->setFlag);
            }
            if (!it->eventTag.empty())   Events::Publish(GameEvents::NarrativeEvent{ it->eventTag, it->owner });
            if (!it->dialogueId.empty()) DialogueManager::Get().Start(it->dialogueId);
        }
    };
}
