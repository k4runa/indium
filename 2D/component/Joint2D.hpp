#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"
#include <string>
#include <cmath>
#include <algorithm>

namespace Indium
{
    // ====================================================================
    // DistanceJoint2D
    //
    // Keeps two entities at a fixed distance (or at most a fixed distance
    // when maxDistanceOnly = true, giving "rope" behaviour).
    //
    // Attach to one entity and set connectedEntityName to the *name* of
    // the other entity in the scene.  Both entities should have a
    // RigidbodyComponent; the constraint distributes correction
    // proportionally to their masses.  If an entity has no Rigidbody it is
    // treated as infinitely massive (its position is not modified).
    // ====================================================================
    struct DistanceJoint2D : Component
    {
        // --- Configuration ---
        std::string connectedEntityName;        // name of the other entity
        float       distance        = 100.0f;   // target distance (world units)
        bool        maxDistanceOnly = false;    // true = rope (no push, only pull)
        float       dampingRatio    = 0.2f;     // 0 = rigid, ~0.5–0.7 = soft
        Vector2     anchor          = {0, 0};   // local offset on THIS entity
        Vector2     connectedAnchor = {0, 0};   // local offset on the OTHER entity
        bool        showDebug       = true;

        // --- Script-facing helper ---
        void Connect(Entity* e) { connectedEntityName = e ? e->name : ""; }

        // --- Lifecycle ---
        void update(float, Vector2, Scene*) override {}
        void fixedUpdate(float fixedDt, Vector2 worldSize, Scene* scene) override;
        void draw() const override;

        void inspect(std::function<void()> snapshotCb) override;

        std::string getName() const override { return "DistanceJoint2D"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<DistanceJoint2D>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j          = Component::serialize();
            j["connectedEntityName"]  = connectedEntityName;
            j["distance"]             = distance;
            j["maxDistanceOnly"]      = maxDistanceOnly;
            j["dampingRatio"]         = dampingRatio;
            j["anchor"]               = {anchor.x, anchor.y};
            j["connectedAnchor"]      = {connectedAnchor.x, connectedAnchor.y};
            j["showDebug"]            = showDebug;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("connectedEntityName")) connectedEntityName = j["connectedEntityName"].get<std::string>();
            if (j.contains("distance"))            distance            = j["distance"].get<float>();
            if (j.contains("maxDistanceOnly"))     maxDistanceOnly     = j["maxDistanceOnly"].get<bool>();
            if (j.contains("dampingRatio"))        dampingRatio        = j["dampingRatio"].get<float>();
            if (j.contains("showDebug"))           showDebug           = j["showDebug"].get<bool>();
            if (j.contains("anchor"))
            { anchor.x = j["anchor"][0]; anchor.y = j["anchor"][1]; }
            if (j.contains("connectedAnchor"))
            { connectedAnchor.x = j["connectedAnchor"][0]; connectedAnchor.y = j["connectedAnchor"][1]; }
        }
    };

    // ====================================================================
    // HingeJoint2D
    //
    // Constrains two entities to share a common pivot point.  The owner
    // entity can rotate freely around that pivot (like a door hinge or
    // pendulum).  Optional angle limits and a motor are supported.
    //
    // Both anchor points are expressed in each entity's local space.
    // Set connectedEntityName = "" to pin the joint to a fixed world point
    // (the connected anchor becomes a world-space position).
    // ====================================================================
    struct HingeJoint2D : Component
    {
        // --- Configuration ---
        std::string connectedEntityName;        // name of connected entity ("" = world anchor)
        Vector2     anchor          = {0, 0};   // local pivot on THIS entity
        Vector2     connectedAnchor = {0, 0};   // local pivot on OTHER entity (or world pos)

        // Angle limits (applied to owner's rotation relative to connected entity)
        bool        useLimits   = false;
        float       minAngle    = -45.0f;       // degrees
        float       maxAngle    =  45.0f;       // degrees

        // Motor — drives angular velocity toward motorSpeed
        bool        useMotor    = false;
        float       motorSpeed  = 90.0f;        // degrees / second
        float       maxTorque   = 100.0f;       // maximum angular impulse per step

        bool        showDebug   = true;

        // --- Script-facing helper ---
        void Connect(Entity* e) { connectedEntityName = e ? e->name : ""; }

        // --- Lifecycle ---
        void update(float, Vector2, Scene*) override {}
        void fixedUpdate(float fixedDt, Vector2 worldSize, Scene* scene) override;
        void draw() const override;

        void inspect(std::function<void()> snapshotCb) override;

        std::string getName() const override { return "HingeJoint2D"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<HingeJoint2D>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j         = Component::serialize();
            j["connectedEntityName"] = connectedEntityName;
            j["anchor"]              = {anchor.x, anchor.y};
            j["connectedAnchor"]     = {connectedAnchor.x, connectedAnchor.y};
            j["useLimits"]           = useLimits;
            j["minAngle"]            = minAngle;
            j["maxAngle"]            = maxAngle;
            j["useMotor"]            = useMotor;
            j["motorSpeed"]          = motorSpeed;
            j["maxTorque"]           = maxTorque;
            j["showDebug"]           = showDebug;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("connectedEntityName")) connectedEntityName = j["connectedEntityName"].get<std::string>();
            if (j.contains("anchor"))
            { anchor.x = j["anchor"][0]; anchor.y = j["anchor"][1]; }
            if (j.contains("connectedAnchor"))
            { connectedAnchor.x = j["connectedAnchor"][0]; connectedAnchor.y = j["connectedAnchor"][1]; }
            if (j.contains("useLimits"))   useLimits   = j["useLimits"].get<bool>();
            if (j.contains("minAngle"))    minAngle    = j["minAngle"].get<float>();
            if (j.contains("maxAngle"))    maxAngle    = j["maxAngle"].get<float>();
            if (j.contains("useMotor"))    useMotor    = j["useMotor"].get<bool>();
            if (j.contains("motorSpeed"))  motorSpeed  = j["motorSpeed"].get<float>();
            if (j.contains("maxTorque"))   maxTorque   = j["maxTorque"].get<float>();
            if (j.contains("showDebug"))   showDebug   = j["showDebug"].get<bool>();
        }
    };

    // ====================================================================
    // SpringJoint2D
    //
    // Soft connection between two entities: a damped spring (Hooke's law)
    // that pulls them toward `restLength` apart instead of rigidly locking
    // the distance.  Great for bouncy ropes, grappling hooks, suspension,
    // and trailing objects.
    //
    // Force = -stiffness * (currentDist - restLength)  along the axis,
    // minus a damping term proportional to the relative velocity.  Force is
    // distributed between the two bodies by inverse mass (a static / missing
    // Rigidbody acts as an immovable anchor).
    // ====================================================================
    struct SpringJoint2D : Component
    {
        std::string connectedEntityName;        // name of the other entity ("" = world anchor)
        Vector2     anchor          = {0, 0};   // local offset on THIS entity
        Vector2     connectedAnchor = {0, 0};   // local offset on OTHER entity (or world pos)
        float       restLength      = 100.0f;   // natural spring length
        float       stiffness       = 40.0f;    // spring constant (higher = stiffer)
        float       damping         = 2.0f;     // velocity damping (higher = less bounce)
        bool        showDebug       = true;

        // --- Script-facing helper ---
        void Connect(Entity* e) { connectedEntityName = e ? e->name : ""; }

        void update(float, Vector2, Scene*) override {}
        void fixedUpdate(float fixedDt, Vector2 worldSize, Scene* scene) override;
        void draw() const override;

        void inspect(std::function<void()> snapshotCb) override;

        std::string getName() const override { return "SpringJoint2D"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<SpringJoint2D>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j         = Component::serialize();
            j["connectedEntityName"] = connectedEntityName;
            j["anchor"]              = {anchor.x, anchor.y};
            j["connectedAnchor"]     = {connectedAnchor.x, connectedAnchor.y};
            j["restLength"]          = restLength;
            j["stiffness"]           = stiffness;
            j["damping"]             = damping;
            j["showDebug"]           = showDebug;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("connectedEntityName")) connectedEntityName = j["connectedEntityName"].get<std::string>();
            if (j.contains("anchor"))
            { anchor.x = j["anchor"][0]; anchor.y = j["anchor"][1]; }
            if (j.contains("connectedAnchor"))
            { connectedAnchor.x = j["connectedAnchor"][0]; connectedAnchor.y = j["connectedAnchor"][1]; }
            if (j.contains("restLength")) restLength = j["restLength"].get<float>();
            if (j.contains("stiffness"))  stiffness  = j["stiffness"].get<float>();
            if (j.contains("damping"))    damping    = j["damping"].get<float>();
            if (j.contains("showDebug"))  showDebug  = j["showDebug"].get<bool>();
        }
    };
}
