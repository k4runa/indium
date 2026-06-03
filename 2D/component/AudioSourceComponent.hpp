#pragma once

#include <string>
#include <cstring>
#include <filesystem>
#include <vector>
#include "../../core/Component.hpp"
#include "../../core/ScriptManager.hpp"
#include "../../core/scene/Scene.hpp"
#include "AudioListenerComponent.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"

// Fallback when this header is compiled outside the editor (e.g. pulled into a
// script library via IndiumEngine.hpp) where the FontAwesome icon header isn't
// included. In the editor build the real macro is already defined, so this is skipped.
#ifndef ICON_FA_TRIANGLE_EXCLAMATION
    #define ICON_FA_TRIANGLE_EXCLAMATION ""
#endif

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

        // --- Spatial Audio ---
        bool        isSpatial   = false;   // enable distance-based volume attenuation
        float       minDistance = 150.0f;  // full volume within this range (world units)
        float       maxDistance = 800.0f;  // silent beyond this range (world units)

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
            loadFailed_    = !loaded_;
            loadedAsMusic_ = isMusic;
        }

        void Play()
        {
            if (!loaded_) return;
            if (loadedAsMusic_)
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
            if (loadedAsMusic_) StopMusicStream(music_);
            else                StopSound(sound_);
        }

        void Pause()
        {
            if (!loaded_) return;
            if (loadedAsMusic_) PauseMusicStream(music_);
            else                PauseSound(sound_);
        }

        void Resume()
        {
            if (!loaded_) return;
            if (loadedAsMusic_) ResumeMusicStream(music_);
            else                ResumeSound(sound_);
        }

        bool IsPlaying() const
        {
            if (!loaded_) return false;
            return loadedAsMusic_ ? IsMusicStreamPlaying(music_) : IsSoundPlaying(sound_);
        }

        void start(Scene* /*scene*/) override
        {
            if (!filePath.empty() && !loaded_) Load(filePath);
            if (playOnStart) Play();
        }

        void update(float /*dt*/, Vector2 /*worldSize*/, Scene* scene) override
        {
            if (loaded_ && loadedAsMusic_ && IsMusicStreamPlaying(music_))
                UpdateMusicStream(music_);

            // --- Spatial volume attenuation ---
            if (isSpatial && loaded_ && scene && owner)
            {
                // Find first active AudioListener in the scene
                Entity* listener = nullptr;
                for (const auto& e : scene->entities)
                {
                    if (e->activeInHierarchy() && e->getComponent<AudioListenerComponent>())
                    { listener = e.get(); break; }
                }

                float effectiveVol = volume;
                if (listener)
                {
                    float dist = Vector2Distance(owner->getGlobalPosition(),
                                                  listener->getGlobalPosition());
                    if (dist <= minDistance)
                        effectiveVol = volume;
                    else if (dist >= maxDistance)
                        effectiveVol = 0.0f;
                    else
                    {
                        float t      = (dist - minDistance) / (maxDistance - minDistance);
                        effectiveVol = volume * (1.0f - t); // linear falloff
                    }
                }

                if (loadedAsMusic_) SetMusicVolume(music_, effectiveVol);
                else                SetSoundVolume(sound_, effectiveVol);
            }
        }

        void destroy(Scene* /*scene*/) override { unload_(); }

        void inspect(std::function<void()> snapshotCb) override
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
            if (ImGui::InputText("##AudioPath", buf, sizeof(buf))) filePath = buf;
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
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::SameLine();
            if (ImGui::RadioButton("Music (Stream)",  isMusic)) isMusic = true;
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            if (wasMus != isMusic && !filePath.empty()) Load(filePath); // reload with new mode

            ImGui::Spacing();

            ImGui::Text("Volume");
            ImGui::PushItemWidth(-1);
            if (ImGui::SliderFloat("##AudioVol", &volume, 0.0f, 1.0f, "%.2f") && loaded_)
            {
                if (loadedAsMusic_) SetMusicVolume(music_, volume);
                else                SetSoundVolume(sound_, volume);
            }
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Text("Pitch");
            ImGui::PushItemWidth(-1);
            if (ImGui::SliderFloat("##AudioPitch", &pitch, 0.1f, 4.0f, "%.2f") && loaded_)
            {
                if (loadedAsMusic_) SetMusicPitch(music_, pitch);
                else                SetSoundPitch(sound_, pitch);
            }
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Checkbox("Loop",          &loop);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::SameLine();
            ImGui::Checkbox("Play On Start", &playOnStart);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Spatial Audio");
            ImGui::Checkbox("Spatial##SpatialToggle", &isSpatial);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            if (isSpatial)
            {
                ImGui::Indent(8.0f);
                ImGui::TextDisabled("Requires an AudioListener in the scene.");
                ImGui::Text("Min Distance");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat("##MinDist", &minDistance, 5.0f, 0.0f, maxDistance - 1.0f, "%.0f");
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
                ImGui::Text("Max Distance");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat("##MaxDist", &maxDistance, 5.0f, minDistance + 1.0f, 50000.0f, "%.0f");
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
                ImGui::Unindent(8.0f);
            }

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
            else if (loadFailed_)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::TextWrapped(ICON_FA_TRIANGLE_EXCLAMATION "  Failed to load: %s", filePath.c_str());
                ImGui::TextWrapped("Check that the file exists and is a supported format (.wav / .mp3 / .ogg).");
                ImGui::PopStyleColor();
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
            c->enabled     = enabled;
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
            j["isSpatial"]   = isSpatial;
            j["minDistance"] = minDistance;
            j["maxDistance"] = maxDistance;
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
            if (j.contains("isSpatial"))   isSpatial   = j["isSpatial"].get<bool>();
            if (j.contains("minDistance")) minDistance = j["minDistance"].get<float>();
            if (j.contains("maxDistance")) maxDistance = j["maxDistance"].get<float>();
        }

        ~AudioSourceComponent() override { unload_(); }

    private:
        Sound  sound_        = {};
        Music  music_        = {};
        bool   loaded_       = false;
        bool   loadFailed_   = false;
        bool   loadedAsMusic_ = false; // mode the live handle was actually loaded as

        void unload_()
        {
            if (!loaded_) return;
            Stop();
            if (loadedAsMusic_) UnloadMusicStream(music_);
            else                UnloadSound(sound_);
            sound_      = {};
            music_      = {};
            loaded_     = false;
            loadFailed_ = false;
        }
    };
}
