#include "RigidbodyComponent.hpp"
#include "../Scene/Scene.hpp"
#include "../Entity/Entity.hpp"
#include "../Entity/Circle.hpp"

namespace Indium
{
    /**
     * @brief Performs Separating Axis Theorem collision detection.
     */
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

                if (minA >= maxB || minB >= maxA) {
                    return false;
                }

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

        // 1. Check polygon axes
        for (size_t a = 0; a < polygon.size(); a++) {
            size_t b = (a + 1) % polygon.size();
            Vector2 edge = Vector2Subtract(polygon[b], polygon[a]);
            Vector2 axis = Vector2Normalize({-edge.y, edge.x});

            float minA = INFINITY, maxA = -INFINITY;
            for (const auto& p : polygon) {
                float proj = Vector2DotProduct(p, axis);
                minA = fminf(minA, proj); maxA = fmaxf(maxA, proj);
            }

            // Project circle
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

        // 2. Check closest point on polygon to circle center
        int closestIdx = 0;
        float minDst = INFINITY;
        for (size_t i = 0; i < polygon.size(); i++) {
            float dstSq = Vector2DistanceSqr(center, polygon[i]);
            if (dstSq < minDst) {
                minDst = dstSq;
                closestIdx = i;
            }
        }

        // Axis from circle center to closest vertex
        Vector2 axis = Vector2Normalize(Vector2Subtract(polygon[closestIdx], center));
        if (axis.x == 0 && axis.y == 0) axis = {0, 1}; // fallback if exactly same point

        // Project polygon
        float minA = INFINITY, maxA = -INFINITY;
        for (const auto& p : polygon) {
            float proj = Vector2DotProduct(p, axis);
            minA = fminf(minA, proj); maxA = fmaxf(maxA, proj);
        }
        // Project circle
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

    /**
     * @brief Calculates the collision normal between two entities.
     *
     * The normal points from entity B towards entity A, representing the
     * direction of the impulse required to separate them.
     */
    static Vector2 getCollisionNormal(Entity* a, Entity* b)
    {
        Circle* ca = dynamic_cast<Circle*>(a);
        Circle* cb = dynamic_cast<Circle*>(b);

        // Specialized Circle-to-Circle normal calculation
        if (ca && cb)
        {
            Vector2 dir = Vector2Subtract(a->position, b->position);
            float len = Vector2Length(dir);
            // Handle perfectly overlapping entities to prevent division by zero
            if (len < 0.001f) return { 0, -1 };
            return Vector2Normalize(dir);
        }

        // Generic AABB-to-AABB normal calculation based on minimal penetration axis
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

    /**
     * @brief Determines the penetration depth between two colliding entities.
     */
    static float getOverlap(Entity* a, Entity* b)
    {
        Circle* ca = dynamic_cast<Circle*>(a);
        Circle* cb = dynamic_cast<Circle*>(b);

        if (ca && cb)
        {
            float dist = Vector2Distance(a->position, b->position);
            return (ca->radius + cb->radius) - dist;
        }

        ::Rectangle ra = a->getBounds();
        ::Rectangle rb = b->getBounds();

        float ox = (ra.width  + rb.width)  / 2.0f - fabsf((ra.x + ra.width/2.0f)  - (rb.x + rb.width/2.0f));
        float oy = (ra.height + rb.height) / 2.0f - fabsf((ra.y + ra.height/2.0f) - (rb.y + rb.height/2.0f));
        return fminf(ox, oy);
    }

    /**
     * @brief Updates the physics state and resolves collisions.
     */
    void RigidbodyComponent::update(float dt, Vector2 worldSize, Scene* scene)
    {
        if (!owner || !scene) return;

        // Apply forces and integrate position (Euler integration)
        if (!isStatic)
        {
            owner->velocity.y += 980.0f * gravityScale * dt; // Apply gravity constant
            owner->position.x += owner->velocity.x * dt;
            owner->position.y += owner->velocity.y * dt;
        }

        // World Boundary Constraints
        ::Rectangle bounds = owner->getBounds();

        // Floor collision
        if (bounds.y + bounds.height >= worldSize.y)
        {
            owner->position.y -= (bounds.y + bounds.height - worldSize.y);
            owner->velocity.y  = -owner->velocity.y * bounciness;
        }

        // Left wall collision
        if (bounds.x < 0)
        {
            owner->position.x -= bounds.x;
            owner->velocity.x  = 0;
        }

        // Right wall collision
        if (bounds.x + bounds.width > worldSize.x)
        {
            owner->position.x -= (bounds.x + bounds.width - worldSize.x);
            owner->velocity.x  = 0;
        }

        // Entity-to-Entity Collision Resolution
        for (auto& other : scene->entities)
        {
            if (other.get() == owner) continue;

            // Broad phase check using AABB
            if (!owner->collidesWith(other.get())) continue;

            Vector2 normal = {0, 0};
            float overlap = 0.0f;

            std::vector<Vector2> p1 = owner->getVertices();
            std::vector<Vector2> p2 = other->getVertices();

            Circle* c1 = dynamic_cast<Circle*>(owner);
            Circle* c2 = dynamic_cast<Circle*>(other.get());

            if (!p1.empty() && !p2.empty()) {
                // Both entities support polygon vertices, use SAT
                if (!checkCollisionSAT(p1, p2, normal, overlap)) {
                    continue; // Separating axis found, no collision
                }

                // Ensure normal points from other to owner
                Vector2 centerDir = Vector2Subtract(owner->position, other->position);
                if (Vector2DotProduct(normal, centerDir) < 0) {
                    normal = Vector2Scale(normal, -1.0f);
                }
            } else if (c1 && !p2.empty()) {
                if (!checkCollisionCirclePolygon(c1->position, c1->radius, p2, normal, overlap)) continue;
                Vector2 centerDir = Vector2Subtract(owner->position, other->position);
                if (Vector2DotProduct(normal, centerDir) < 0) normal = Vector2Scale(normal, -1.0f);
            } else if (!p1.empty() && c2) {
                if (!checkCollisionCirclePolygon(c2->position, c2->radius, p1, normal, overlap)) continue;
                Vector2 centerDir = Vector2Subtract(owner->position, other->position);
                if (Vector2DotProduct(normal, centerDir) < 0) normal = Vector2Scale(normal, -1.0f);
            } else {
                // Fallback to circle/AABB legacy logic
                overlap = getOverlap(owner, other.get());
                if (overlap <= 0) continue;
                normal = getCollisionNormal(owner, other.get());
            }

            // Penetration correction
            owner->position = Vector2Add(owner->position, Vector2Scale(normal, overlap));

            // Apply impulse by zeroing or reflecting velocity along the collision normal
            float dot = Vector2DotProduct(owner->velocity, normal);
            if (dot < 0)
            {
                owner->velocity = Vector2Subtract(owner->velocity, Vector2Scale(normal, dot * (1.0f + bounciness)));
            }
        }
    }

    /**
     * @brief Exposes physics parameters to the Editor UI.
     */
    void RigidbodyComponent::inspect()
    {
        if (!owner) return;
        ImGui::Text("Rigidbody Properties");
        ImGui::Separator();

        ImGui::DragFloat("Mass", &mass, 0.1f, 0.1f, 100.0f);
        ImGui::SliderFloat("Gravity Scale", &gravityScale, -10.0f, 10.0f);
        ImGui::SliderFloat("Bounciness", &bounciness, 0.0f, 1.0f);
        ImGui::DragFloat2("Velocity", &owner->velocity.x, 0.1f);
    }
}
