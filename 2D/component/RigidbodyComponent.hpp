#pragma once
#include "../../core/Component.hpp"
#include "raylib.h"

namespace Indium
{
    struct RigidbodyComponent : Component
    {
        // --- Body Type ---
        bool  isStatic    = false;   // never moves, gravity disabled
        bool  isKinematic = false;   // script-controlled, collides as solid for others

        // --- Linear Motion ---
        float mass        = 1.0f;
        float gravityScale = 1.0f;
        float linearDrag  = 0.0f;   // velocity decay per second (0 = no drag)
        float bounciness  = 0.0f;   // restitution (0 = inelastic, 1 = perfectly elastic)

        // --- Angular Motion ---
        float angularVelocity = 0.0f;  // degrees/second
        float angularDrag     = 5.0f;  // angular velocity decay (default prevents infinite spin)
        bool  freezeRotation  = false; // lock rotation axis

        // --- Collision Filtering ---
        int collisionLayer = 1;           // which layer this body belongs to (1-32)
        int collisionMask  = 0x7FFFFFFF;  // bitmask of layers to collide with

        // --- Sleep State (runtime only, not serialized) ---
        float sleepTimer_ = 0.0f;
        bool  isSleeping_ = false;

        void update(float dt, Vector2 worldSize, Scene* scene) override {}
        void fixedUpdate(float fixedDt, Vector2 worldSize, Scene* scene) override;
        std::string getName() const override { return "Rigidbody"; }

        // Centralized collision resolver — called once per fixed step by Scene.
        // Iterates all unique entity pairs so each collision is resolved exactly once.
        static void ResolveScene(Scene* scene, float fixedDt);
        std::unique_ptr<Component> clone() const override { return std::make_unique<RigidbodyComponent>(*this); }
        void inspect(std::function<void()> snapshotCb) override;

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["mass"]           = mass;
            j["gravityScale"]   = gravityScale;
            j["bounciness"]     = bounciness;
            j["isStatic"]       = isStatic;
            j["isKinematic"]    = isKinematic;
            j["linearDrag"]     = linearDrag;
            j["angularDrag"]    = angularDrag;
            j["freezeRotation"] = freezeRotation;
            j["collisionLayer"] = collisionLayer;
            j["collisionMask"]  = collisionMask;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("mass"))           mass           = j["mass"];
            if (j.contains("gravityScale"))   gravityScale   = j["gravityScale"];
            if (j.contains("bounciness"))     bounciness     = j["bounciness"];
            if (j.contains("isStatic"))       isStatic       = j["isStatic"];
            if (j.contains("isKinematic"))    isKinematic    = j["isKinematic"];
            if (j.contains("linearDrag"))     linearDrag     = j["linearDrag"];
            if (j.contains("angularDrag"))    angularDrag    = j["angularDrag"];
            if (j.contains("freezeRotation")) freezeRotation = j["freezeRotation"];
            if (j.contains("collisionLayer")) collisionLayer = j["collisionLayer"];
            if (j.contains("collisionMask"))  collisionMask  = j["collisionMask"];
        }
    };
}
