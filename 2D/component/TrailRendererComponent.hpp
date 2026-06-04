#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"
#include <deque>

namespace Indium
{
    // --------------------------------------------------------------------
    // TrailRendererComponent
    //
    // Samples the entity's world position over time and draws a fading,
    // tapering ribbon behind it — for projectiles, dashes, swords, comets.
    //
    // The trail lives in WORLD space (a history of where the entity has
    // been), so it correctly streaks behind a moving object regardless of
    // the entity's own rotation.
    // --------------------------------------------------------------------
    struct TrailRendererComponent : Component
    {
        float time      = 0.5f;     // seconds a point stays alive
        float minVertexDistance = 4.0f; // only add a point after moving this far
        float startWidth = 12.0f;   // width at the head (entity)
        float endWidth   = 0.0f;    // width at the tail
        Color startColor = { 255, 255, 255, 255 };
        Color endColor   = { 255, 255, 255, 0 };
        bool  emitting   = true;
        bool  useEntityColor = false;

        struct Point { Vector2 pos; float age; };
        std::deque<Point> points_;   // runtime (not serialized)

        void update(float dt, Vector2, Scene*) override
        {
            if (!owner) return;

            // Age + expire existing points
            for (auto& p : points_) p.age += dt;
            while (!points_.empty() && points_.front().age >= time) points_.pop_front();

            if (!emitting) return;

            Vector2 pos = owner->getGlobalPosition();
            if (points_.empty() || Vector2Distance(points_.back().pos, pos) >= minVertexDistance)
                points_.push_back({ pos, 0.0f });
        }

        // --- Script-facing ---
        void Emit(bool on) { emitting = on; }
        void ClearTrail()  { points_.clear(); }

        void draw() const override
        {
            if (points_.size() < 2) return;
            Color sc = useEntityColor && owner ? owner->color : startColor;
            Color ec = useEntityColor && owner ? Color{ owner->color.r, owner->color.g, owner->color.b, 0 } : endColor;

            // Draw as a strip of trapezoids from tail (oldest) to head (newest).
            for (size_t i = 0; i + 1 < points_.size(); i++)
            {
                // t = 0 at tail, 1 at head
                float t0 = 1.0f - (points_[i].age     / time);
                float t1 = 1.0f - (points_[i + 1].age / time);
                t0 = (t0 < 0) ? 0 : (t0 > 1 ? 1 : t0);
                t1 = (t1 < 0) ? 0 : (t1 > 1 ? 1 : t1);

                float w0 = endWidth + (startWidth - endWidth) * t0;
                float w1 = endWidth + (startWidth - endWidth) * t1;

                Color c0 = lerpColor_(ec, sc, t0);
                Color c1 = lerpColor_(ec, sc, t1);

                Vector2 a = points_[i].pos;
                Vector2 b = points_[i + 1].pos;
                Vector2 dir = Vector2Subtract(b, a);
                if (Vector2Length(dir) < 0.0001f) continue;
                Vector2 n = Vector2Normalize({ -dir.y, dir.x });

                Vector2 a0 = { a.x + n.x * w0 * 0.5f, a.y + n.y * w0 * 0.5f };
                Vector2 a1 = { a.x - n.x * w0 * 0.5f, a.y - n.y * w0 * 0.5f };
                Vector2 b0 = { b.x + n.x * w1 * 0.5f, b.y + n.y * w1 * 0.5f };
                Vector2 b1 = { b.x - n.x * w1 * 0.5f, b.y - n.y * w1 * 0.5f };

                // Two triangles per segment (use averaged color for the fill)
                DrawTriangle(a0, a1, b0, c0);
                DrawTriangle(a1, b1, b0, c1);
            }
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::Text("Lifetime (s)");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##TRTime", &time, 0.05f, 0.05f, 10.0f, "%.2f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Text("Min Vertex Distance");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##TRDist", &minVertexDistance, 0.5f, 0.5f, 200.0f, "%.1f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Text("Start / End Width");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##TRSW", &startWidth, 0.5f, 0.0f, 500.0f, "Start %.1f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::DragFloat("##TREW", &endWidth, 0.5f, 0.0f, 500.0f, "End %.1f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Checkbox("Emitting", &emitting);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::SameLine();
            ImGui::Checkbox("Use Entity Color", &useEntityColor);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            if (!useEntityColor)
            {
                colorEdit_("Start Color", "##TRSC", startColor, snapshotCb);
                colorEdit_("End Color",   "##TREC", endColor,   snapshotCb);
            }
            ImGui::TextDisabled("Live points: %d", (int)points_.size());
        }

        std::string getName() const override { return "TrailRenderer"; }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<TrailRendererComponent>(*this);
            c->points_.clear();
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j      = Component::serialize();
            j["time"]             = time;
            j["minVertexDistance"]= minVertexDistance;
            j["startWidth"]       = startWidth;
            j["endWidth"]         = endWidth;
            j["startColor"]       = { startColor.r, startColor.g, startColor.b, startColor.a };
            j["endColor"]         = { endColor.r, endColor.g, endColor.b, endColor.a };
            j["emitting"]         = emitting;
            j["useEntityColor"]   = useEntityColor;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("time"))              time              = j["time"].get<float>();
            if (j.contains("minVertexDistance")) minVertexDistance = j["minVertexDistance"].get<float>();
            if (j.contains("startWidth"))        startWidth        = j["startWidth"].get<float>();
            if (j.contains("endWidth"))          endWidth          = j["endWidth"].get<float>();
            if (j.contains("emitting"))          emitting          = j["emitting"].get<bool>();
            if (j.contains("useEntityColor"))    useEntityColor    = j["useEntityColor"].get<bool>();
            readColor_(j, "startColor", startColor);
            readColor_(j, "endColor",   endColor);
        }

    private:
        static Color lerpColor_(Color a, Color b, float t)
        {
            return {
                (unsigned char)(a.r + (b.r - a.r) * t),
                (unsigned char)(a.g + (b.g - a.g) * t),
                (unsigned char)(a.b + (b.b - a.b) * t),
                (unsigned char)(a.a + (b.a - a.a) * t)
            };
        }
        static void readColor_(const nlohmann::json& j, const char* key, Color& c)
        {
            if (!j.contains(key)) return;
            c.r = j[key][0].get<unsigned char>(); c.g = j[key][1].get<unsigned char>();
            c.b = j[key][2].get<unsigned char>(); c.a = j[key][3].get<unsigned char>();
        }
        static void colorEdit_(const char* label, const char* id, Color& c, std::function<void()>& cb)
        {
            ImGui::Text("%s", label);
            float v[4] = { c.r/255.f, c.g/255.f, c.b/255.f, c.a/255.f };
            ImGui::PushItemWidth(-1);
            if (ImGui::ColorEdit4(id, v))
            {
                c.r=(unsigned char)(v[0]*255); c.g=(unsigned char)(v[1]*255);
                c.b=(unsigned char)(v[2]*255); c.a=(unsigned char)(v[3]*255);
            }
            if (ImGui::IsItemActivated() && cb) cb();
            ImGui::PopItemWidth();
        }
    };
}
