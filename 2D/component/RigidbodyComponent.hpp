#pragma once
#include "../../core/Component.hpp"
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

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["mass"] = mass;
            j["gravityScale"] = gravityScale;
            j["bounciness"] = bounciness;
            j["isStatic"] = isStatic;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            if (j.contains("mass")) mass = j["mass"];
            if (j.contains("gravityScale")) gravityScale = j["gravityScale"];
            if (j.contains("bounciness")) bounciness = j["bounciness"];
            if (j.contains("isStatic")) isStatic = j["isStatic"];
        }
    };
}
