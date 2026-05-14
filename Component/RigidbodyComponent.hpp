#pragma once
#include "Component.hpp"
#include "raylib.h"

namespace Indium
{
    struct RigidbodyComponent : Component
    {
        float mass         = 1.0f;
        float gravityScale = 1.0f;
        float bounciness   = 0.0f;
        bool  isStatic     = false;

        void update(float dt, Vector2 worldSize, Scene* scene) override;
        std::string getName() const override { return "Rigidbody"; }
        std::unique_ptr<Component> clone() const override { return std::make_unique<RigidbodyComponent>(*this); }
        void inspect() override;
    };
}
