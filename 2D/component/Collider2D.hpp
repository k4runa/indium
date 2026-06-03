#pragma once
#include "../../core/Entity.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"
#include <vector>

namespace Indium
{
    // --------------------------------------------------------------------
    // Collider2D  —  base collision shape attached to an Entity.
    // Subclasses (BoxCollider2D, CircleCollider2D) provide shape geometry.
    // RigidbodyComponent reads from these; TriggerComponent too.
    // --------------------------------------------------------------------
    struct Collider2D : Component
    {
        bool    isTrigger  = false;
        Vector2 offset     = {0.0f, 0.0f};
        bool    showDebug  = false;

        void update(float, Vector2, Scene*) override {}

        virtual ::Rectangle          getBounds()       const = 0;
        virtual std::vector<Vector2> getVertices()     const { return {}; }
        virtual bool                 isCircleShape()   const { return false; }
        virtual float                getCircleRadius() const { return 0.0f; }
        virtual bool                 contains(Vector2 point) const = 0;

        virtual bool intersects(Collider2D* other) const { return CheckCollisionRecs(getBounds(), other->getBounds()); }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["isTrigger"] = isTrigger;
            j["offset"]    = { offset.x, offset.y };
            j["showDebug"] = showDebug;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("isTrigger")) isTrigger = j["isTrigger"];
            if (j.contains("offset"))
            {
                offset.x = j["offset"][0];
                offset.y = j["offset"][1];
            }
            if (j.contains("showDebug")) showDebug = j["showDebug"];
        }
    };

    // --------------------------------------------------------------------
    // BoxCollider2D  —  rotated rectangle collider.
    // By default mirrors owner->scale; set useEntityScale=false for a
    // custom independent size.
    // --------------------------------------------------------------------
    struct BoxCollider2D : Collider2D
    {
        Vector2 size           = {100.0f, 100.0f};
        bool    useEntityScale = true;

        // Returns an AABB that wraps the rotated box (broad-phase).
        ::Rectangle getBounds() const override
        {
            auto verts = getVertices();
            if (verts.empty())
            {
                Vector2 gPos = owner->getGlobalPosition();
                Vector2 sz   = useEntityScale ? owner->getGlobalScale() : size;
                return { gPos.x + offset.x - sz.x * 0.5f, gPos.y + offset.y - sz.y * 0.5f, sz.x, sz.y };
            }
            float minX = INFINITY, minY = INFINITY, maxX = -INFINITY, maxY = -INFINITY;
            for (const auto& v : verts)
            {
                minX = fminf(minX, v.x); minY = fminf(minY, v.y);
                maxX = fmaxf(maxX, v.x); maxY = fmaxf(maxY, v.y);
            }
            return { minX, minY, maxX - minX, maxY - minY };
        }

        std::vector<Vector2> getVertices() const override
        {
            Vector2 gPos  = owner->getGlobalPosition();
            Vector2 sz    = useEntityScale ? owner->getGlobalScale() : size;
            float   gRot  = owner->getGlobalRotation();
            float   hw    = sz.x * 0.5f, hh = sz.y * 0.5f;
            float   rad   = gRot * DEG2RAD;
            float   c     = cosf(rad), s = sinf(rad);
            Vector2 ctr   = { gPos.x + offset.x, gPos.y + offset.y };

            Vector2 corners[4] = { {-hw,-hh},{hw,-hh},{hw,hh},{-hw,hh} };
            std::vector<Vector2> verts(4);
            for (int i = 0; i < 4; i++)
            {
                verts[i] = {
                    ctr.x + corners[i].x * c - corners[i].y * s,
                    ctr.y + corners[i].x * s + corners[i].y * c
                };
            }
            return verts;
        }

        bool contains(Vector2 point) const override
        {
            Vector2 gPos = owner->getGlobalPosition();
            Vector2 sz   = useEntityScale ? owner->getGlobalScale() : size;
            float   gRot = owner->getGlobalRotation();
            float   hw   = sz.x * 0.5f, hh = sz.y * 0.5f;
            float   dx   = point.x - (gPos.x + offset.x);
            float   dy   = point.y - (gPos.y + offset.y);
            float   rad  = -gRot * DEG2RAD;
            float   cc   = cosf(rad), ss = sinf(rad);
            float   rx   = dx * cc - dy * ss;
            float   ry   = dx * ss + dy * cc;
            return (rx >= -hw && rx <= hw && ry >= -hh && ry <= hh);
        }

        void draw() const override
        {
            if (!showDebug || !owner) return;
            auto verts = getVertices();
            for (int i = 0; i < 4; i++) DrawLineEx(verts[i], verts[(i + 1) % 4], 1.0f, Color{0, 230, 118, 200});
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::Checkbox("Is Trigger",       &isTrigger);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::Checkbox("Show Debug",       &showDebug);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::Separator();
            ImGui::Checkbox("Use Entity Scale", &useEntityScale);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            if (!useEntityScale)
            {
                ImGui::Text("Size");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat2("##BoxSize", &size.x, 1.0f, 1.0f, 10000.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
            }
            ImGui::Text("Offset");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##BoxOffset", &offset.x, 1.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
        }

        std::string getName() const override { return "BoxCollider2D"; }

        std::unique_ptr<Component> clone() const override { return std::make_unique<BoxCollider2D>(*this); }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Collider2D::serialize();
            j["size"]           = { size.x, size.y };
            j["useEntityScale"] = useEntityScale;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Collider2D::deserialize(j);
            if (j.contains("size")) { size.x = j["size"][0]; size.y = j["size"][1]; }
            if (j.contains("useEntityScale")) useEntityScale = j["useEntityScale"];
        }
    };

    // --------------------------------------------------------------------
    // CircleCollider2D  —  circle collider.
    // Provides precise circle–circle and circle–AABB broad-phase checks.
    // --------------------------------------------------------------------
    struct CircleCollider2D : Collider2D
    {
        float radius = 50.0f;

        bool  isCircleShape()   const override { return true; }

        // Effective radius = authored radius × the entity's Transform scale, so render,
        // collision, bounds and scene queries all track the scaled visual. A non-uniform
        // scale collapses to its average — a true circle can't become an ellipse.
        float getCircleRadius() const override
        {
            float s = 1.0f;
            if (owner) { Vector2 g = owner->getGlobalScale(); s = 0.5f * (fabsf(g.x) + fabsf(g.y)); }
            return radius * s;
        }

        ::Rectangle getBounds() const override
        {
            Vector2 center = Vector2Add(owner->getGlobalPosition(), offset);
            float   r      = getCircleRadius();
            return { center.x - r, center.y - r, r * 2.0f, r * 2.0f };
        }

        bool contains(Vector2 point) const override
        {
            Vector2 center = Vector2Add(owner->getGlobalPosition(), offset);
            return CheckCollisionPointCircle(point, center, getCircleRadius());
        }

        bool intersects(Collider2D* other) const override
        {
            Vector2 center = Vector2Add(owner->getGlobalPosition(), offset);
            if (other->isCircleShape())
            {
                Vector2 otherCenter = Vector2Add(other->owner->getGlobalPosition(), other->offset);
                return CheckCollisionCircles(center, getCircleRadius(), otherCenter, other->getCircleRadius());
            }
            return CheckCollisionCircleRec(center, getCircleRadius(), other->getBounds());
        }

        void draw() const override
        {
            if (!showDebug || !owner) return;
            Vector2 center = Vector2Add(owner->getGlobalPosition(), offset);
            DrawCircleLines((int)center.x, (int)center.y, getCircleRadius(), Color{0, 230, 118, 200});
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::Checkbox("Is Trigger", &isTrigger);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::Checkbox("Show Debug", &showDebug);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::Separator();
            ImGui::Text("Radius");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##CircRadius", &radius, 0.5f, 1.0f, 5000.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Text("Offset");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##CircOffset", &offset.x, 1.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
        }

        std::string getName() const override { return "CircleCollider2D"; }

        std::unique_ptr<Component> clone() const override { return std::make_unique<CircleCollider2D>(*this);}

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Collider2D::serialize();
            j["radius"] = radius;
            return j;
        }

        void deserialize(const nlohmann::json& j) override{ Collider2D::deserialize(j); if (j.contains("radius")) radius = j["radius"];}
    };

} // namespace Indium
