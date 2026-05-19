#pragma once
namespace Indium
{
    struct Time
    {
        static float scale;      // time multiplier (1.0 = normal, 0.5 = half-speed)
        static float elapsed;    // total play time in seconds (reset on Play start)
        static float delta;      // this frame's dt (already scaled)
        static constexpr float fixedDelta = 1.0f / 60.0f;
    };

    inline float Time::scale   = 1.0f;
    inline float Time::elapsed = 0.0f;
    inline float Time::delta   = 0.0f;
}
