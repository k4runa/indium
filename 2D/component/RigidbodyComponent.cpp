#include "RigidbodyComponent.hpp"
#include "Collider2D.hpp"
#include "TilemapComponent.hpp"
#include "../../core/scene/Scene.hpp"
#include "../../core/Entity.hpp"
#include "../../core/NativeScript.hpp"
#include <complex>
#include <raymath.h>
#include <set>
#include <unordered_set>
#include <utility>

namespace Indium
{
    static bool checkCollisionSAT(const std::vector<Vector2>& p1, const std::vector<Vector2>& p2, Vector2& outNormal, float& outOverlap)
    {
        outOverlap = INFINITY;

        for (int shape = 0; shape < 2; shape++)
        {
            const auto& polygon = (shape == 0) ? p1 : p2;

            for (size_t a = 0; a < polygon.size(); a++)
            {
                size_t b = (a + 1) % polygon.size();
                Vector2 edge = Vector2Subtract(polygon[b], polygon[a]);
                Vector2 axis = Vector2Normalize({-edge.y, edge.x});

                float minA = INFINITY, maxA = -INFINITY;
                for (const auto& p : p1)
                {
                    float proj = Vector2DotProduct(p, axis);
                    minA = fminf(minA, proj);
                    maxA = fmaxf(maxA, proj);
                }

                float minB = INFINITY, maxB = -INFINITY;
                for (const auto& p : p2)
                {
                    float proj = Vector2DotProduct(p, axis);
                    minB = fminf(minB, proj); maxB = fmaxf(maxB, proj);
                }

                if (minA >= maxB || minB >= maxA) return false;

                float axisOverlap = fminf(maxA, maxB) - fmaxf(minA, minB);
                if (axisOverlap < outOverlap)
                {
                    outOverlap = axisOverlap;
                    outNormal  = axis;
                }
            }
        }
        return true;
    }

    static bool checkCollisionCirclePolygon(const Vector2& center, float radius, const std::vector<Vector2>& polygon, Vector2& outNormal, float& outOverlap)
    {
        outOverlap = INFINITY;

        for (size_t a = 0; a < polygon.size(); a++)
        {
            size_t b = (a + 1) % polygon.size();
            Vector2 edge = Vector2Subtract(polygon[b], polygon[a]);
            Vector2 axis = Vector2Normalize({-edge.y, edge.x});

            float minA = INFINITY, maxA = -INFINITY;
            for (const auto& p : polygon)
            {
                float proj = Vector2DotProduct(p, axis);
                minA = fminf(minA, proj); maxA = fmaxf(maxA, proj);
            }

            float centerProj = Vector2DotProduct(center, axis);
            float minB = centerProj - radius;
            float maxB = centerProj + radius;

            if (minA >= maxB || minB >= maxA) return false;

            float axisOverlap = fminf(maxA, maxB) - fmaxf(minA, minB);
            if (axisOverlap < outOverlap)
            {
                outOverlap = axisOverlap;
                outNormal = axis;
            }
        }

        int closestIdx = 0;
        float minDst = INFINITY;
        for (size_t i = 0; i < polygon.size(); i++)
        {
            float dstSq = Vector2DistanceSqr(center, polygon[i]);
            if (dstSq < minDst) { minDst = dstSq; closestIdx = (int)i; }
        }

        Vector2 axis = Vector2Normalize(Vector2Subtract(polygon[closestIdx], center));
        if (axis.x == 0 && axis.y == 0) axis = {0, 1};

        float minA = INFINITY, maxA = -INFINITY;
        for (const auto& p : polygon)
        {
            float proj = Vector2DotProduct(p, axis);
            minA = fminf(minA, proj); maxA = fmaxf(maxA, proj);
        }
        float centerProj = Vector2DotProduct(center, axis);
        float minB = centerProj - radius;
        float maxB = centerProj + radius;

        if (minA >= maxB || minB >= maxA) return false;

        float axisOverlap = fminf(maxA, maxB) - fmaxf(minA, minB);
        if (axisOverlap < outOverlap)
        {
            outOverlap = axisOverlap;
            outNormal  = axis;
        }

        return true;
    }

    // Returns collision normal pointing from b toward a.
    // Uses Collider2D information instead of entity subclass casts.
    static Vector2 getCollisionNormal(Entity* a, Entity* b, Collider2D* colA, Collider2D* colB)
    {
        if (colA && colA->isCircleShape() && colB && colB->isCircleShape())
        {
            Vector2 dir = Vector2Subtract(a->getGlobalPosition(), b->getGlobalPosition());
            float len = Vector2Length(dir);
            if (len < 0.001f) return { 0, -1 };
            return Vector2Normalize(dir);
        }

        ::Rectangle ra = colA ? colA->getBounds() : a->getBounds();
        ::Rectangle rb = colB ? colB->getBounds() : b->getBounds();

        float cax = ra.x + ra.width  / 2.0f;
        float cay = ra.y + ra.height / 2.0f;
        float cbx = rb.x + rb.width  / 2.0f;
        float cby = rb.y + rb.height / 2.0f;

        float ox = (ra.width  + rb.width)  / 2.0f - fabsf(cax - cbx);
        float oy = (ra.height + rb.height) / 2.0f - fabsf(cay - cby);

        if (ox < oy) return { cax > cbx ? 1.0f : -1.0f, 0 };
        else         return { 0, cay > cby ? 1.0f : -1.0f };
    }

    static float getOverlap(Entity* a, Entity* b, Collider2D* colA, Collider2D* colB)
    {
        if (colA && colA->isCircleShape() && colB && colB->isCircleShape())
        {
            float dist = Vector2Distance(a->getGlobalPosition(), b->getGlobalPosition());
            return (colA->getCircleRadius() + colB->getCircleRadius()) - dist;
        }

        ::Rectangle ra = colA ? colA->getBounds() : a->getBounds();
        ::Rectangle rb = colB ? colB->getBounds() : b->getBounds();

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
        if (!isKinematic) owner->velocity.y += 980.0f * gravityScale * fixedDt;

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
        if (!freezeRotation) owner->rotation += angularVelocity * fixedDt;
    }

    enum class CollisionEvent { Enter, Stay, Exit };

    static void fireCallbacks(Entity* a, Entity* b, CollisionEvent ev, Scene* scene)
    {
        auto dispatch = [ev, scene](NativeScript* ns, Entity* other)
        {
            switch (ev)
            {
                case CollisionEvent::Enter: ns->DispatchCollisionEnter2D(other, scene); break;
                case CollisionEvent::Stay:  ns->DispatchCollisionStay2D(other, scene);  break;
                case CollisionEvent::Exit:  ns->DispatchCollisionExit2D(other, scene);  break;
            }
        };
        for (auto& c : a->components) if (auto* ns = dynamic_cast<NativeScript*>(c.get())) dispatch(ns, b);
        for (auto& c : b->components) if (auto* ns = dynamic_cast<NativeScript*>(c.get())) dispatch(ns, a);
    }

    // Resolve dynamic rigidbodies against a tilemap's solid / one-way rectangles.
    // Each merged region (TilemapComponent::GetSolidWorldRects) is static world
    // geometry: the body is pushed out along the contact normal and its velocity INTO
    // the surface is killed, so it lands on and slides against tiles. One-way rects
    // block only a body descending onto their top edge (jump up through, stand on top).
    // Box colliders resolve by AABB, circle colliders by closest-point. Static and
    // kinematic bodies are never pushed; trigger colliders are non-solid. Touch
    // contacts are recorded in `pairs` so the existing enter/stay/exit pass fires
    // OnCollision*2D between the body and the tilemap entity.
    static void resolveTilemapCollisions(Scene* scene, float fixedDt, std::set<std::pair<int,int>>& pairs)
    {
        auto& entities = scene->entities;
        const int count = (int)entities.size();

        struct TileSurface { Entity* owner; const std::vector<TilemapComponent::SolidRect>* rects; };
        std::vector<TileSurface> surfaces;
        for (int i = 0; i < count; ++i)
        {
            Entity* e = entities[i].get();
            if (!e->activeInHierarchy()) continue;
            auto* tm = e->getComponent<TilemapComponent>();
            if (!tm || !tm->enabled || !tm->collisionEnabled) continue;
            const auto& rects = tm->GetSolidWorldRects();
            if (!rects.empty()) surfaces.push_back({ e, &rects });
        }
        if (surfaces.empty()) return;

        constexpr float contactTol = 1.0f;   // a body within 1px of a face counts as touching

        for (int i = 0; i < count; ++i)
        {
            Entity* a = entities[i].get();
            if (!a->activeInHierarchy()) continue;
            RigidbodyComponent* rb = a->getComponent<RigidbodyComponent>();
            if (!rb || rb->isStatic || rb->isKinematic) continue;   // only dynamic bodies are pushed
            Collider2D* col = a->getComponent<Collider2D>();
            if (col && col->isTrigger) continue;                    // triggers are non-solid
            const bool isCircle = col && col->isCircleShape();

            for (const auto& surf : surfaces)
            {
                if (surf.owner == a) continue;
                for (const auto& sr : *surf.rects)
                {
                    const ::Rectangle& t = sr.rect;

                    bool    separated   = false;   // clearly apart → ignore
                    bool    penetrating = false;   // overlapping → push out
                    Vector2 n           = { 0.0f, 0.0f }; // contact normal (out of the tile)
                    float   pen         = 0.0f;    // penetration depth along n
                    float   bottom      = 0.0f;    // body's lowest edge (for one-way landing test)

                    if (isCircle)
                    {
                        // Closest point on the rect to the circle centre.
                        const Vector2 cc = Vector2Add(a->getGlobalPosition(), col->offset);
                        const float   r  = col->getCircleRadius();
                        const float   qx = Clamp(cc.x, t.x, t.x + t.width);
                        const float   qy = Clamp(cc.y, t.y, t.y + t.height);
                        const float   ex = cc.x - qx, ey = cc.y - qy;
                        const float   d2 = ex * ex + ey * ey;
                        bottom = cc.y + r;
                        if      (d2 > (r + contactTol) * (r + contactTol)) separated = true;
                        else if (d2 >= r * r) { /* touching, not penetrating */ }
                        else if (d2 > 1e-6f)
                        {
                            const float d = sqrtf(d2);
                            n = { ex / d, ey / d }; pen = r - d; penetrating = true;
                        }
                        else // centre inside the rect — eject through the nearest face
                        {
                            const float left = cc.x - t.x, right = t.x + t.width - cc.x;
                            const float top  = cc.y - t.y, bot   = t.y + t.height - cc.y;
                            const float m = fminf(fminf(left, right), fminf(top, bot));
                            if      (m == left)  n = { -1.0f,  0.0f };
                            else if (m == right) n = {  1.0f,  0.0f };
                            else if (m == top)   n = {  0.0f, -1.0f };
                            else                 n = {  0.0f,  1.0f };
                            pen = r + m; penetrating = true;
                        }
                    }
                    else
                    {
                        // Recompute the body AABB per rect — an earlier push may have moved it.
                        const ::Rectangle b = col ? col->getBounds() : a->getBounds();
                        const float bcx = b.x + b.width  * 0.5f, bcy = b.y + b.height * 0.5f;
                        const float tcx = t.x + t.width  * 0.5f, tcy = t.y + t.height * 0.5f;
                        const float dx  = bcx - tcx;
                        const float dy  = bcy - tcy;
                        const float px  = (b.width  + t.width)  * 0.5f - fabsf(dx);
                        const float py  = (b.height + t.height) * 0.5f - fabsf(dy);
                        bottom = b.y + b.height;
                        if (px <= -contactTol || py <= -contactTol) separated = true;
                        else if (px > 0.0f && py > 0.0f)
                        {
                            if (px < py) { n = { dx < 0.0f ? -1.0f : 1.0f, 0.0f }; pen = px; }
                            else         { n = { 0.0f, dy < 0.0f ? -1.0f : 1.0f }; pen = py; }
                            penetrating = true;
                        }
                    }

                    if (separated) continue;

                    // One-way platforms are solid only on their top face: a body may
                    // land on and rest on top, but passes through from below and the
                    // sides. Recording the resting-on-top contact (not just the landing
                    // push) keeps OnCollision callbacks from flapping once it sleeps.
                    if (sr.oneWay)
                    {
                        const float prevBottom   = bottom - a->velocity.y * fixedDt;
                        const bool  landing      = penetrating && n.y < 0.0f && a->velocity.y > 0.0f && prevBottom <= t.y + 1.0f;
                        const bool  restingOnTop = !penetrating && bottom >= t.y - contactTol && bottom <= t.y + contactTol;
                        if (!landing && !restingOnTop) continue;   // otherwise pass straight through
                    }

                    // Touching (or landing) is a contact — record it so the callback pass
                    // fires OnCollision*2D between this body and the tilemap entity.
                    pairs.insert({ std::min(a->id, surf.owner->id), std::max(a->id, surf.owner->id) });

                    if (!penetrating) continue;

                    Vector2 pos = a->getGlobalPosition();
                    pos.x += n.x * pen;
                    pos.y += n.y * pen;
                    a->setGlobalPosition(pos);

                    // Kill only the velocity component pointing into the surface.
                    const float vn = a->velocity.x * n.x + a->velocity.y * n.y;
                    if (vn < 0.0f) { a->velocity.x -= vn * n.x; a->velocity.y -= vn * n.y; }
                }
            }
        }
    }

    void RigidbodyComponent::ResolveScene(Scene* scene, float fixedDt)
    {
        if (!scene) return;
        auto& entities = scene->entities;
        const int count = (int)entities.size();

        // Build a rigidbody-only spatial grid for O(N) broad-phase
        SpatialGrid rbGrid;
        for (int i = 0; i < count; ++i)
        {
            Entity* e = entities[i].get();
            if (!e->activeInHierarchy()) continue;
            if (!e->getComponent<RigidbodyComponent>()) continue;
            auto* col = e->getComponent<Collider2D>();
            rbGrid.Insert(i, col ? col->getBounds() : e->getBounds());
        }

        std::set<std::pair<int,int>> currentPairs;

        for (int i = 0; i < count; i++)
        {
            Entity* a = entities[i].get();
            if (!a->activeInHierarchy()) continue;
            RigidbodyComponent* rbA = a->getComponent<RigidbodyComponent>();
            if (!rbA) continue;

            Collider2D* colA = a->getComponent<Collider2D>();
            auto candidates = rbGrid.Query(colA ? colA->getBounds() : a->getBounds());
            std::unordered_set<int> seen;

            for (int j : candidates)
            {
                if (j <= i) continue;
                if (!seen.insert(j).second) continue;

                Entity* b = entities[j].get();
                if (!b->activeInHierarchy()) continue;
                RigidbodyComponent* rbB = b->getComponent<RigidbodyComponent>();
                if (!rbB) continue;

                bool aIsDynamic = !rbA->isStatic && !rbA->isKinematic;
                bool bIsDynamic = !rbB->isStatic && !rbB->isKinematic;

                if (!aIsDynamic && !bIsDynamic) continue;
                if (a->depthLayer != b->depthLayer) continue;
                if (!(rbA->collisionMask & (1 << (rbB->collisionLayer - 1)))) continue;
                if (!(rbB->collisionMask & (1 << (rbA->collisionLayer - 1)))) continue;
                if (rbA->isSleeping_ && rbB->isSleeping_) continue;

                // Use Collider2D for broad-phase; fall back to entity bounds
                Collider2D* colB = b->getComponent<Collider2D>();

                // A collider flagged as a trigger is non-solid: it produces no physical
                // response or OnCollision pair. (Overlap notifications come from the
                // separate TriggerComponent.) Skip the pair entirely.
                if ((colA && colA->isTrigger) || (colB && colB->isTrigger)) continue;

                bool broadPhase = colA && colB ? colA->intersects(colB) : a->collidesWith(b);
                if (!broadPhase) continue;

                Vector2 normal  = {0, 0};
                float   overlap = 0.0f;

                std::vector<Vector2> p1 = colA ? colA->getVertices() : a->getVertices();
                std::vector<Vector2> p2 = colB ? colB->getVertices() : b->getVertices();
                bool aIsCircle = colA && colA->isCircleShape();
                bool bIsCircle = colB && colB->isCircleShape();

                if (!p1.empty() && !p2.empty())
                {
                    if (!checkCollisionSAT(p1, p2, normal, overlap)) continue;
                    if (overlap <= 0.0f) continue;
                    Vector2 centerDir = Vector2Subtract(a->getGlobalPosition(), b->getGlobalPosition());
                    if (Vector2DotProduct(normal, centerDir) < 0) normal = Vector2Scale(normal, -1.0f);
                }
                else if (aIsCircle && !p2.empty())
                {
                    Vector2 center = Vector2Add(a->getGlobalPosition(), colA->offset);
                    if (!checkCollisionCirclePolygon(center, colA->getCircleRadius(), p2, normal, overlap)) continue;
                    if (overlap <= 0.0f) continue;
                    Vector2 centerDir = Vector2Subtract(a->getGlobalPosition(), b->getGlobalPosition());
                    if (Vector2DotProduct(normal, centerDir) < 0) normal = Vector2Scale(normal, -1.0f);
                }
                else if (!p1.empty() && bIsCircle)
                {
                    Vector2 center = Vector2Add(b->getGlobalPosition(), colB->offset);
                    if (!checkCollisionCirclePolygon(center, colB->getCircleRadius(), p1, normal, overlap)) continue;
                    if (overlap <= 0.0f) continue;
                    Vector2 centerDir = Vector2Subtract(a->getGlobalPosition(), b->getGlobalPosition());
                    if (Vector2DotProduct(normal, centerDir) < 0) normal = Vector2Scale(normal, -1.0f);
                }
                else
                {
                    overlap = getOverlap(a, b, colA, colB);
                    if (overlap <= 0.0f) continue;
                    normal = getCollisionNormal(a, b, colA, colB);
                }

                // Track this collision pair for callbacks
                currentPairs.insert({std::min(a->id, b->id), std::max(a->id, b->id)});

                // Wake sleeping neighbors
                if (rbA->isSleeping_) { rbA->isSleeping_ = false; rbA->sleepTimer_ = 0.0f; }
                if (rbB->isSleeping_) { rbB->isSleeping_ = false; rbB->sleepTimer_ = 0.0f; }

                // Baumgarte stabilization: ignore sub-slop penetrations to prevent jitter
                constexpr float k_slop    = 0.2f;  // pixels of allowed penetration
                constexpr float k_correct = 0.8f;  // fraction of excess to correct per step
                float correctionMag = fmaxf(overlap - k_slop, 0.0f) * k_correct;
                if (correctionMag > 0.0f)
                {
                    if (aIsDynamic && bIsDynamic)
                    {
                        a->setGlobalPosition(Vector2Add(a->getGlobalPosition(), Vector2Scale(normal, correctionMag * 0.5f)));
                        b->setGlobalPosition(Vector2Subtract(b->getGlobalPosition(), Vector2Scale(normal, correctionMag * 0.5f)));
                    }
                    else if (aIsDynamic) { a->setGlobalPosition(Vector2Add(a->getGlobalPosition(), Vector2Scale(normal, correctionMag))); }
                    else { b->setGlobalPosition(Vector2Subtract(b->getGlobalPosition(), Vector2Scale(normal, correctionMag)));}
                }

                float invMassA   = (aIsDynamic && rbA->mass > 0.0001f) ? 1.0f / rbA->mass : 0.0f;
                float invMassB   = (bIsDynamic && rbB->mass > 0.0001f) ? 1.0f / rbB->mass : 0.0f;
                float invMassSum = invMassA + invMassB;
                if (invMassSum < 0.0001f) continue;

                Vector2 vA = aIsDynamic ? a->velocity : Vector2{0, 0};
                Vector2 vB = bIsDynamic ? b->velocity : Vector2{0, 0};

                float vRelN = Vector2DotProduct(Vector2Subtract(vA, vB), normal);
                if (vRelN >= 0.0f) continue;

                float restitution = fmaxf(rbA->bounciness, rbB->bounciness);
                float jImpulse    = -(1.0f + restitution) * vRelN / invMassSum;

                // Apply the normal impulse once to each dynamic body. (A previous version
                // re-applied it in a second block that also read an uninitialized `vn` and
                // used the wrong inverse mass for b — undefined behavior that doubled and
                // corrupted the response, letting bodies jitter and sink into each other.)
                if (aIsDynamic) a->velocity = Vector2Add(vA, Vector2Scale(normal, jImpulse * invMassA));
                if (bIsDynamic) b->velocity = Vector2Subtract(vB, Vector2Scale(normal, jImpulse * invMassB));

                // Angular impulse for a
                if (aIsDynamic && !rbA->freezeRotation)
                {
                    Vector2 tangent       = {-normal.y, normal.x};
                    float tangentVel      = Vector2DotProduct(Vector2Subtract(vA, vB), tangent);
                    ::Rectangle bounds    = colA ? colA->getBounds() : a->getBounds();
                    float momentArm       = fmaxf(bounds.width, bounds.height) * 0.25f;
                    rbA->angularVelocity += (tangentVel * jImpulse * invMassA * momentArm) * RAD2DEG * 0.05f;
                }

                // Angular impulse for b (reaction)
                if (bIsDynamic && !rbB->freezeRotation)
                {
                    Vector2 tangent       = {-normal.y, normal.x};
                    float tangentVel      = Vector2DotProduct(Vector2Subtract(vB, vA), tangent);
                    ::Rectangle bounds    = colB ? colB->getBounds() : b->getBounds();
                    float momentArm       = fmaxf(bounds.width, bounds.height) * 0.25f;
                    rbB->angularVelocity += (tangentVel * jImpulse * invMassB * momentArm) * RAD2DEG * 0.05f;
                }
            }
        }

        // --- Tilemap collision: push dynamic bodies out of solid / one-way tiles ---
        resolveTilemapCollisions(scene, fixedDt, currentPairs);

        // --- Collision Callbacks ---
        auto& prevPairs = scene->_activeCollisionPairs;

        // Enter and Stay
        for (const auto& p : currentPairs)
        {
            Entity* ea = scene->FindEntity(p.first);
            Entity* eb = scene->FindEntity(p.second);
            if (!ea || !eb) continue;
            bool isNew = prevPairs.find(p) == prevPairs.end();
            fireCallbacks(ea, eb, isNew ? CollisionEvent::Enter : CollisionEvent::Stay, scene);
        }

        // Exit
        for (const auto& p : prevPairs)
        {
            if (currentPairs.find(p) == currentPairs.end())
            {
                Entity* ea = scene->FindEntity(p.first);
                Entity* eb = scene->FindEntity(p.second);
                if (!ea || !eb) continue;
                fireCallbacks(ea, eb, CollisionEvent::Exit, scene);
            }
        }

        prevPairs = std::move(currentPairs);
    }

    void RigidbodyComponent::inspect(std::function<void()> snapshotCb)
    {
        if (!owner) return;

        // Body type radio buttons
        ImGui::Text("Body Type");
        ImGui::SameLine();
        int bodyType = isStatic ? 2 : (isKinematic ? 1 : 0);
        bool changed = false;
        if (ImGui::RadioButton("Dynamic",   bodyType == 0)) { bodyType = 0; changed = true; }
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::SameLine();
        if (ImGui::RadioButton("Kinematic", bodyType == 1)) { bodyType = 1; changed = true; }
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::SameLine();
        if (ImGui::RadioButton("Static",    bodyType == 2)) { bodyType = 2; changed = true; }
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        if (changed)
        {
            isStatic    = (bodyType == 2);
            isKinematic = (bodyType == 1);
        }

        if (isStatic)           ImGui::TextDisabled("Static: immovable, no gravity, acts as a solid wall.");
        else if (isKinematic)   ImGui::TextDisabled("Kinematic: script-controlled, collides as solid for others.");
        ImGui::Separator();

        // --- Linear ---
        if (ImGui::CollapsingHeader("Linear Motion", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(8.0f);

            if (!isStatic && !isKinematic)
            {
                ImGui::DragFloat("Mass", &mass, 0.1f, 0.0001f, 1000.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::SliderFloat("Gravity Scale",  &gravityScale, -10.0f, 10.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::DragFloat("Linear Drag",   &linearDrag,   0.01f, 0.0f, 20.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::SliderFloat("Bounciness",     &bounciness,   0.0f,  1.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::DragFloat2("Velocity",      &owner->velocity.x, 0.1f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

                ImGui::Spacing();
                ImGui::TextDisabled("Sleep: %s", isSleeping_ ? "yes" : "no");
            }
            else
            {
                ImGui::SliderFloat("Bounciness", &bounciness, 0.0f, 1.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            }

            ImGui::Unindent(8.0f);
        }

        // --- Angular ---
        if (!isStatic && ImGui::CollapsingHeader("Angular Motion", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(8.0f);
            ImGui::DragFloat("Angular Velocity", &angularVelocity, 1.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::DragFloat("Angular Drag",     &angularDrag,     0.1f, 0.0f, 50.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::Checkbox("Freeze Rotation",   &freezeRotation);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::Unindent(8.0f);
        }

        // --- Collision Layers ---
        if (ImGui::CollapsingHeader("Collision Layers"))
        {
            ImGui::Indent(8.0f);

            ImGui::Text("Layer (this body):");
            ImGui::PushID("LayerButtons");
            for (int i = 1; i <= 32; i++)
            {
                if ((i - 1) % 8 != 0) ImGui::SameLine();
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
            for (int i = 1; i <= 32; i++)
            {
                if ((i - 1) % 8 != 0) ImGui::SameLine();
                bool active = (collisionMask & (1 << (i - 1))) != 0;
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
                char lbl[4]; snprintf(lbl, sizeof(lbl), "%d", i);
                if (ImGui::Button(lbl, ImVec2(28, 24))) collisionMask ^= (1 << (i - 1));
                if (active) ImGui::PopStyleColor();
            }
            ImGui::PopID();

            ImGui::Unindent(8.0f);
        }
    }

}
