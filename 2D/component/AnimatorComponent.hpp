#pragma once
#include <map>
#include <string>
#include "../../core/Component.hpp"
#include "raylib.h"

namespace Indium
{
    /** @brief A single named animation sequence on a sprite sheet. */
    struct Clip
    {
        int   frameWidth  = 32;
        int   frameHeight = 32;
        int   startX      = 0;
        int   startY      = 0;
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

        void update(float dt, Vector2 worldSize, Scene* scene) override;
        void inspect(std::function<void()> snapshotCb) override;
        std::string getName() const override;
        std::unique_ptr<Component> clone() const override;
        nlohmann::json serialize() const override;
        void deserialize(const nlohmann::json& j) override;
    };
}
