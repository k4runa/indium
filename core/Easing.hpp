#pragma once
//
// Easing.hpp — pure easing curves + typed interpolation primitives.
//
// This is a dependency-free building block (no Component / Entity / Scene).
// TweenComponent builds on it today; the planned cutscene timeline can reuse
// the same curves without pulling in any per-entity machinery.
//
// EaseApply() maps a normalised time t in [0, 1] to an eased value. Most curves
// also land exactly on 0 at t=0 and 1 at t=1; the Back/Elastic families overshoot
// in the middle but are still anchored at both endpoints.
//
#include "raylib.h"   // Vector2, Color
#include "raymath.h"  // Lerp, Vector2Lerp, PI
#include <cmath>

namespace Indium
{
    enum class Ease
    {
        Linear,
        InSine,    OutSine,    InOutSine,
        InQuad,    OutQuad,    InOutQuad,
        InCubic,   OutCubic,   InOutCubic,
        InQuart,   OutQuart,   InOutQuart,
        InExpo,    OutExpo,    InOutExpo,
        InBack,    OutBack,    InOutBack,
        InElastic, OutElastic, InOutElastic,
        InBounce,  OutBounce,  InOutBounce,
        Count
    };

    // --- Bounce helper (the out variant is the building block for the others) ---
    inline float Ease_OutBounce(float t)
    {
        constexpr float n1 = 7.5625f;
        constexpr float d1 = 2.75f;
        if (t < 1.0f / d1)      { return n1 * t * t; }
        else if (t < 2.0f / d1) { t -= 1.5f  / d1; return n1 * t * t + 0.75f; }
        else if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
        else                    { t -= 2.625f / d1; return n1 * t * t + 0.984375f; }
    }

    /** @brief Evaluate an easing curve. t is clamped to [0, 1]. */
    inline float EaseApply(Ease e, float t)
    {
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;

        switch (e)
        {
            case Ease::Linear:     return t;

            case Ease::InSine:     return 1.0f - cosf((t * PI) * 0.5f);
            case Ease::OutSine:    return sinf((t * PI) * 0.5f);
            case Ease::InOutSine:  return -(cosf(PI * t) - 1.0f) * 0.5f;

            case Ease::InQuad:     return t * t;
            case Ease::OutQuad:    return 1.0f - (1.0f - t) * (1.0f - t);
            case Ease::InOutQuad:  return t < 0.5f ? 2.0f * t * t
                                                   : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) * 0.5f;

            case Ease::InCubic:    return t * t * t;
            case Ease::OutCubic:   return 1.0f - powf(1.0f - t, 3.0f);
            case Ease::InOutCubic: return t < 0.5f ? 4.0f * t * t * t
                                                   : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) * 0.5f;

            case Ease::InQuart:    return t * t * t * t;
            case Ease::OutQuart:   return 1.0f - powf(1.0f - t, 4.0f);
            case Ease::InOutQuart: return t < 0.5f ? 8.0f * t * t * t * t
                                                   : 1.0f - powf(-2.0f * t + 2.0f, 4.0f) * 0.5f;

            case Ease::InExpo:     return powf(2.0f, 10.0f * t - 10.0f);
            case Ease::OutExpo:    return 1.0f - powf(2.0f, -10.0f * t);
            case Ease::InOutExpo:  return t < 0.5f ? powf(2.0f, 20.0f * t - 10.0f) * 0.5f
                                                   : (2.0f - powf(2.0f, -20.0f * t + 10.0f)) * 0.5f;

            case Ease::InBack:
            {
                constexpr float c1 = 1.70158f;
                constexpr float c3 = c1 + 1.0f;
                return c3 * t * t * t - c1 * t * t;
            }
            case Ease::OutBack:
            {
                constexpr float c1 = 1.70158f;
                constexpr float c3 = c1 + 1.0f;
                return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
            }
            case Ease::InOutBack:
            {
                constexpr float c1 = 1.70158f;
                constexpr float c2 = c1 * 1.525f;
                return t < 0.5f
                    ? (powf(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) * 0.5f
                    : (powf(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) * 0.5f;
            }

            case Ease::InElastic:
            {
                constexpr float c4 = (2.0f * PI) / 3.0f;
                return -powf(2.0f, 10.0f * t - 10.0f) * sinf((t * 10.0f - 10.75f) * c4);
            }
            case Ease::OutElastic:
            {
                constexpr float c4 = (2.0f * PI) / 3.0f;
                return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
            }
            case Ease::InOutElastic:
            {
                constexpr float c5 = (2.0f * PI) / 4.5f;
                return t < 0.5f
                    ? -(powf(2.0f, 20.0f * t - 10.0f) * sinf((20.0f * t - 11.125f) * c5)) * 0.5f
                    :  (powf(2.0f, -20.0f * t + 10.0f) * sinf((20.0f * t - 11.125f) * c5)) * 0.5f + 1.0f;
            }

            case Ease::InBounce:    return 1.0f - Ease_OutBounce(1.0f - t);
            case Ease::OutBounce:   return Ease_OutBounce(t);
            case Ease::InOutBounce: return t < 0.5f
                                        ? (1.0f - Ease_OutBounce(1.0f - 2.0f * t)) * 0.5f
                                        : (1.0f + Ease_OutBounce(2.0f * t - 1.0f)) * 0.5f;

            default:                return t;
        }
    }

    /** @brief Human-readable name for an easing curve (inspector combos, serialization). */
    inline const char* EaseName(Ease e)
    {
        switch (e)
        {
            case Ease::Linear:       return "Linear";
            case Ease::InSine:       return "InSine";
            case Ease::OutSine:      return "OutSine";
            case Ease::InOutSine:    return "InOutSine";
            case Ease::InQuad:       return "InQuad";
            case Ease::OutQuad:      return "OutQuad";
            case Ease::InOutQuad:    return "InOutQuad";
            case Ease::InCubic:      return "InCubic";
            case Ease::OutCubic:     return "OutCubic";
            case Ease::InOutCubic:   return "InOutCubic";
            case Ease::InQuart:      return "InQuart";
            case Ease::OutQuart:     return "OutQuart";
            case Ease::InOutQuart:   return "InOutQuart";
            case Ease::InExpo:       return "InExpo";
            case Ease::OutExpo:      return "OutExpo";
            case Ease::InOutExpo:    return "InOutExpo";
            case Ease::InBack:       return "InBack";
            case Ease::OutBack:      return "OutBack";
            case Ease::InOutBack:    return "InOutBack";
            case Ease::InElastic:    return "InElastic";
            case Ease::OutElastic:   return "OutElastic";
            case Ease::InOutElastic: return "InOutElastic";
            case Ease::InBounce:     return "InBounce";
            case Ease::OutBounce:    return "OutBounce";
            case Ease::InOutBounce:  return "InOutBounce";
            default:                 return "Linear";
        }
    }

    // --- Typed interpolation used by tweens ----------------------------------
    // 'u' is the eased progress (already passed through EaseApply); these just
    // blend the endpoints. Color has no raymath helper, so it is done per-channel.

    inline float   TweenLerp(float a, float b, float u)     { return Lerp(a, b, u); }
    inline Vector2 TweenLerp(Vector2 a, Vector2 b, float u) { return Vector2Lerp(a, b, u); }
    inline Color   TweenLerp(Color a, Color b, float u)
    {
        auto ch = [](unsigned char x, unsigned char y, float t) -> unsigned char
        {
            float v = Lerp((float)x, (float)y, t);
            if (v < 0.0f)   v = 0.0f;
            if (v > 255.0f) v = 255.0f;
            return (unsigned char)(v + 0.5f);
        };
        return Color{ ch(a.r, b.r, u), ch(a.g, b.g, u), ch(a.b, b.b, u), ch(a.a, b.a, u) };
    }
}
