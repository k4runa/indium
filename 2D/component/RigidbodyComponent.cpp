#include "RigidbodyComponent.hpp"
#include "../../core/scene/Scene.hpp"
#include "../../core/Entity.hpp"
#include "../entity/Circle.hpp"

namespace Indium
{
    static bool checkCollisionSAT(const std::vector<Vector2>& p1, const std::vector<Vector2>& p2, Vector2& outNormal, float& outOverlap) {
        outOverlap = INFINITY;

        for (int shape = 0; shape < 2; shape++) {
            const auto& polygon = (shape == 0) ? p1 : p2;

            for (size_t a = 0; a < polygon.size(); a++) {
                size_t b = (a + 1) % polygon.size();

                Vector2 edge = Vector2Subtract(polygon[b], polygon[a]);
                Vector2 axis = Vector2Normalize({-edge.y, edge.x});

                float minA = INFINITY, maxA = -INFINITY;
                for (const auto& p : p1) {
                    float proj = Vector2DotProduct(p, axis);
                    minA = fminf(minA, proj); maxA = fmaxf(maxA, proj);
                }

                float minB = INFINITY, maxB = -INFINITY;
                for (const auto& p : p2) {
                    float proj = Vector2DotProduct(p, axis);
                    minB = fminf(minB, proj); maxB = fmaxf(maxB, proj);
                }

                if (minA >= maxB || minB >= maxA) return false;

                float axisOverlap = fminf(maxA, maxB) - fmaxf(minA, minB);
                if (axisOverlap < outOverlap) {
                    outOverlap = axisOverlap;
                    outNormal = axis;
                }
            }
        }
        return true;
    }

    static bool checkCollisionCirclePolygon(const Vector2& center, float radius, const std::vector<Vector2>& polygon, Vector2& outNormal, float& outOverlap) {
        outOverlap = INFINITY;

        for (size_t a = 0; a < polygon.size(); a++) {
            size_t b = (a + 1) % polygon.size();
            Vector2 edge = Vector2Subtract(polygon[b], polygon[a]);
            Vector2 axis = Vector2Normalize({-edge.y, edge.x});

            float minA = INFINITY, maxA = -INFINITY;
            for (const auto& p : polygon) {
                float proj = Vector2DotProduct(p, axis);
                minA = fminf(minA, proj); maxA = fmaxf(maxA, proj);
            }

            float centerProj = Vector2DotProduct(center, axis);
            float minB = centerProj - radius;
            float maxB = centerProj + radius;

            if (minA >= maxB || minB >= maxA) return false;

            float axisOverlap = fminf(maxA, maxB) - fmaxf(minA, minB);
            if (axisOverlap < outOverlap) {
                outOverlap = axisOverlap;
                outNormal = axis;
            }
        }

        int closestIdx = 0;
        float minDst = INFINITY;
        for (size_t i = 0; i < polygon.size(); i++) {
            float dstSq = Vector2DistanceSqr(center, polygon[i]);
            if (dstSq < minDst) {
                minDst = dstSq;
                closestIdx = (int)i;
            }
        }

        Vector2 axis = Vector2Normalize(Vector2Subtract(polygon[closestIdx], center));
        if (axis.x == 0 && axis.y == 0) axis = {0, 1};

        float minA = INFINITY, maxA = -INFINITY;
        for (const auto& p : polygon) {
            float proj = Vector2DotProduct(p, axis);
            minA = fminf(minA, proj); maxA = fmaxf(maxA, proj);
        }
        float centerProj = Vector2DotProduct(center, axis);
        float minB = centerProj - radius;
        float maxB = centerProj + radius;

        if (minA >= maxB || minB >= maxA) return false;

        float axisOverlap = fminf(maxA, maxB) - fmaxf(minA, minB);
        if (axisOverlap < outOverlap) {
            outOverlap = axisOverlap;
            outNormal = axis;
        }

        return true;
    }

    static Vector2 getCollisionNormal(Entity* a, Entity* b)
    {
        Circle* ca = dynamic_cast<Circle*>(a);
        Circle* cb = dynamic_cast<Circle*>(b);

        if (ca && cb)
        {
            Vector2 dir = Vector2Subtract(a->getGlobalPosition(), b->getGlobalPosition());
            float len = Vector2Length(dir);
            if (len < 0.001f) return { 0, -1 };
            return Vector2Normalize(dir);
        }

        ::Rectangle ra = a->getBounds();
        ::Rectangle rb = b->getBounds();

        float cax = ra.x + ra.width  / 2.0f;
        float cay = ra.y + ra.height / 2.0f;
        float cbx = rb.x + rb.width  / 2.0f;
        float cby = rb.y + rb.height / 2.0f;

        float ox = (ra.width  + rb.width)  / 2.0f - fabsf(cax - cbx);
        float oy = (ra.height + rb.height) / 2.0f - fabsf(cay - cby);

        if (ox < oy)
            return { cax > cbx ? 1.0f : -1.0f, 0 };
        else
            return { 0, cay > cby ? 1.0f : -1.0f };
    }

    static float getOverlap(Entity* a, Entity* b)
    {
        Circle* ca = dynamic_cast<Circle*>(a);
        Circle* cb = dynamic_cast<Circle*>(b);

        if (ca && cb)
        {
            float dist = Vector2Distance(a->getGlobalPosition(), b->getGlobalPosition());
            return (ca->radius + cb->radius) - dist;
        }

        ::Rectangle ra = a->getBounds();
        ::Rectangle rb = b->getBounds();

        float ox = (ra.width  + rb.width)  / 2.0f - fabsf((ra.x + ra.width/2.0f)  - (rb.x + rb.width/2.0f));
        float oy = (ra.height + rb.height) / 2.0f - fabsf((ra.y + ra.height/2.0f) - (rb.y + rb.height/2.0f));
        return fminf(ox, oy);
    }

    void RigidbodyComponent::fixedUpdate(float fixedDt, Vector2 worldSize, Scene* scene)
    {
        if (!owner || !scene) return;
        if (isStatic) return;

        // --- Sleep State ---
        constexpr float SLEEP_VEL_SQ = 4.0f;
        constexpr float SLEEP_TIME   = 0.5f;

        float speedSq = owner->velocity.x * owner->velocity.x + owner->velocity.y * owner->velocity.y;
        float angSpeedSq = angularVelocity * angularVelocity;

        if (speedSq < SLEEP_VEL_SQ && angSpeedSq < SLEEP_VEL_SQ)
        {
            sleepTimer_ += fixedDt;
            if (sleepTimer_ >= SLEEP_TIME)
            {
                isSleeping_ = true;
                owner->velocity = {0, 0};
                angularVelocity = 0.0f;
                return;
            }
        }
        else
        {
            sleepTimer_ = 0.0f;
            isSleeping_ = false;
        }

        if (isSleeping_) return;

        // --- Gravity (skipped for kinematic) ---
        if (!isKinematic)
            owner->velocity.y += 980.0f * gravityScale * fixedDt;

        // --- Linear Drag ---
        float ld = fmaxf(0.0f, 1.0f - linearDrag * fixedDt);
        owner->velocity.x *= ld;
        owner->velocity.y *= ld;

        // --- Angular Drag ---
        float ad = fmaxf(0.0f, 1.0f - angularDrag * fixedDt);
        angularVelocity *= ad;
        if (freezeRotation) angularVelocity = 0.0f;

        // --- Integrate Position ---
        Vector2 pos = owner->getGlobalPosition();
        pos.x += owner->velocity.x * fixedDt;
        pos.y += owner->velocity.y * fixedDt;
        owner->setGlobalPosition(pos);

        // --- Integrate Rotation ---
        if (!freezeRotation)
            owner->rotation += angularVelocity * fixedDt;
    }

    void RigidbodyComponent::ResolveScene(Scene* scene, float /*fixedDt*/)
    {
        if (!scene) return;

        auto& entities = scene->entities;
        const size_t count = entities.size();

        for (size_t i = 0; i < count; i++)
        {
            Entity* a = entities[i].get();
            if (!a->activeInHierarchy()) continue;
            RigidbodyComponent* rbA = a->getComponent<RigidbodyComponent>();
            if (!rbA) continue;

            for (size_t j = i + 1; j < count; j++)
            {
                Entity* b = entities[j].get();
                if (!b->activeInHierarchy()) continue;
                RigidbodyComponent* rbB = b->getComponent<RigidbodyComponent>();
                if (!rbB) continue;

                bool aIsDynamic = !rbA->isStatic && !rbA->isKinematic;
                bool bIsDynamic = !rbB->isStatic && !rbB->isKinematic;

                // At least one body must be dynamic for a physics response
                if (!aIsDynamic && !bIsDynamic) continue;

                if (a->depthLayer != b->depthLayer) continue;

                if (!(rbA->collisionMask & (1 << (rbB->collisionLayer - 1)))) continue;
                if (!(rbB->collisionMask & (1 << (rbA->collisionLayer - 1)))) continue;

                if (rbA->isSleeping_ && rbB->isSleeping_) continue;

                if (!a->collidesWith(b)) continue;

                Vector2 normal = {0, 0};
                float overlap = 0.0f;

                std::vector<Vector2> p1 = a->getVertices();
                std::vector<Vector2> p2 = b->getVertices();
                Circle* c1 = dynamic_cast<Circle*>(a);
                Circle* c2 = dynamic_cast<Circle*>(b);

                if (!p1.empty() && !p2.empty()) {
                    if (!checkCollisionSAT(p1, p2, normal, overlap)) continue;
                    if (overlap <= 0.0f) continue;
                    Vector2 centerDir = Vector2Subtract(a->getGlobalPosition(), b->getGlobalPosition());
                    if (Vector2DotProduct(normal, centerDir) < 0) normal = Vector2Scale(normal, -1.0f);
                } else if (c1 && !p2.empty()) {
                    if (!checkCollisionCirclePolygon(c1->getGlobalPosition(), c1->radius, p2, normal, overlap)) continue;
                    if (overlap <= 0.0f) continue;
                    Vector2 centerDir = Vector2Subtract(a->getGlobalPosition(), b->getGlobalPosition());
                    if (Vector2DotProduct(normal, centerDir) < 0) normal = Vector2Scale(normal, -1.0f);
                } else if (!p1.empty() && c2) {
                    if (!checkCollisionCirclePolygon(c2->getGlobalPosition(), c2->radius, p1, normal, overlap)) continue;
                    if (overlap <= 0.0f) continue;
                    Vector2 centerDir = Vector2Subtract(a->getGlobalPosition(), b->getGlobalPosition());
                    if (Vector2DotProduct(normal, centerDir) < 0) normal = Vector2Scale(normal, -1.0f);
                } else {
                    overlap = getOverlap(a, b);
                    if (overlap <= 0.0f) continue;
                    normal = getCollisionNormal(a, b);
                }

                // Wake sleeping neighbors
                if (rbA->isSleeping_) { rbA->isSleeping_ = false; rbA->sleepTimer_ = 0.0f; }
                if (rbB->isSleeping_) { rbB->isSleeping_ = false; rbB->sleepTimer_ = 0.0f; }

                // Positional correction — normal points from b toward a.
                // Only dynamic bodies are displaced; static/kinematic bodies hold their position.
                if (aIsDynamic && bIsDynamic) {
                    a->setGlobalPosition(Vector2Add(a->getGlobalPosition(), Vector2Scale(normal, overlap * 0.5f)));
                    b->setGlobalPosition(Vector2Subtract(b->getGlobalPosition(), Vector2Scale(normal, overlap * 0.5f)));
                } else if (aIsDynamic) {
                    a->setGlobalPosition(Vector2Add(a->getGlobalPosition(), Vector2Scale(normal, overlap)));
                } else {
                    b->setGlobalPosition(Vector2Subtract(b->getGlobalPosition(), Vector2Scale(normal, overlap)));
                }

                // Static/kinematic bodies have infinite effective mass (invMass = 0)
                float invMassA = (aIsDynamic && rbA->mass > 0.0f) ? 1.0f / rbA->mass : 0.0f;
                float invMassB = (bIsDynamic && rbB->mass > 0.0f) ? 1.0f / rbB->mass : 0.0f;
                float invMassSum = invMassA + invMassB;
                if (invMassSum < 0.0001f) continue;

                Vector2 vA = aIsDynamic ? a->velocity : Vector2{0, 0};
                Vector2 vB = bIsDynamic ? b->velocity : Vector2{0, 0};

                float vRelN = Vector2DotProduct(Vector2Subtract(vA, vB), normal);
                if (vRelN >= 0.0f) continue;

                float restitution = fmaxf(rbA->bounciness, rbB->bounciness);
                float jImpulse = -(1.0f + restitution) * vRelN / invMassSum;

                if (aIsDynamic)
                    a->velocity = Vector2Add(vA, Vector2Scale(normal, jImpulse * invMassA));
                if (bIsDynamic)
                    b->velocity = Vector2Subtract(vB, Vector2Scale(normal, jImpulse * invMassB));

                // Angular impulse for a
                if (aIsDynamic && !rbA->freezeRotation) {
                    Vector2 tangent = {-normal.y, normal.x};
                    float tangentVel = Vector2DotProduct(Vector2Subtract(vA, vB), tangent);
                    ::Rectangle bounds = a->getBounds();
                    float momentArm = fmaxf(bounds.width, bounds.height) * 0.25f;
                    rbA->angularVelocity += (tangentVel * jImpulse * invMassA * momentArm) * RAD2DEG * 0.05f;
                }

                // Angular impulse for b (reaction)
                if (bIsDynamic && !rbB->freezeRotation) {
                    Vector2 tangent = {-normal.y, normal.x};
                    float tangentVel = Vector2DotProduct(Vector2Subtract(vB, vA), tangent);
                    ::Rectangle bounds = b->getBounds();
                    float momentArm = fmaxf(bounds.width, bounds.height) * 0.25f;
                    rbB->angularVelocity += (tangentVel * jImpulse * invMassB * momentArm) * RAD2DEG * 0.05f;
                }
            }
        }
    }

    void RigidbodyComponent::inspect()
    {
        if (!owner) return;

        // Body type radio buttons
        ImGui::Text("Body Type");
        ImGui::SameLine();
        int bodyType = isStatic ? 2 : (isKinematic ? 1 : 0);
        bool changed = false;
        if (ImGui::RadioButton("Dynamic",   bodyType == 0)) { bodyType = 0; changed = true; }
        ImGui::SameLine();
        if (ImGui::RadioButton("Kinematic", bodyType == 1)) { bodyType = 1; changed = true; }
        ImGui::SameLine();
        if (ImGui::RadioButton("Static",    bodyType == 2)) { bodyType = 2; changed = true; }
        if (changed)
        {
            isStatic    = (bodyType == 2);
            isKinematic = (bodyType == 1);
        }

        if (isStatic)
            ImGui::TextDisabled("Static: immovable, no gravity, acts as a solid wall.");
        else if (isKinematic)
            ImGui::TextDisabled("Kinematic: script-controlled, collides as solid for others.");

        ImGui::Separator();

        // --- Linear ---
        if (ImGui::CollapsingHeader("Linear Motion", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(8.0f);

            if (!isStatic && !isKinematic)
            {
                ImGui::DragFloat("Mass",          &mass,         0.1f, 0.1f, 1000.0f);
                ImGui::SliderFloat("Gravity Scale",  &gravityScale, -10.0f, 10.0f);
                ImGui::DragFloat("Linear Drag",   &linearDrag,   0.01f, 0.0f, 20.0f);
                ImGui::SliderFloat("Bounciness",     &bounciness,   0.0f,  1.0f);
                ImGui::DragFloat2("Velocity",      &owner->velocity.x, 0.1f);

                ImGui::Spacing();
                ImGui::TextDisabled("Sleep: %s", isSleeping_ ? "yes" : "no");
            }
            else
            {
                ImGui::SliderFloat("Bounciness", &bounciness, 0.0f, 1.0f);
            }

            ImGui::Unindent(8.0f);
        }

        // --- Angular ---
        if (!isStatic && ImGui::CollapsingHeader("Angular Motion", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(8.0f);
            ImGui::DragFloat("Angular Velocity", &angularVelocity, 1.0f);
            ImGui::DragFloat("Angular Drag",     &angularDrag,     0.1f, 0.0f, 50.0f);
            ImGui::Checkbox("Freeze Rotation",   &freezeRotation);
            ImGui::Unindent(8.0f);
        }

        // --- Collision Layers ---
        if (ImGui::CollapsingHeader("Collision Layers"))
        {
            ImGui::Indent(8.0f);

            ImGui::Text("Layer (this body):");
            ImGui::PushID("LayerButtons");
            for (int i = 1; i <= 8; i++)
            {
                if (i > 1) ImGui::SameLine();
                bool isThis = (collisionLayer == i);
                if (isThis) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
                char lbl[4]; snprintf(lbl, sizeof(lbl), "%d", i);
                if (ImGui::Button(lbl, ImVec2(28, 24))) collisionLayer = i;
                if (isThis) ImGui::PopStyleColor();
            }
            ImGui::PopID();

            ImGui::Spacing();
            ImGui::Text("Collides With (mask):");
            ImGui::PushID("MaskButtons");
            for (int i = 1; i <= 8; i++)
            {
                if (i > 1) ImGui::SameLine();
                bool active = (collisionMask & (1 << (i - 1))) != 0;
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
                char lbl[4]; snprintf(lbl, sizeof(lbl), "%d", i);
                if (ImGui::Button(lbl, ImVec2(28, 24)))
                    collisionMask ^= (1 << (i - 1));
                if (active) ImGui::PopStyleColor();
            }
            ImGui::PopID();

            ImGui::Unindent(8.0f);
        }
    }
}
