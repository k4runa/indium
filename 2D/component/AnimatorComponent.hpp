#pragma once
#include <map>
#include <string>
#include "../../core/Component.hpp"
#include "raylib.h"
#include "imgui.h"

namespace Indium
{
    /** @brief A single named animation sequence on a sprite sheet. */
    struct Clip
    {
        int   frameWidth  = 32;
        int   frameHeight = 32;
        int   startX      = 0;    // sheet origin X in pixels
        int   startY      = 0;    // sheet origin Y in pixels
        int   frameCount  = 1;
        float fps         = 60.0f;
        bool  loop        = true;
    };

    /**
     * @brief Frame-based sprite sheet animator.
     *
     * Attach to a Sprite entity. The Sprite's update() will query
     * getCurrentSourceRect() each frame and apply it to sourceRec.
     *
     * Script usage:
     *   auto* anim = owner->getComponent<AnimatorComponent>();
     *   anim->Play("walk");
     */
    struct AnimatorComponent : Component
    {
        std::map<std::string, Clip> clips;
        std::string currentClip  = "";
        int         currentFrame = 0;
        float       timer        = 0.0f;
        bool        playing      = true;

        /** @brief Switch to a clip by name. Resets frame and timer. No-op if already playing. */
        void Play(const std::string& clipName)
        {
            if (currentClip == clipName && playing) return;
            if (clips.find(clipName) == clips.end()) return;
            currentClip  = clipName;
            currentFrame = 0;
            timer        = 0.0f;
            playing      = true;
        }

        /** @brief Returns the source rectangle for the current frame on the sprite sheet. */
        [[nodiscard]] ::Rectangle getCurrentSourceRect() const
        {
            if (currentClip.empty()) return {0.0f, 0.0f, 0.0f, 0.0f};
            auto iter = clips.find(currentClip);
            if (iter == clips.end()) return {0.0f, 0.0f, 0.0f, 0.0f};

            const Clip& clip = iter->second;
            return ::Rectangle{
                static_cast<float>(clip.startX + currentFrame * clip.frameWidth),
                static_cast<float>(clip.startY),
                static_cast<float>(clip.frameWidth),
                static_cast<float>(clip.frameHeight)
            };
        }

        void update(float dt, Vector2 /*worldSize*/, Scene* /*scene*/) override
        {
            if (!playing || currentClip.empty()) return;
            auto iter = clips.find(currentClip);
            if (iter == clips.end()) return;

            const Clip& clip = iter->second;
            if (clip.fps <= 0.0f || clip.frameCount <= 1) return;

            timer += dt;
            const float frameDuration = 1.0f / clip.fps;

            if (timer >= frameDuration)
            {
                timer -= frameDuration;
                currentFrame++;

                if (currentFrame >= clip.frameCount)
                {
                    if (clip.loop)
                    {
                        currentFrame = 0;
                    }
                    else
                    {
                        currentFrame = clip.frameCount - 1;
                        playing = false;
                    }
                }
            }
        }

        void inspect() override
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
                    if (iter != clips.end())
                    {
                        ImGui::TextDisabled("Frame %d / %d", currentFrame + 1, iter->second.frameCount);
                    }
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
                    if (ImGui::MenuItem("Delete")) toDelete = name;
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
                    ImGui::PopItemWidth();

                    ImGui::Text("Sheet Origin (px)");
                    ImGui::PushItemWidth(-1);
                    int origin[2] = {clip.startX, clip.startY};
                    if (ImGui::DragInt2("##Origin", origin, 1.0f, 0, 16384))
                    {
                        clip.startX = origin[0];
                        clip.startY = origin[1];
                    }
                    ImGui::PopItemWidth();

                    ImGui::Text("Frames");
                    ImGui::PushItemWidth(-1);
                    ImGui::DragInt("##FrameCount", &clip.frameCount, 1.0f, 1, 256);
                    ImGui::PopItemWidth();

                    ImGui::Text("FPS");
                    ImGui::PushItemWidth(-1);
                    ImGui::DragFloat("##FPS", &clip.fps, 0.5f, 1.0f, 120.0f, "%.1f");
                    ImGui::PopItemWidth();

                    ImGui::Checkbox("Loop", &clip.loop);

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

        std::string getName() const override { return "Animator"; }

        std::unique_ptr<Component> clone() const override
        {
            auto copy = std::make_unique<AnimatorComponent>(*this);
            copy->currentFrame = 0;
            copy->timer        = 0.0f;
            return copy;
        }

        nlohmann::json serialize() const override
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

        void deserialize(const nlohmann::json& j) override
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
    };
}
