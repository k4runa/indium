#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../core/Easing.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "imgui.h"
#include <vector>
#include <functional>
#include <memory>
#include <algorithm>

namespace Indium
{
    // --------------------------------------------------------------------
    // TweenComponent
    //
    // Interpolates entity properties (or any float) over time with an easing
    // curve. It is the runtime "tween runner": scripts (and, later, the cutscene
    // timeline) start tweens through it and it ticks them each frame.
    //
    // Lifetime is scoped to the owning entity — when the entity is destroyed the
    // component and all its tweens go with it, so the typed helpers below (which
    // capture `owner`) can never write to freed memory. Runtime state is never
    // serialized and is cleared on clone()/start(), matching Timer / PathFollower.
    //
    // Script usage:
    //   auto* tw = entity->GetComponent<TweenComponent>();   // or AddComponent<>
    //   tw->MoveTo({400, 200}, 0.5f, Ease::OutCubic);
    //   tw->FadeTo(0.0f, 1.0f);
    //   int id = tw->ScaleTo({2,2}, 0.4f, Ease::OutBack);
    //   tw->SetLoop(id, TweenComponent::LoopMode::PingPong);
    // --------------------------------------------------------------------
    struct TweenComponent : Component
    {
        enum class LoopMode { Once, Loop, PingPong };

        // --- Low-level primitive: drives `apply(easedProgress)` over `dur` seconds.
        //     Returns a handle usable with Stop / SetLoop / OnComplete. --------
        int Add(std::function<void(float)> apply, float dur,
                Ease ease = Ease::OutQuad, float delay = 0.0f)
        {
            Tween t;
            t.apply    = std::move(apply);
            t.duration = (dur > 0.0001f) ? dur : 0.0001f;
            t.ease     = ease;
            t.delay    = delay > 0.0f ? delay : 0.0f;
            t.id       = nextId_++;
            tweens_.push_back(std::move(t));
            return tweens_.back().id;
        }

        // --- Safe typed helpers (capture `owner`; the start value is sampled
        //     lazily on the tween's first active frame so chained tweens compose). ---
        int MoveTo(Vector2 target, float dur, Ease ease = Ease::OutQuad)
        {
            if (!owner) return -1;
            Entity* o = owner;
            auto from = std::make_shared<Vector2>(); auto cap = std::make_shared<bool>(false);
            return Add([=](float u){ if (!*cap){ *from = o->position; *cap = true; }
                                     o->position = TweenLerp(*from, target, u); }, dur, ease);
        }

        int MoveBy(Vector2 delta, float dur, Ease ease = Ease::OutQuad)
        {
            if (!owner) return -1;
            Entity* o = owner;
            auto from = std::make_shared<Vector2>(); auto cap = std::make_shared<bool>(false);
            return Add([=](float u){ if (!*cap){ *from = o->position; *cap = true; }
                                     o->position = TweenLerp(*from, Vector2{from->x + delta.x, from->y + delta.y}, u); }, dur, ease);
        }

        int ScaleTo(Vector2 target, float dur, Ease ease = Ease::OutQuad)
        {
            if (!owner) return -1;
            Entity* o = owner;
            auto from = std::make_shared<Vector2>(); auto cap = std::make_shared<bool>(false);
            return Add([=](float u){ if (!*cap){ *from = o->scale; *cap = true; }
                                     o->scale = TweenLerp(*from, target, u); }, dur, ease);
        }

        int RotateTo(float degrees, float dur, Ease ease = Ease::OutQuad)
        {
            if (!owner) return -1;
            Entity* o = owner;
            auto from = std::make_shared<float>(); auto cap = std::make_shared<bool>(false);
            return Add([=](float u){ if (!*cap){ *from = o->rotation; *cap = true; }
                                     o->rotation = TweenLerp(*from, degrees, u); }, dur, ease);
        }

        int ColorTo(Color target, float dur, Ease ease = Ease::OutQuad)
        {
            if (!owner) return -1;
            Entity* o = owner;
            auto from = std::make_shared<Color>(); auto cap = std::make_shared<bool>(false);
            return Add([=](float u){ if (!*cap){ *from = o->color; *cap = true; }
                                     o->color = TweenLerp(*from, target, u); }, dur, ease);
        }

        /** @brief Tween the owner's alpha. `alpha01` is 0..1 (mapped to color.a 0..255). */
        int FadeTo(float alpha01, float dur, Ease ease = Ease::OutQuad)
        {
            if (!owner) return -1;
            Entity* o = owner;
            float targetA = alpha01 * 255.0f;
            auto from = std::make_shared<float>(); auto cap = std::make_shared<bool>(false);
            return Add([=](float u){ if (!*cap){ *from = (float)o->color.a; *cap = true; }
                                     float v = TweenLerp(*from, targetA, u);
                                     o->color.a = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v + 0.5f); }, dur, ease);
        }

        /** @brief Generic float tween. `field` must outlive the tween — use for a
         *  member of this entity or one of its components, not a stack temporary. */
        int TweenFloat(float* field, float to, float dur, Ease ease = Ease::OutQuad)
        {
            if (!field) return -1;
            auto from = std::make_shared<float>(); auto cap = std::make_shared<bool>(false);
            return Add([=](float u){ if (!*cap){ *from = *field; *cap = true; }
                                     *field = TweenLerp(*from, to, u); }, dur, ease);
        }

        // --- Control -----------------------------------------------------------
        void StopAll() { tweens_.clear(); }
        bool Stop(int id)
        {
            for (auto it = tweens_.begin(); it != tweens_.end(); ++it)
                if (it->id == id) { tweens_.erase(it); return true; }
            return false;
        }
        TweenComponent& SetLoop(int id, LoopMode m)                   { if (Tween* t = find_(id)) t->loop = m;                 return *this; }
        TweenComponent& OnComplete(int id, std::function<void()> cb)  { if (Tween* t = find_(id)) t->onComplete = std::move(cb); return *this; }
        [[nodiscard]] bool IsTweening()   const { return !tweens_.empty(); }
        [[nodiscard]] int  ActiveCount()  const { return (int)tweens_.size(); }

        // --- Engine hooks ------------------------------------------------------
        void start(Scene*) override { tweens_.clear(); nextId_ = 1; }

        void update(float dt, Vector2, Scene*) override
        {
            if (tweens_.empty()) return;

            // onComplete callbacks are collected here and fired only AFTER the tick
            // loop and the compaction below. A callback may Add() / Stop() / StopAll()
            // tweens — or even remove this component — and doing any of that mid-
            // iteration would realloc or shrink `tweens_` under us (use-after-free or
            // out-of-bounds read). Deferring keeps the container stable while we walk
            // it; tweens a callback starts are appended afterwards and so first tick
            // next frame, exactly as before. (apply() never touches `tweens_`, so the
            // loop body itself can't invalidate the iteration.)
            std::vector<std::function<void()>> completed;

            for (Tween& t : tweens_)
            {
                if (t.done) continue;

                float step = dt;
                if (t.delay > 0.0f)
                {
                    t.delay -= step;
                    if (t.delay > 0.0f) continue;
                    step = -t.delay;       // spend the leftover after the delay ends
                    t.delay = 0.0f;
                }

                t.elapsed += step;

                float p;
                bool finished = false;
                if (t.loop == LoopMode::Once)
                {
                    p = t.elapsed / t.duration;
                    if (p >= 1.0f) { p = 1.0f; finished = true; }
                }
                else if (t.loop == LoopMode::Loop)
                {
                    p = fmodf(t.elapsed, t.duration) / t.duration;
                }
                else // PingPong: 0->1->0 across 2*duration
                {
                    float two = 2.0f * t.duration;
                    float c   = fmodf(t.elapsed, two);
                    p = (c <= t.duration) ? (c / t.duration) : (2.0f - c / t.duration);
                }

                if (t.apply) t.apply(EaseApply(t.ease, p));

                if (finished)
                {
                    t.done = true;
                    if (t.onComplete) completed.push_back(t.onComplete);
                }
            }

            tweens_.erase(std::remove_if(tweens_.begin(), tweens_.end(),
                          [](const Tween& t){ return t.done; }), tweens_.end());

            for (auto& cb : completed) cb();
        }

        void inspect(std::function<void()>) override
        {
            ImGui::TextDisabled("Tweens are started from scripts / the timeline.");
            ImGui::Spacing();
            ImGui::Text("Active: %d", ActiveCount());

            // Manual verification affordance — animates only during Play (edit
            // mode does not tick components).
            if (ImGui::Button("Pulse Scale (test)", ImVec2(-1, 0)) && owner)
            {
                int id = ScaleTo({ owner->scale.x * 1.3f, owner->scale.y * 1.3f }, 0.4f, Ease::OutBack);
                SetLoop(id, LoopMode::PingPong);
            }
        }

        std::string getName() const override { return "Tween"; }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<TweenComponent>();
            c->enabled = enabled;     // runtime tweens intentionally not copied
            return c;
        }

        // Runtime-only component: nothing to persist beyond the base `enabled`.
        nlohmann::json serialize() const override { return Component::serialize(); }
        void deserialize(const nlohmann::json& j) override { Component::deserialize(j); }

    private:
        struct Tween
        {
            std::function<void(float)> apply;
            std::function<void()>      onComplete;
            float    delay    = 0.0f;
            float    duration = 0.3f;
            float    elapsed  = 0.0f;
            Ease     ease     = Ease::OutQuad;
            LoopMode loop     = LoopMode::Once;
            bool     done     = false;
            int      id       = 0;
        };

        Tween* find_(int id)
        {
            for (auto& t : tweens_) if (t.id == id) return &t;
            return nullptr;
        }

        std::vector<Tween> tweens_;
        int                nextId_ = 1;
    };
}
