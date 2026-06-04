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

    // --------------------------------------------------------------------
    // PolygonCollider2D — arbitrary convex/concave polygon collider.
    //
    // Points are defined in local space (relative to the entity center).
    // In the Inspector, add/remove/drag individual points.  The polygon is
    // automatically transformed to world space at runtime.
    //
    // Physics note: the engine's SAT resolver reads getVertices(), so
    // polygon–polygon and polygon–circle collisions are fully supported.
    // For best physics results keep the polygon convex.
    // --------------------------------------------------------------------
    struct PolygonCollider2D : Collider2D
    {
        // Default: square matching a 100×100 entity
        std::vector<Vector2> points = { {-50,-50}, {50,-50}, {50,50}, {-50,50} };

        // --- Shape interface ---

        std::vector<Vector2> getVertices() const override
        {
            if (!owner) return {};
            Vector2 gPos = owner->getGlobalPosition();
            float   gRot = owner->getGlobalRotation() * DEG2RAD;
            float   c    = cosf(gRot), s = sinf(gRot);
            std::vector<Vector2> verts;
            verts.reserve(points.size());
            for (const auto& p : points)
            {
                verts.push_back({
                    gPos.x + offset.x + p.x * c - p.y * s,
                    gPos.y + offset.y + p.x * s + p.y * c
                });
            }
            return verts;
        }

        ::Rectangle getBounds() const override
        {
            auto verts = getVertices();
            if (verts.empty()) return {0, 0, 0, 0};
            float minX = verts[0].x, maxX = verts[0].x;
            float minY = verts[0].y, maxY = verts[0].y;
            for (const auto& v : verts)
            {
                minX = fminf(minX, v.x); maxX = fmaxf(maxX, v.x);
                minY = fminf(minY, v.y); maxY = fmaxf(maxY, v.y);
            }
            return {minX, minY, maxX - minX, maxY - minY};
        }

        bool contains(Vector2 point) const override
        {
            // Ray-casting point-in-polygon
            auto verts = getVertices();
            int  n     = (int)verts.size();
            if (n < 3) return false;
            bool inside = false;
            for (int i = 0, j = n - 1; i < n; j = i++)
            {
                bool crossY = (verts[i].y > point.y) != (verts[j].y > point.y);
                if (crossY)
                {
                    float xIntersect = (verts[j].x - verts[i].x) *
                                       (point.y - verts[i].y) /
                                       (verts[j].y - verts[i].y) + verts[i].x;
                    if (point.x < xIntersect) inside = !inside;
                }
            }
            return inside;
        }

        bool intersects(Collider2D* other) const override
        {
            // Broad-phase AABB, narrow-phase handled by engine SAT via getVertices()
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

        void draw() const override
        {
            if (!showDebug || !owner) return;
            auto  verts = getVertices();
            int   n     = (int)verts.size();
            Color col   = isTrigger ? Color{0, 200, 255, 200} : Color{0, 230, 118, 200};
            for (int i = 0; i < n; i++)
                DrawLineEx(verts[i], verts[(i + 1) % n], 1.5f, col);
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::Checkbox("Is Trigger", &isTrigger);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::Checkbox("Show Debug", &showDebug);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::Separator();

            ImGui::Text("Offset");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##PolyOff", &offset.x, 1.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Separator();

            ImGui::Text("Points  (%d)", (int)points.size());
            bool changed = false;
            for (int i = 0; i < (int)points.size(); i++)
            {
                ImGui::PushID(i);
                float avail = ImGui::GetContentRegionAvail().x;
                ImGui::SetNextItemWidth(avail - 28.0f);
                if (ImGui::DragFloat2("##PP", &points[i].x, 1.0f)) changed = true;
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::SameLine();
                bool canDel = (int)points.size() > 3;
                if (!canDel) ImGui::BeginDisabled();
                if (ImGui::SmallButton("x"))
                {
                    if (snapshotCb) snapshotCb();
                    points.erase(points.begin() + i);
                    ImGui::PopID();
                    if (!canDel) ImGui::EndDisabled();
                    break;
                }
                if (!canDel) ImGui::EndDisabled();
                ImGui::PopID();
            }
            if (ImGui::Button("+ Add Point", ImVec2(-1, 0)))
            {
                if (snapshotCb) snapshotCb();
                // Insert after the last point, slightly offset
                Vector2 last = points.empty() ? Vector2{0,0} : points.back();
                points.push_back({last.x + 20.0f, last.y});
            }
        }

        std::string getName() const override { return "PolygonCollider2D"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<PolygonCollider2D>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j    = Collider2D::serialize();
            nlohmann::json arr  = nlohmann::json::array();
            for (const auto& p : points) arr.push_back({p.x, p.y});
            j["points"] = arr;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Collider2D::deserialize(j);
            if (j.contains("points"))
            {
                points.clear();
                for (const auto& p : j["points"])
                    points.push_back({p[0].get<float>(), p[1].get<float>()});
            }
        }
    };

    // --------------------------------------------------------------------
    // EdgeCollider2D — open polyline collider for one-sided surfaces.
    //
    // Ideal for ground, walls, and platforms that don't need a filled area.
    // Points are in local space; the polyline is NOT automatically closed.
    // Physics broad-phase uses the AABB; the physics engine handles the
    // narrow-phase.  Edge colliders do not contain volume, so Contains()
    // always returns false.
    // --------------------------------------------------------------------
    struct EdgeCollider2D : Collider2D
    {
        std::vector<Vector2> points = { {-100, 0}, {100, 0} }; // default: flat line

        // --- Shape interface ---

        std::vector<Vector2> getVertices() const override
        {
            // Return empty so the physics SAT treats this as AABB-only.
            // The world-space segment data is used only for debug drawing.
            return {};
        }

        // World-space transformed edge points (for drawing / custom queries)
        std::vector<Vector2> getWorldPoints() const
        {
            if (!owner) return {};
            Vector2 gPos = owner->getGlobalPosition();
            float   gRot = owner->getGlobalRotation() * DEG2RAD;
            float   c    = cosf(gRot), s = sinf(gRot);
            std::vector<Vector2> wp;
            wp.reserve(points.size());
            for (const auto& p : points)
            {
                wp.push_back({
                    gPos.x + offset.x + p.x * c - p.y * s,
                    gPos.y + offset.y + p.x * s + p.y * c
                });
            }
            return wp;
        }

        ::Rectangle getBounds() const override
        {
            auto wp = getWorldPoints();
            if (wp.empty()) return {0, 0, 0, 0};
            float minX = wp[0].x, maxX = wp[0].x;
            float minY = wp[0].y, maxY = wp[0].y;
            for (const auto& v : wp)
            {
                minX = fminf(minX, v.x); maxX = fmaxf(maxX, v.x);
                minY = fminf(minY, v.y); maxY = fmaxf(maxY, v.y);
            }
            const float pad = 4.0f;   // small thickness for broad-phase
            return {minX - pad, minY - pad, maxX - minX + pad * 2.0f, maxY - minY + pad * 2.0f};
        }

        bool contains(Vector2 /*point*/) const override { return false; }

        bool intersects(Collider2D* other) const override
        {
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

        void draw() const override
        {
            if (!showDebug || !owner) return;
            auto  wp  = getWorldPoints();
            Color col = isTrigger ? Color{0, 200, 255, 200} : Color{0, 230, 118, 200};
            for (int i = 0; i + 1 < (int)wp.size(); i++)
                DrawLineEx(wp[i], wp[i + 1], 2.0f, col);
            for (const auto& v : wp)
                DrawCircleV(v, 3.0f, col);
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::Checkbox("Is Trigger", &isTrigger);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::Checkbox("Show Debug", &showDebug);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::Separator();

            ImGui::Text("Offset");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##EdgeOff", &offset.x, 1.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Separator();

            ImGui::Text("Points  (%d)", (int)points.size());
            for (int i = 0; i < (int)points.size(); i++)
            {
                ImGui::PushID(i);
                float avail = ImGui::GetContentRegionAvail().x;
                ImGui::SetNextItemWidth(avail - 28.0f);
                ImGui::DragFloat2("##EP", &points[i].x, 1.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::SameLine();
                bool canDel = (int)points.size() > 2;
                if (!canDel) ImGui::BeginDisabled();
                if (ImGui::SmallButton("x"))
                {
                    if (snapshotCb) snapshotCb();
                    points.erase(points.begin() + i);
                    ImGui::PopID();
                    if (!canDel) ImGui::EndDisabled();
                    break;
                }
                if (!canDel) ImGui::EndDisabled();
                ImGui::PopID();
            }
            if (ImGui::Button("+ Add Point", ImVec2(-1, 0)))
            {
                if (snapshotCb) snapshotCb();
                Vector2 last = points.empty() ? Vector2{0,0} : points.back();
                points.push_back({last.x + 50.0f, last.y});
            }
        }

        std::string getName() const override { return "EdgeCollider2D"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<EdgeCollider2D>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j   = Collider2D::serialize();
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& p : points) arr.push_back({p.x, p.y});
            j["points"] = arr;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Collider2D::deserialize(j);
            if (j.contains("points"))
            {
                points.clear();
                for (const auto& p : j["points"])
                    points.push_back({p[0].get<float>(), p[1].get<float>()});
            }
        }
    };

} // namespace Indium
