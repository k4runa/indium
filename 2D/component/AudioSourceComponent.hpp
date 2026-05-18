#pragma once

#include <string>
#include <cstring>
#include <filesystem>
#include <vector>
#include "../../core/Component.hpp"
#include "../../core/ScriptManager.hpp"
#include "raylib.h"
#include "imgui.h"

namespace Indium
{
    /**
     * @brief Plays audio files attached to an entity.
     *
     * Two modes:
     *   - Sound (SFX): in-memory, low-latency, can overlap
     *   - Music (Stream): disk-streamed, suited for long background tracks
     *
     * Script usage:
     *   auto* audio = owner->getComponent<AudioSourceComponent>();
     *   audio->Play();
     *   audio->Stop();
     */
    struct AudioSourceComponent : Component
    {
        std::string filePath    = "";
        float       volume      = 1.0f;
        float       pitch       = 1.0f;
        bool        loop        = false;
        bool        playOnStart = false;
        bool        isMusic     = false; // false=Sound (SFX), true=Music (streaming)

        std::string ResolvePath(const std::string& path) const
        {
            if (path.empty()) return "";
            if (std::filesystem::path(path).is_absolute()) return path;

            std::string projPath = ScriptManager::Get().GetActiveProjectPath();
            if (!projPath.empty())
            {
                return (std::filesystem::path(projPath) / path).string();
            }
            return path;
        }

        void Load(const std::string& path)
        {
            unload_();
            if (path.empty()) return;
            filePath = path;

            std::string resolved = ResolvePath(path);

            if (isMusic)
            {
                music_  = LoadMusicStream(resolved.c_str());
                loaded_ = (music_.stream.buffer != nullptr);
                if (loaded_)
                {
                    music_.looping = loop;
                    SetMusicVolume(music_, volume);
                    SetMusicPitch(music_,  pitch);
                }
            }
            else
            {
                sound_  = LoadSound(resolved.c_str());
                loaded_ = (sound_.stream.buffer != nullptr);
                if (loaded_)
                {
                    SetSoundVolume(sound_, volume);
                    SetSoundPitch(sound_,  pitch);
                }
            }
        }

        void Play()
        {
            if (!loaded_) return;
            if (isMusic)
            {
                music_.looping = loop;
                SetMusicVolume(music_, volume);
                SetMusicPitch(music_,  pitch);
                PlayMusicStream(music_);
            }
            else
            {
                SetSoundVolume(sound_, volume);
                SetSoundPitch(sound_,  pitch);
                PlaySound(sound_);
            }
        }

        void Stop()
        {
            if (!loaded_) return;
            if (isMusic) StopMusicStream(music_);
            else         StopSound(sound_);
        }

        void Pause()
        {
            if (!loaded_) return;
            if (isMusic) PauseMusicStream(music_);
            else         PauseSound(sound_);
        }

        void Resume()
        {
            if (!loaded_) return;
            if (isMusic) ResumeMusicStream(music_);
            else         ResumeSound(sound_);
        }

        bool IsPlaying() const
        {
            if (!loaded_) return false;
            return isMusic ? IsMusicStreamPlaying(music_) : IsSoundPlaying(sound_);
        }

        void start(Scene* /*scene*/) override
        {
            if (!filePath.empty() && !loaded_) Load(filePath);
            if (playOnStart) Play();
        }

        void update(float /*dt*/, Vector2 /*worldSize*/, Scene* /*scene*/) override
        {
            if (loaded_ && isMusic && IsMusicStreamPlaying(music_))
                UpdateMusicStream(music_);
        }

        void destroy(Scene* /*scene*/) override { unload_(); }

        void inspect() override
        {
            ImGui::Text("Audio File Selection (from assets)");

            // --- Scan active project assets folder recursively ---
            std::vector<std::string> audioFiles;
            std::string projPath = ScriptManager::Get().GetActiveProjectPath();

            if (!projPath.empty())
            {
                std::vector<std::string> searchPaths = { projPath + "/Assets", projPath + "/assets" };
                for (const auto& sp : searchPaths)
                {
                    if (std::filesystem::exists(sp))
                    {
                        for (const auto& entry : std::filesystem::recursive_directory_iterator(sp))
                        {
                            if (entry.is_regular_file())
                            {
                                std::string ext = entry.path().extension().string();
                                if (ext == ".wav" || ext == ".mp3" || ext == ".ogg")
                                {
                                    std::string relPath = std::filesystem::relative(entry.path(), projPath).string();
                                    audioFiles.push_back(relPath);
                                }
                            }
                        }
                    }
                }
            }

            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##AudioSelect", filePath.empty() ? "(Choose an audio file)" : filePath.c_str()))
            {
                if (ImGui::Selectable("(None)", filePath.empty()))
                {
                    filePath = "";
                    unload_();
                }

                for (const auto& file : audioFiles)
                {
                    if (ImGui::Selectable(file.c_str(), filePath == file))
                    {
                        filePath = file;
                        Load(filePath);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            // Drag and drop target on selection box
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    std::string droppedPath = (const char*)payload->Data;
                    std::string ext = std::filesystem::path(droppedPath).extension().string();
                    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg")
                    {
                        filePath = droppedPath;
                        Load(filePath);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Or type/edit path manually:");

            char buf[512] = {};
            strncpy(buf, filePath.c_str(), sizeof(buf) - 1);
            ImGui::PushItemWidth(-80);
            if (ImGui::InputText("##AudioPath", buf, sizeof(buf)))
                filePath = buf;
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Load##Audio"))
            {
                if (!filePath.empty()) Load(filePath);
            }

            ImGui::Spacing();

            ImGui::Text("Mode");
            bool wasMus = isMusic;
            if (ImGui::RadioButton("Sound (SFX)",    !isMusic)) isMusic = false;
            ImGui::SameLine();
            if (ImGui::RadioButton("Music (Stream)",  isMusic)) isMusic = true;
            if (wasMus != isMusic && !filePath.empty())
                Load(filePath); // reload with new mode

            ImGui::Spacing();

            ImGui::Text("Volume");
            ImGui::PushItemWidth(-1);
            if (ImGui::SliderFloat("##AudioVol", &volume, 0.0f, 1.0f, "%.2f") && loaded_)
            {
                if (isMusic) SetMusicVolume(music_, volume);
                else         SetSoundVolume(sound_, volume);
            }
            ImGui::PopItemWidth();

            ImGui::Text("Pitch");
            ImGui::PushItemWidth(-1);
            if (ImGui::SliderFloat("##AudioPitch", &pitch, 0.1f, 4.0f, "%.2f") && loaded_)
            {
                if (isMusic) SetMusicPitch(music_, pitch);
                else         SetSoundPitch(sound_, pitch);
            }
            ImGui::PopItemWidth();

            ImGui::Checkbox("Loop",          &loop);
            ImGui::SameLine();
            ImGui::Checkbox("Play On Start", &playOnStart);

            ImGui::Spacing();
            ImGui::Separator();

            if (loaded_)
            {
                bool playing = IsPlaying();
                if (playing)
                {
                    if (ImGui::Button("Stop##AudioCtrl"))  Stop();
                    ImGui::SameLine();
                    if (ImGui::Button("Pause##AudioCtrl")) Pause();
                }
                else
                {
                    if (ImGui::Button("Play##AudioCtrl"))   Play();
                    ImGui::SameLine();
                    if (ImGui::Button("Resume##AudioCtrl")) Resume();
                }
                ImGui::SameLine();
                ImGui::TextDisabled(playing ? "Playing" : "Stopped");
            }
            else
            {
                ImGui::TextDisabled("No audio loaded");
            }
        }

        std::string getName() const override { return "AudioSource"; }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<AudioSourceComponent>();
            c->filePath    = filePath;
            c->volume      = volume;
            c->pitch       = pitch;
            c->loop        = loop;
            c->playOnStart = playOnStart;
            c->isMusic     = isMusic;
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["filePath"]    = filePath;
            j["volume"]      = volume;
            j["pitch"]       = pitch;
            j["loop"]        = loop;
            j["playOnStart"] = playOnStart;
            j["isMusic"]     = isMusic;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("filePath"))    filePath    = j["filePath"].get<std::string>();
            if (j.contains("volume"))      volume      = j["volume"].get<float>();
            if (j.contains("pitch"))       pitch       = j["pitch"].get<float>();
            if (j.contains("loop"))        loop        = j["loop"].get<bool>();
            if (j.contains("playOnStart")) playOnStart = j["playOnStart"].get<bool>();
            if (j.contains("isMusic"))     isMusic     = j["isMusic"].get<bool>();
        }

        ~AudioSourceComponent() override { unload_(); }

    private:
        Sound  sound_  = {};
        Music  music_  = {};
        bool   loaded_ = false;

        void unload_()
        {
            if (!loaded_) return;
            Stop();
            if (isMusic) UnloadMusicStream(music_);
            else         UnloadSound(sound_);
            sound_  = {};
            music_  = {};
            loaded_ = false;
        }
    };
}
