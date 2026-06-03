#include "Joint2D.hpp"
#include "RigidbodyComponent.hpp"
#include "../../core/scene/Scene.hpp"
#include "../../core/Entity.hpp"
#include "../../include/extras/IconsFontAwesome6.h"

#include <cmath>
#include <algorithm>

namespace Indium
{
    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    // Rotate a local-space offset by an entity's world rotation.
    static Vector2 rotateLocal(Vector2 local, float degrees)
    {
        float rad = degrees * DEG2RAD;
        float c   = cosf(rad), s = sinf(rad);
        return { local.x * c - local.y * s,
                 local.x * s + local.y * c };
    }

    // World-space anchor position for an entity + local offset.
    static Vector2 worldAnchor(const Entity* e, Vector2 localOffset)
    {
        Vector2 rot = rotateLocal(localOffset, e->getGlobalRotation());
        return { e->getGlobalPosition().x + rot.x,
                 e->getGlobalPosition().y + rot.y };
    }

    // Find first entity in the scene whose name matches (case-sensitive).
    static Entity* findByName(Scene* scene, const std::string& name)
    {
        if (name.empty()) return nullptr;
        for (const auto& e : scene->entities)
            if (e->name == name) return e.get();
        return nullptr;
    }

    // Inverse mass: 0 if static / kinematic (immovable), else 1/mass.
    static float invMass(Entity* e)
    {
        auto* rb = e->getComponent<RigidbodyComponent>();
        if (!rb || rb->isStatic || rb->isKinematic) return 0.0f;
        return 1.0f / std::max(rb->mass, 0.001f);
    }

    // -----------------------------------------------------------------------
    // DistanceJoint2D
    // -----------------------------------------------------------------------

    void DistanceJoint2D::fixedUpdate(float /*fixedDt*/, Vector2 /*worldSize*/, Scene* scene)
    {
        if (!owner || !scene || !enabled) return;

        Entity* other = findByName(scene, connectedEntityName);
        if (!other || other == owner) return;

        // World-space anchor positions
        Vector2 wA = worldAnchor(owner, anchor);
        Vector2 wB = worldAnchor(other, connectedAnchor);

        Vector2 delta = { wB.x - wA.x, wB.y - wA.y };
        float   dist  = Vector2Length(delta);
        if (dist < 0.001f) return;

        float error = dist - distance;               // positive = too far
        if (maxDistanceOnly && error <= 0.0f) return; // rope: only when taut

        Vector2 dir   = Vector2Scale(delta, 1.0f / dist);
        float   invA  = invMass(owner);
        float   invB  = invMass(other);
        float   total = invA + invB;
        if (total < 0.0001f) return;

        // Baumgarte positional correction split by mass ratio
        float corrMag = error * (1.0f - dampingRatio);

        if (invA > 0.0f)
        {
            float share = corrMag * (invA / total);
            owner->position.x += dir.x * share;
            owner->position.y += dir.y * share;

            // Damp the relative velocity along the constraint axis
            auto* rb = owner->getComponent<RigidbodyComponent>();
            if (rb)
            {
                float vd = Vector2DotProduct(owner->velocity, dir);
                owner->velocity.x -= dir.x * vd * dampingRatio;
                owner->velocity.y -= dir.y * vd * dampingRatio;
            }
        }

        if (invB > 0.0f)
        {
            float share = corrMag * (invB / total);
            other->position.x -= dir.x * share;
            other->position.y -= dir.y * share;

            auto* rb = other->getComponent<RigidbodyComponent>();
            if (rb)
            {
                float vd = Vector2DotProduct(other->velocity, dir);
                other->velocity.x += dir.x * vd * dampingRatio;
                other->velocity.y += dir.y * vd * dampingRatio;
            }
        }
    }

    void DistanceJoint2D::draw() const
    {
        if (!showDebug || !owner) return;

        // Draw anchor dot on owner
        Vector2 wA = worldAnchor(owner, anchor);
        DrawCircleV(wA, 4.0f, Color{255, 220, 50, 220});

        // Draw the target-distance ring around the owner anchor
        DrawCircleLinesV(wA, distance, Color{255, 220, 50, 60});
    }

    void DistanceJoint2D::inspect(std::function<void()> snapshotCb)
    {
        ImGui::Text("Connected Entity");
        char buf[128] = {};
        strncpy(buf, connectedEntityName.c_str(), sizeof(buf) - 1);
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##DJTarget", buf, sizeof(buf)))
        {
            if (snapshotCb) snapshotCb();
            connectedEntityName = buf;
        }
        ImGui::PopItemWidth();
        ImGui::TextDisabled("Type the exact entity name.");
        ImGui::Spacing();

        ImGui::Text("Distance");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##DJDist", &distance, 1.0f, 0.0f, 10000.0f, "%.1f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Checkbox("Max Distance Only (Rope)", &maxDistanceOnly);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

        ImGui::Text("Damping Ratio");
        ImGui::PushItemWidth(-1);
        ImGui::SliderFloat("##DJDamp", &dampingRatio, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Separator();
        ImGui::Text("Anchor (local)");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat2("##DJAnchor", &anchor.x, 1.0f);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Connected Anchor (local)");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat2("##DJCAnchor", &connectedAnchor.x, 1.0f);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Separator();
        ImGui::Checkbox("Show Debug", &showDebug);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
    }

    // -----------------------------------------------------------------------
    // HingeJoint2D
    // -----------------------------------------------------------------------

    void HingeJoint2D::fixedUpdate(float fixedDt, Vector2 /*worldSize*/, Scene* scene)
    {
        if (!owner || !scene || !enabled) return;

        // --- Positional constraint: keep both anchors at the same world point ---
        Entity* other = findByName(scene, connectedEntityName);

        Vector2 wA = worldAnchor(owner, anchor);
        Vector2 wB;

        if (other && other != owner)
        {
            wB = worldAnchor(other, connectedAnchor);
        }
        else
        {
            // No connected entity → pin to the connectedAnchor as a fixed world point
            wB = connectedAnchor;
        }

        Vector2 error = { wA.x - wB.x, wA.y - wB.y };
        float   errLen = Vector2Length(error);

        if (errLen > 0.01f)
        {
            float invA = invMass(owner);
            float invB = other ? invMass(other) : 0.0f;
            float total = invA + invB;
            if (total > 0.0001f)
            {
                if (invA > 0.0f)
                {
                    float share = invA / total;
                    owner->position.x -= error.x * share;
                    owner->position.y -= error.y * share;
                }
                if (other && invB > 0.0f)
                {
                    float share = invB / total;
                    other->position.x += error.x * share;
                    other->position.y += error.y * share;
                }
            }
        }

        // --- Motor: drive owner's angular velocity toward motorSpeed ---
        if (useMotor)
        {
            auto* rb = owner->getComponent<RigidbodyComponent>();
            if (rb && !rb->isStatic && !rb->freezeRotation)
            {
                float deltaAV = motorSpeed - rb->angularVelocity;
                float impulse = std::clamp(deltaAV * rb->mass, -maxTorque * fixedDt, maxTorque * fixedDt);
                rb->angularVelocity += impulse / std::max(rb->mass, 0.001f);
            }
        }

        // --- Angle limits: clamp owner rotation relative to connected entity ---
        if (useLimits)
        {
            float refAngle = other ? other->rotation : 0.0f;
            float relAngle = owner->rotation - refAngle;

            // Wrap to [-180, 180]
            while (relAngle >  180.0f) relAngle -= 360.0f;
            while (relAngle < -180.0f) relAngle += 360.0f;

            auto* rb = owner->getComponent<RigidbodyComponent>();

            if (relAngle < minAngle)
            {
                owner->rotation = refAngle + minAngle;
                if (rb && rb->angularVelocity < 0.0f) rb->angularVelocity = 0.0f;
            }
            else if (relAngle > maxAngle)
            {
                owner->rotation = refAngle + maxAngle;
                if (rb && rb->angularVelocity > 0.0f) rb->angularVelocity = 0.0f;
            }
        }
    }

    void HingeJoint2D::draw() const
    {
        if (!showDebug || !owner) return;

        // Draw pivot dot at the owner's world anchor
        Vector2 wA = worldAnchor(owner, anchor);
        DrawCircleV(wA, 5.0f, Color{100, 200, 255, 230});
        DrawCircleLinesV(wA, 5.0f, Color{255, 255, 255, 180});

        // If limits are active, draw the allowed arc
        if (useLimits)
        {
            float refAngle = 0.0f; // visual only, no scene access in draw()
            DrawCircleSector(wA,
                30.0f / 1.0f,
                refAngle + minAngle - 90.0f,
                refAngle + maxAngle - 90.0f,
                8,
                Color{100, 200, 255, 30});
        }
    }

    void HingeJoint2D::inspect(std::function<void()> snapshotCb)
    {
        ImGui::Text("Connected Entity");
        char buf[128] = {};
        strncpy(buf, connectedEntityName.c_str(), sizeof(buf) - 1);
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##HJTarget", buf, sizeof(buf)))
        {
            if (snapshotCb) snapshotCb();
            connectedEntityName = buf;
        }
        ImGui::PopItemWidth();
        ImGui::TextDisabled("Type the exact entity name.\nLeave empty to pin to world.");
        ImGui::Spacing();

        ImGui::Text("Anchor (local)");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat2("##HJAnchor", &anchor.x, 1.0f);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Connected Anchor (local / world)");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat2("##HJCAnchor", &connectedAnchor.x, 1.0f);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Separator();

        // Angle limits
        ImGui::Checkbox("Use Angle Limits", &useLimits);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        if (useLimits)
        {
            ImGui::Indent(8.0f);
            ImGui::Text("Min Angle");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##HJMin", &minAngle, 0.5f, -180.0f, 0.0f, "%.1f°");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Text("Max Angle");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##HJMax", &maxAngle, 0.5f, 0.0f, 180.0f, "%.1f°");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Unindent(8.0f);
        }

        ImGui::Separator();

        // Motor
        ImGui::Checkbox("Use Motor", &useMotor);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        if (useMotor)
        {
            ImGui::Indent(8.0f);
            ImGui::Text("Motor Speed (°/s)");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##HJSpeed", &motorSpeed, 1.0f, -3600.0f, 3600.0f, "%.1f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Text("Max Torque");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##HJTorque", &maxTorque, 1.0f, 0.0f, 100000.0f, "%.1f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Unindent(8.0f);
        }

        ImGui::Separator();
        ImGui::Checkbox("Show Debug", &showDebug);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
    }

    // -----------------------------------------------------------------------
    // SpringJoint2D
    // -----------------------------------------------------------------------

    void SpringJoint2D::fixedUpdate(float fixedDt, Vector2 /*worldSize*/, Scene* scene)
    {
        if (!owner || !scene || !enabled || fixedDt <= 0.0f) return;

        Entity* other = findByName(scene, connectedEntityName);

        Vector2 wA = worldAnchor(owner, anchor);
        Vector2 wB = (other && other != owner) ? worldAnchor(other, connectedAnchor)
                                                : connectedAnchor; // world anchor fallback

        Vector2 delta = { wB.x - wA.x, wB.y - wA.y };
        float   dist  = Vector2Length(delta);
        if (dist < 0.0001f) return;

        Vector2 dir = Vector2Scale(delta, 1.0f / dist);

        // Hooke's law: F = -k * displacement  (positive pulls A toward B)
        float displacement = dist - restLength;
        float springForce  = stiffness * displacement;

        // Damping along the spring axis using relative velocity
        Vector2 relVel = { owner->velocity.x - (other ? other->velocity.x : 0.0f),
                           owner->velocity.y - (other ? other->velocity.y : 0.0f) };
        float   relVelAlong = Vector2DotProduct(relVel, dir);
        float   dampForce   = damping * relVelAlong;

        float   totalForce  = springForce - dampForce;
        Vector2 force       = Vector2Scale(dir, totalForce);

        float invA  = invMass(owner);
        float invB  = other ? invMass(other) : 0.0f;
        float total = invA + invB;
        if (total < 0.0001f) return;

        // Apply as impulses scaled by inverse mass (heavier bodies move less)
        if (invA > 0.0f)
        {
            owner->velocity.x += force.x * invA * fixedDt;
            owner->velocity.y += force.y * invA * fixedDt;
        }
        if (other && invB > 0.0f)
        {
            other->velocity.x -= force.x * invB * fixedDt;
            other->velocity.y -= force.y * invB * fixedDt;
        }
    }

    void SpringJoint2D::draw() const
    {
        if (!showDebug || !owner) return;

        Vector2 wA = worldAnchor(owner, anchor);
        // Draw a simple zig-zag spring line toward the connected anchor's world pos.
        // In the editor we only know the owner anchor reliably, so draw the rest-length
        // ring as a hint plus the anchor dot.
        DrawCircleV(wA, 4.0f, Color{180, 255, 120, 220});
        DrawCircleLinesV(wA, restLength, Color{180, 255, 120, 50});
    }

    void SpringJoint2D::inspect(std::function<void()> snapshotCb)
    {
        ImGui::Text("Connected Entity");
        char buf[128] = {};
        strncpy(buf, connectedEntityName.c_str(), sizeof(buf) - 1);
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##SJTarget", buf, sizeof(buf)))
        {
            if (snapshotCb) snapshotCb();
            connectedEntityName = buf;
        }
        ImGui::PopItemWidth();
        ImGui::TextDisabled("Type the exact entity name.\nLeave empty to anchor to world.");
        ImGui::Spacing();

        ImGui::Text("Rest Length");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##SJRest", &restLength, 1.0f, 0.0f, 10000.0f, "%.1f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Stiffness");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##SJStiff", &stiffness, 0.5f, 0.0f, 10000.0f, "%.1f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Damping");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##SJDamp", &damping, 0.1f, 0.0f, 1000.0f, "%.2f");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Separator();
        ImGui::Text("Anchor (local)");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat2("##SJAnchor", &anchor.x, 1.0f);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Connected Anchor (local / world)");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat2("##SJCAnchor", &connectedAnchor.x, 1.0f);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Separator();
        ImGui::Checkbox("Show Debug", &showDebug);
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
    }
}
