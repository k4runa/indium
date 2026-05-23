#pragma once
namespace Indium
{
    struct Time
    {
        static float scale;   // time multiplier (1.0 = normal, 0.5 = half-speed)
        static float elapsed; // total play time in seconds (reset on Play start)
        static float delta;   // this frame's dt (already scaled)
        // Fixed step dt is Scene::FIXED_TIMESTEP (1/60 s) — use that instead.
    };

    inline float Time::scale   = 1.0f;
    inline float Time::elapsed = 0.0f;
    inline float Time::delta   = 0.0f;
}
