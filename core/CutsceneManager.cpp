/**********************************************************************************************
*
*   CutsceneManager.cpp — runtime sampling + triggering for the timeline sequencer.
*
*   The trigger-facing API and the JSON round-trip are inline in CutsceneManager.hpp so
*   scripts can Play()/Stop() a cutscene by id. The per-frame work that actually binds
*   tracks to scene entities lives here, where it can pull in the component headers it
*   drives without leaking them into every compiled script.
*
**********************************************************************************************/

#include "CutsceneManager.hpp"

#include <cmath>

#include "scene/Scene.hpp"
#include "Entity.hpp"
#include "StoryState.hpp"
#include "EventBus.hpp"
#include "events/GameEvents.hpp"
#include "DialogueManager.hpp"
#include "Time.hpp"
#include "../2D/component/CameraComponent.hpp"
#include "../2D/component/AudioSourceComponent.hpp"
#include "../2D/component/AnimatorComponent.hpp"
#include "../2D/component/ParticleSystemComponent.hpp"

namespace Indium
{
    namespace
    {
        // Resolve a track's bound entity. A Camera track with an empty target binds the
        // scene's primary camera (falling back to any camera); otherwise we match by tag
        // or by name. Resolved every use rather than cached, so destroying a bound entity
        // mid-cutscene degrades gracefully instead of dangling.
        Entity* resolveTarget(Scene* scene, const CutsceneTrack& t)
        {
            if (!scene) return nullptr;

            if (t.type == CutsceneTrackType::Camera && t.target.empty())
            {
                Entity* anyCam = nullptr;
                for (const auto& e : scene->entities)
                    if (auto* cam = e->getComponent<CameraComponent>())
                    {
                        if (cam->isPrimary) return e.get();
                        if (!anyCam) anyCam = e.get();
                    }
                return anyCam;
            }

            if (t.targetIsTag) return scene->FindWithTag(t.target);

            for (const auto& e : scene->entities)
                if (e->name == t.target) return e.get();
            return nullptr;
        }

        // Sample an interpolated track (assumed sorted, non-empty) at time t, easing the
        // segment by its earlier key's curve. Clamps to the first/last key outside range.
        CutsceneKey sampleKeys(const std::vector<CutsceneKey>& keys, float t)
        {
            if (t <= keys.front().time) return keys.front();
            if (t >= keys.back().time)  return keys.back();

            for (std::size_t i = 0; i + 1 < keys.size(); ++i)
            {
                const CutsceneKey& a = keys[i];
                const CutsceneKey& b = keys[i + 1];
                if (t < a.time || t > b.time) continue;

                const float span = b.time - a.time;
                const float u    = (span > 1e-6f) ? (t - a.time) / span : 0.0f;
                const float e    = CutsceneManager::ApplyEasing(u, a.easing);

                CutsceneKey r;
                r.time  = t;
                r.pos   = { a.pos.x + (b.pos.x - a.pos.x) * e, a.pos.y + (b.pos.y - a.pos.y) * e };
                r.rot   = a.rot + (b.rot - a.rot) * e;
                r.scale = { a.scale.x + (b.scale.x - a.scale.x) * e, a.scale.y + (b.scale.y - a.scale.y) * e };
                r.zoom  = a.zoom + (b.zoom - a.zoom) * e;
                return r;
            }
            return keys.back();
        }

        // Write a sampled keyframe onto the bound entity, honoring channel toggles.
        void applyInterpolated(const CutsceneTrack& t, Entity* e, const CutsceneKey& k)
        {
            if (!e) return;
            if (t.type == CutsceneTrackType::Transform)
            {
                if (t.animatePosition) e->position = k.pos;
                if (t.animateRotation) e->rotation = k.rot;
                if (t.animateScale)    e->scale    = k.scale;
            }
            else if (t.type == CutsceneTrackType::Camera)
            {
                e->position = k.pos;   // the camera entity's position is its look-at point
                if (auto* cam = e->getComponent<CameraComponent>())
                {
                    cam->SetZoom(k.zoom);    // sets zoom + target so the smoothing can't lag behind us
                    cam->baseRotation = k.rot;
                }
            }
        }

        // Run one trigger event against the scene.
        void fireEvent(Scene* scene, const CutsceneTrack& tr, const CutsceneEvent& ev)
        {
            switch (tr.type)
            {
                case CutsceneTrackType::Dialogue:
                    if (!ev.a.empty()) DialogueManager::Get().Start(ev.a);
                    break;

                case CutsceneTrackType::StoryFlag:
                    if (!ev.a.empty())
                    {
                        if (ev.b == "false") StoryState::Get().ClearFlag(ev.a);
                        else                 StoryState::Get().SetFlag(ev.a);
                    }
                    break;

                case CutsceneTrackType::Event:
                    if (!ev.a.empty()) Events::Publish(GameEvents::NarrativeEvent{ ev.a, nullptr });
                    break;

                case CutsceneTrackType::Audio:
                    if (Entity* e = resolveTarget(scene, tr))
                        if (auto* a = e->getComponent<AudioSourceComponent>())
                        {
                            if (ev.a == "stop") a->Stop();
                            else                a->Play();
                        }
                    break;

                case CutsceneTrackType::Animation:
                    if (Entity* e = resolveTarget(scene, tr))
                        if (auto* an = e->getComponent<AnimatorComponent>())
                            an->Play(ev.a);
                    break;

                case CutsceneTrackType::Activation:
                    if (Entity* e = resolveTarget(scene, tr))
                        e->setActive(ev.a != "hide");
                    break;

                case CutsceneTrackType::Particle:
                    if (Entity* e = resolveTarget(scene, tr))
                        if (auto* ps = e->getComponent<ParticleSystemComponent>())
                        {
                            if (ev.a == "stop") ps->Stop();
                            else                ps->Play();
                        }
                    break;

                default: break;   // interpolated tracks have no events
            }
        }
    } // namespace

    // ── Public ──────────────────────────────────────────────────────────────────

    void CutsceneManager::SampleCutscene(const Cutscene& cs, float t, Scene* scene)
    {
        if (!scene) return;
        for (const auto& tr : cs.tracks)
        {
            if (tr.muted || !tr.isInterpolated() || tr.keys.empty()) continue;
            Entity* e = resolveTarget(scene, tr);
            if (!e) continue;

            // Tolerate unsorted authoring order: sample on a time-sorted local copy.
            std::vector<CutsceneKey> keys = tr.keys;
            std::sort(keys.begin(), keys.end(), [](const CutsceneKey& a, const CutsceneKey& b) { return a.time < b.time; });
            applyInterpolated(tr, e, sampleKeys(keys, t));
        }
    }

    void CutsceneManager::SampleAt(float t, Scene* scene)
    {
        // current_ is already time-sorted (PlayCutscene sorts), but SampleCutscene
        // re-sorts a local copy cheaply; share one path.
        SampleCutscene(current_, t, scene);
    }

    void CutsceneManager::Update(float dt, Scene* scene)
    {
        if (!active_ || !scene) return;

        if (pendingStart_) { begin_(scene); pendingStart_ = false; }

        // Skip: snap to the end, firing only the triggers flagged fireOnSkip so story
        // state stays consistent (audio / dialogue cues are dropped).
        if (skipRequested_)
        {
            fireWindow_(scene, prevTime_, current_.duration, /*skipping=*/true);
            SampleAt(current_.duration, scene);
            prevTime_ = time_ = current_.duration;
            skipRequested_ = false;
            finish_(scene);
            return;
        }

        // Paused: hold the current pose without advancing or firing.
        if (paused_) { SampleAt(time_, scene); return; }

        prevTime_ = time_;
        time_    += dt;

        if (time_ >= current_.duration)
        {
            if (current_.loop)
            {
                // Finish this pass, wrap the overshoot, then play the head of the next pass.
                fireWindow_(scene, prevTime_, current_.duration, false);
                const float over = time_ - current_.duration;
                time_     = (current_.duration > 0.0f) ? std::fmod(over, current_.duration) : 0.0f;
                prevTime_ = 0.0f;
                fireWindow_(scene, -1.0f, time_, false);   // (-1, time_] re-fires any t<=0 openers too
                SampleAt(time_, scene);
                return;
            }

            time_ = current_.duration;
            fireWindow_(scene, prevTime_, time_, false);
            SampleAt(time_, scene);
            finish_(scene);
            return;
        }

        fireWindow_(scene, prevTime_, time_, false);
        SampleAt(time_, scene);
    }

    // ── Scene-touching internals ─────────────────────────────────────────────────

    void CutsceneManager::begin_(Scene* scene)
    {
        savedCameraFollow_.clear();
        didPauseGameplay_ = false;

        // Camera tracks take authority over the camera: park follow and remember it so a
        // mid-game cutscene can hand control back when it ends.
        for (auto& tr : current_.tracks)
        {
            if (tr.type != CutsceneTrackType::Camera || tr.muted) continue;
            if (Entity* e = resolveTarget(scene, tr))
                if (auto* cam = e->getComponent<CameraComponent>())
                {
                    savedCameraFollow_.emplace_back(e->id, cam->followEnabled);
                    cam->followEnabled = false;
                }
        }

        if (current_.pausesGameplay)
        {
            savedTimeScale_   = Time::scale;
            Time::scale       = 0.0f;     // CutsceneManager ticks on unscaled dt, so it keeps playing
            didPauseGameplay_ = true;
        }

        SampleAt(0.0f, scene);                   // establish the opening pose
        fireWindow_(scene, -1.0f, 0.0f, false);  // fire any t<=0 openers (e.g. music at 0)
    }

    void CutsceneManager::finish_(Scene* scene)
    {
        for (auto& [id, follow] : savedCameraFollow_)
            if (Entity* e = scene ? scene->FindEntity(id) : nullptr)
                if (auto* cam = e->getComponent<CameraComponent>())
                    cam->followEnabled = follow;
        savedCameraFollow_.clear();

        if (didPauseGameplay_) { Time::scale = savedTimeScale_; didPauseGameplay_ = false; }

        // Mirror TimerComponent: a completion can set a flag and/or publish a beat.
        if (!current_.onCompleteFlag.empty())  StoryState::Get().SetFlag(current_.onCompleteFlag);
        if (!current_.onCompleteEvent.empty()) Events::Publish(GameEvents::NarrativeEvent{ current_.onCompleteEvent, nullptr });

        active_       = false;
        paused_       = false;
        pendingStart_ = false;
    }

    void CutsceneManager::fireWindow_(Scene* scene, float from, float to, bool skipping)
    {
        for (auto& tr : current_.tracks)
        {
            if (tr.muted || tr.isInterpolated()) continue;
            for (const auto& ev : tr.events)
            {
                if (ev.time <= from || ev.time > to) continue;   // crossed into (from, to] this frame
                if (skipping && !ev.fireOnSkip) continue;
                fireEvent(scene, tr, ev);
            }
        }
    }
}
