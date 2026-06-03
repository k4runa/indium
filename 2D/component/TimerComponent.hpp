#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../core/EventBus.hpp"
#include "../../core/events/GameEvents.hpp"
#include "../../core/StoryState.hpp"
#include "../../include/nlohmann/json.hpp"
#include "imgui.h"
#include <string>
#include <cstring>

namespace Indium
{
    // --------------------------------------------------------------------
    // TimerComponent
    //
    // A countdown that fires when it reaches zero.  On completion it can:
    //   - publish a NarrativeEvent with `eventTag` (scripts/quests can react)
    //   - set a story flag (`setFlagOnComplete`)
    // and optionally repeat.
    //
    // Scripts can also poll/control it:
    //   auto* t = owner->getComponent<TimerComponent>();
    //   if (t->IsFinished()) { ... }
    //   t->Restart();
    // --------------------------------------------------------------------
    struct TimerComponent : Component
    {
        float       duration         = 3.0f;   // seconds
        bool        loop             = false;  // restart automatically on complete
        bool        startOnPlay      = true;   // begin counting when Play starts
        std::string eventTag;                  // NarrativeEvent tag fired on complete
        std::string setFlagOnComplete;         // story flag set on complete

        // --- Runtime state (not serialized) ---
        float elapsed_   = 0.0f;
        bool  running_   = false;
        bool  finished_  = false;

        // --- Script-facing controls ---
        void Restart()  { elapsed_ = 0.0f; running_ = true;  finished_ = false; }
        void Pause()    { running_ = false; }
        void Resume()   { if (!finished_) running_ = true; }
        void Stop()     { running_ = false; elapsed_ = 0.0f; finished_ = false; }
        [[nodiscard]] bool  IsRunning()  const { return running_; }
        [[nodiscard]] bool  IsFinished() const { return finished_; }
        [[nodiscard]] float Remaining()  const { return duration - elapsed_ > 0.0f ? duration - elapsed_ : 0.0f; }
        [[nodiscard]] float Progress()   const { return duration > 0.0f ? (elapsed_ / duration) : 1.0f; }

        void start(Scene*) override
        {
            elapsed_  = 0.0f;
            finished_ = false;
            running_  = startOnPlay;
        }

        void update(float dt, Vector2, Scene*) override
        {
            if (!running_) return;

            elapsed_ += dt;
            if (elapsed_ >= duration)
            {
                fire_();
                if (loop) { elapsed_ = 0.0f; finished_ = false; }
                else      { running_ = false; finished_ = true; elapsed_ = duration; }
            }
        }

        void fire_()
        {
            if (!eventTag.empty())
                Events::Publish(GameEvents::NarrativeEvent{ eventTag, owner });
            if (!setFlagOnComplete.empty())
                StoryState::Get().SetFlag(setFlagOnComplete);
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::Text("Duration (s)");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##TimerDur", &duration, 0.05f, 0.0f, 100000.0f, "%.2f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Checkbox("Loop", &loop);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::SameLine();
            ImGui::Checkbox("Start On Play", &startOnPlay);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            ImGui::Separator();
            ImGui::TextDisabled("On Complete");

            char evtBuf[64] = {};
            strncpy(evtBuf, eventTag.c_str(), sizeof(evtBuf) - 1);
            ImGui::Text("Event Tag");
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##TimerEvt", evtBuf, sizeof(evtBuf))) { if (snapshotCb) snapshotCb(); eventTag = evtBuf; }
            ImGui::PopItemWidth();

            char flagBuf[64] = {};
            strncpy(flagBuf, setFlagOnComplete.c_str(), sizeof(flagBuf) - 1);
            ImGui::Text("Set Flag");
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##TimerFlag", flagBuf, sizeof(flagBuf))) { if (snapshotCb) snapshotCb(); setFlagOnComplete = flagBuf; }
            ImGui::PopItemWidth();

            // Live readout during Play
            if (running_ || finished_)
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::ProgressBar(Progress(), ImVec2(-1, 0),
                                   finished_ ? "Finished" : nullptr);
                if (!finished_) ImGui::TextDisabled("Remaining: %.2fs", Remaining());
            }
        }

        std::string getName() const override { return "Timer"; }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<TimerComponent>(*this);
            c->elapsed_  = 0.0f;
            c->running_  = false;
            c->finished_ = false;
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j        = Component::serialize();
            j["duration"]           = duration;
            j["loop"]               = loop;
            j["startOnPlay"]        = startOnPlay;
            j["eventTag"]           = eventTag;
            j["setFlagOnComplete"]  = setFlagOnComplete;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("duration"))          duration          = j["duration"].get<float>();
            if (j.contains("loop"))              loop              = j["loop"].get<bool>();
            if (j.contains("startOnPlay"))       startOnPlay       = j["startOnPlay"].get<bool>();
            if (j.contains("eventTag"))          eventTag          = j["eventTag"].get<std::string>();
            if (j.contains("setFlagOnComplete")) setFlagOnComplete = j["setFlagOnComplete"].get<std::string>();
        }
    };
}
