#include "RigidbodyComponent.hpp"
#include "../Scene/Scene.hpp"
#include "../Entity/Entity.hpp"
#include "../Entity/Circle.hpp"

namespace Indium
{
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
            if (!owner->collidesWith(other.get())) continue;

            float overlap = getOverlap(owner, other.get());
            if (overlap <= 0) continue;

            // Simple penetration correction
            Vector2 normal = getCollisionNormal(owner, other.get());
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
        ImGui::DragFloat("Gravity Scale", &gravityScale, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Bounciness", &bounciness, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat2("Velocity", &owner->velocity.x, 0.1f);
    }
}
