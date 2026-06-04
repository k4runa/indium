#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"
#include <vector>

namespace Indium
{
    // --------------------------------------------------------------------
    // LineRendererComponent
    //
    // Draws a polyline through a set of points — for ropes, lasers, bullet
    // trails, aim indicators, connections, etc.  Points are in LOCAL space
    // (relative to the entity) and follow the entity's position + rotation.
    //
    // Scripts can rewrite the point list at runtime:
    //   auto* lr = owner->getComponent<LineRendererComponent>();
    //   lr->points = { {0,0}, hitPointLocal };
    // --------------------------------------------------------------------
    struct LineRendererComponent : Component
    {
        std::vector<Vector2> points    = { {0, 0}, {100, 0} }; // local-space
        Color                color     = { 255, 255, 255, 255 };
        float                thickness = 2.0f;
        bool                 closed    = false;  // connect last point back to first
        bool                 useEntityColor = false; // override color with entity tint

        void update(float, Vector2, Scene*) override {}

        // --- Script-facing helpers (all points are LOCAL to the entity) ---
        void SetEndpoints(Vector2 a, Vector2 b) { points = { a, b }; }
        void SetPoints(const std::vector<Vector2>& pts) { points = pts; }
        void AddPoint(Vector2 p) { points.push_back(p); }
        void Clear() { points.clear(); }
        void SetColor(Color c) { color = c; useEntityColor = false; }

        // Transform a local point to world space (position + rotation).
        Vector2 toWorld_(Vector2 p) const
        {
            float rad = owner->getGlobalRotation() * DEG2RAD;
            float c = cosf(rad), s = sinf(rad);
            Vector2 gp = owner->getGlobalPosition();
            return { gp.x + p.x * c - p.y * s,
                     gp.y + p.x * s + p.y * c };
        }

        void draw() const override
        {
            if (!owner || points.size() < 2) return;
            Color col = useEntityColor ? owner->color : color;

            for (int i = 0; i + 1 < (int)points.size(); i++)
                DrawLineEx(toWorld_(points[i]), toWorld_(points[i + 1]), thickness, col);

            if (closed && points.size() > 2)
                DrawLineEx(toWorld_(points.back()), toWorld_(points.front()), thickness, col);
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::Text("Thickness");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##LRThick", &thickness, 0.1f, 0.1f, 100.0f, "%.1f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Checkbox("Closed Loop", &closed);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::SameLine();
            ImGui::Checkbox("Use Entity Color", &useEntityColor);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            if (!useEntityColor)
            {
                ImGui::Text("Color");
                float c[4] = { color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f };
                ImGui::PushItemWidth(-1);
                if (ImGui::ColorEdit4("##LRColor", c))
                {
                    color.r = (unsigned char)(c[0] * 255);
                    color.g = (unsigned char)(c[1] * 255);
                    color.b = (unsigned char)(c[2] * 255);
                    color.a = (unsigned char)(c[3] * 255);
                }
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
            }

            ImGui::Separator();
            ImGui::Text("Points  (%d)  — local offsets", (int)points.size());
            for (int i = 0; i < (int)points.size(); i++)
            {
                ImGui::PushID(i);
                float avail = ImGui::GetContentRegionAvail().x;
                ImGui::SetNextItemWidth(avail - 28.0f);
                ImGui::DragFloat2("##LP", &points[i].x, 1.0f);
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

        std::string getName() const override { return "LineRenderer"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<LineRendererComponent>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j   = Component::serialize();
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& p : points) arr.push_back({p.x, p.y});
            j["points"]         = arr;
            j["color"]          = { color.r, color.g, color.b, color.a };
            j["thickness"]      = thickness;
            j["closed"]         = closed;
            j["useEntityColor"] = useEntityColor;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("points"))
            {
                points.clear();
                for (const auto& p : j["points"])
                    points.push_back({p[0].get<float>(), p[1].get<float>()});
            }
            if (j.contains("color"))
            {
                color.r = j["color"][0].get<unsigned char>();
                color.g = j["color"][1].get<unsigned char>();
                color.b = j["color"][2].get<unsigned char>();
                color.a = j["color"][3].get<unsigned char>();
            }
            if (j.contains("thickness"))      thickness      = j["thickness"].get<float>();
            if (j.contains("closed"))         closed         = j["closed"].get<bool>();
            if (j.contains("useEntityColor")) useEntityColor = j["useEntityColor"].get<bool>();
        }
    };
}
