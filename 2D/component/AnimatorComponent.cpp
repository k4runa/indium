#include "AnimatorComponent.hpp"

namespace Indium
{
    void AnimatorComponent::update(float dt, Vector2, Scene*)
    {
        if (!playing || currentClip.empty()) return;
        auto iter = clips.find(currentClip);
        if (iter == clips.end()) return;

        const Clip& clip = iter->second;
        if (clip.fps <= 0.0f || clip.frameCount <= 1) return;

        timer += dt;
        const float frameDuration = 1.0f / clip.fps;

        while (timer >= frameDuration)
        {
            timer -= frameDuration;
            currentFrame++;

            if (currentFrame >= clip.frameCount)
            {
                if (clip.loop) { currentFrame = 0; }
                else           { currentFrame = clip.frameCount - 1; playing = false; break; }
            }
        }
    }

    void AnimatorComponent::inspect(std::function<void()> snapshotCb)
    {
        // --- Playback controls ---
        if (!clips.empty())
        {
            ImGui::PushItemWidth(-80);
            if (ImGui::BeginCombo("##AnimClip", currentClip.empty() ? "(none)" : currentClip.c_str()))
            {
                for (auto& [name, clip] : clips)
                {
                    bool selected = (name == currentClip);
                    if (ImGui::Selectable(name.c_str(), selected)) Play(name);
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();

            if (playing)
            {
                if (ImGui::Button("||##AnimPause", ImVec2(36, 0))) playing = false;
            }
            else
            {
                if (ImGui::Button(">##AnimPlay", ImVec2(36, 0))) playing = true;
            }

            if (!currentClip.empty())
            {
                auto iter = clips.find(currentClip);
                if (iter != clips.end()) ImGui::TextDisabled("Frame %d / %d", currentFrame + 1, iter->second.frameCount);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        // --- Add clip ---
        static char newClipName[64] = "idle";
        ImGui::PushItemWidth(-80);
        ImGui::InputText("##NewClipName", newClipName, sizeof(newClipName));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("+ Clip", ImVec2(60, 0)))
        {
            if (newClipName[0] != '\0' && clips.find(newClipName) == clips.end())
            {
                if (snapshotCb) snapshotCb();
                clips[newClipName] = Clip{};
                if (currentClip.empty()) Play(newClipName);
            }
        }

        ImGui::Spacing();

        // --- Clip list ---
        std::string toDelete;
        for (auto& [name, clip] : clips)
        {
            ImGui::PushID(name.c_str());

            bool open = ImGui::CollapsingHeader(name.c_str());

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Delete"))
                {
                    if (snapshotCb) snapshotCb();
                    toDelete = name;
                }
                ImGui::EndPopup();
            }

            if (open)
            {
                ImGui::Indent(8.0f);

                ImGui::Text("Frame Size");
                ImGui::PushItemWidth(-1);
                int frameSize[2] = {clip.frameWidth, clip.frameHeight};
                if (ImGui::DragInt2("##FrameSize", frameSize, 1.0f, 1, 4096))
                {
                    clip.frameWidth  = frameSize[0];
                    clip.frameHeight = frameSize[1];
                }
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();

                ImGui::Text("Sheet Origin (px)");
                ImGui::PushItemWidth(-1);
                int origin[2] = {clip.startX, clip.startY};
                if (ImGui::DragInt2("##Origin", origin, 1.0f, 0, 16384))
                {
                    clip.startX = origin[0];
                    clip.startY = origin[1];
                }
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();

                ImGui::Text("Frames");
                ImGui::PushItemWidth(-1);
                ImGui::DragInt("##FrameCount", &clip.frameCount, 1.0f, 1, 256);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();

                ImGui::Text("FPS");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat("##FPS", &clip.fps, 0.5f, 1.0f, 120.0f, "%.1f");
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();

                ImGui::Checkbox("Loop", &clip.loop);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

                ImGui::Unindent(8.0f);
            }

            ImGui::PopID();
        }

        if (!toDelete.empty())
        {
            clips.erase(toDelete);
            if (currentClip == toDelete)
            {
                currentClip  = clips.empty() ? "" : clips.begin()->first;
                currentFrame = 0;
                timer        = 0.0f;
                playing      = !currentClip.empty();
            }
        }
    }

    std::string AnimatorComponent::getName() const { return "Animator"; }

    std::unique_ptr<Component> AnimatorComponent::clone() const
    {
        auto copy = std::make_unique<AnimatorComponent>(*this);
        copy->currentFrame = 0;
        copy->timer        = 0.0f;
        return copy;
    }

    nlohmann::json AnimatorComponent::serialize() const
    {
        nlohmann::json j = Component::serialize();
        j["currentClip"] = currentClip;

        nlohmann::json clipsJson = nlohmann::json::object();
        for (const auto& [name, clip] : clips)
        {
            clipsJson[name] = {
                {"frameWidth",  clip.frameWidth},
                {"frameHeight", clip.frameHeight},
                {"startX",      clip.startX},
                {"startY",      clip.startY},
                {"frameCount",  clip.frameCount},
                {"fps",         clip.fps},
                {"loop",        clip.loop}
            };
        }
        j["clips"] = clipsJson;
        return j;
    }

    void AnimatorComponent::deserialize(const nlohmann::json& j)
    {
        if (j.contains("currentClip")) currentClip = j["currentClip"].get<std::string>();
        if (j.contains("clips"))
        {
            for (auto& [name, cj] : j["clips"].items())
            {
                Clip clip;
                if (cj.contains("frameWidth"))  clip.frameWidth  = cj["frameWidth"].get<int>();
                if (cj.contains("frameHeight")) clip.frameHeight = cj["frameHeight"].get<int>();
                if (cj.contains("startX"))      clip.startX      = cj["startX"].get<int>();
                if (cj.contains("startY"))      clip.startY      = cj["startY"].get<int>();
                if (cj.contains("frameCount"))  clip.frameCount  = cj["frameCount"].get<int>();
                if (cj.contains("fps"))         clip.fps         = cj["fps"].get<float>();
                if (cj.contains("loop"))        clip.loop        = cj["loop"].get<bool>();
                clips[name] = clip;
            }
        }
    }
}
