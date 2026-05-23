#pragma once
#include <vector>
#include "../../core/Component.hpp"
#include "raylib.h"

namespace Indium
{
    /**
     * @brief CPU particle system: emits, simulates, and draws 2D particles.
     *
     * Attach to any entity. The emitter spawns at the owner's world position.
     * Color and size lerp from start → end values over each particle's lifetime.
     *
     * Script usage:
     *   auto* ps = owner->getComponent<ParticleSystemComponent>();
     *   ps->Play();   // start emitting
     *   ps->Stop();   // stop emitting (existing particles finish their life)
     *   ps->Clear();  // kill all particles immediately
     */
    struct ParticleSystemComponent : Component
    {
        // ---- Emission ----
        float emissionRate  = 20.0f;   // particles / second
        int   maxParticles  = 200;
        bool  loop          = true;
        bool  playOnStart   = true;
        bool  playing       = false;

        // ---- Shape ----
        enum class EmitterShape { Point, Circle, Rectangle };
        EmitterShape shape      = EmitterShape::Point;
        float        shapeRadius = 20.0f;
        Vector2      shapeSize   = {40.0f, 40.0f};

        // ---- Particle life ----
        float lifetimeMin = 0.8f;
        float lifetimeMax = 1.5f;

        // ---- Velocity ----
        float directionAngle = -90.0f;  // degrees, -90 = up
        float spreadAngle    = 30.0f;   // ± cone half-width in degrees
        float speedMin       = 80.0f;
        float speedMax       = 160.0f;

        // ---- Physics ----
        float gravityScale = 0.0f;
        float damping      = 0.0f;   // velocity damping per second (0 = none)

        // ---- Appearance ----
        Color   startColor = {255, 200, 60,  255};
        Color   endColor   = {255, 50,  0,   0  };
        float   startSize  = 8.0f;
        float   endSize    = 0.0f;

        // ---- Controls (hot path / scripts) ----
        void Play()  { playing = true; }
        void Stop()  { playing = false; }
        void Clear() { particles_.clear(); emitAccum_ = 0.0f; }

        // ---- Component interface ----
        void start(Scene*) override { if (playOnStart) Play(); }
        void update(float dt, Vector2 worldSize, Scene* scene) override;
        void draw() const override;
        void inspect(std::function<void()> snapshotCb = {}) override;
        std::string getName() const override;
        std::unique_ptr<Component> clone() const override;
        nlohmann::json serialize() const override;
        void deserialize(const nlohmann::json& j) override;

    private:
        struct Particle
        {
            Vector2 position  = {0, 0};
            Vector2 velocity  = {0, 0};
            float   age       = 0.0f;
            float   lifetime  = 1.0f;
        };

        std::vector<Particle> particles_;
        float emitAccum_ = 0.0f;

        void spawnOne_();
        static Color lerpColor_(Color a, Color b, float t);
    };
}
