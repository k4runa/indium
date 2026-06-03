#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../core/scene/Scene.hpp"
#include "../../include/nlohmann/json.hpp"
#include "RigidbodyComponent.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"
#include <string>
#include <cstring>

namespace Indium
{
    // --------------------------------------------------------------------
    // AreaEffect2DComponent
    //
    // Applies a continuous force to every entity inside an area each frame
    // — wind zones, conveyor belts, explosion pushes, gravity wells, water
    // currents, etc.  Only entities that carry a (non-static) Rigidbody are
    // affected; the force is added to their velocity.
    //
    // Shapes:   Box  |  Circle
    // Modes:
    //   Directional — constant force in `direction` (wind, conveyor)
    //   Radial      — push out from / pull toward the center (explosion / well)
    //
    // `strength` is an acceleration in px/s². Radial mode optionally falls
    // off linearly to zero at the area's edge for a softer field.
    // --------------------------------------------------------------------
    struct AreaEffect2DComponent : Component
    {
        enum class Shape { Box, Circle };
        enum class Mode  { Directional, Radial };

        Shape   shape     = Shape::Box;
        Mode    mode      = Mode::Directional;
        Vector2 size      = { 200.0f, 200.0f };  // Box shape
        float   radius    = 150.0f;              // Circle shape
        Vector2 offset    = { 0.0f, 0.0f };

        Vector2 direction = { 0.0f, -1.0f };     // Directional mode (auto-normalized)
        float   strength  = 500.0f;              // acceleration px/s²
        bool    radialFalloff = true;            // Radial: fade to 0 at the edge
        int     layerMask = -1;                  // -1 = all layers
        bool    showDebug = true;

        void update(float dt, Vector2, Scene* scene) override
        {
            if (!owner || !scene || dt <= 0.0f) return;

            Vector2 center = Vector2Add(owner->getGlobalPosition(), offset);

            std::vector<Entity*> hits = (shape == Shape::Circle)
                ? scene->OverlapCircle(center, radius, layerMask)
                : scene->OverlapBox(center, size, layerMask);

            Vector2 dirN = Vector2Length(direction) > 0.0001f
                         ? Vector2Normalize(direction) : Vector2{0, 0};

            for (Entity* e : hits)
            {
                if (e == owner) continue;
                auto* rb = e->getComponent<RigidbodyComponent>();
                if (!rb || rb->isStatic) continue;

                Vector2 force = {0, 0};
                if (mode == Mode::Directional)
                {
                    force = Vector2Scale(dirN, strength);
                }
                else // Radial
                {
                    Vector2 d = Vector2Subtract(e->getGlobalPosition(), center);
                    float   dist = Vector2Length(d);
                    if (dist < 0.001f) continue;
                    Vector2 out = Vector2Scale(d, 1.0f / dist);
                    float   mag = strength;
                    if (radialFalloff)
                    {
                        float maxR = (shape == Shape::Circle) ? radius : fmaxf(size.x, size.y) * 0.5f;
                        if (maxR > 0.0f) mag *= fmaxf(0.0f, 1.0f - dist / maxR);
                    }
                    force = Vector2Scale(out, mag);
                }

                e->velocity.x += force.x * dt;
                e->velocity.y += force.y * dt;
            }
        }

        void draw() const override
        {
            if (!showDebug || !owner) return;
            Vector2 center = Vector2Add(owner->getGlobalPosition(), offset);
            Color   col    = Color{120, 200, 255, 120};

            if (shape == Shape::Circle)
                DrawCircleLinesV(center, radius, col);
            else
                DrawRectangleLinesEx({center.x - size.x*0.5f, center.y - size.y*0.5f, size.x, size.y}, 1.5f, col);

            // Direction arrow for Directional mode
            if (mode == Mode::Directional && Vector2Length(direction) > 0.0001f)
            {
                Vector2 d = Vector2Normalize(direction);
                Vector2 tip = { center.x + d.x * 40.0f, center.y + d.y * 40.0f };
                DrawLineEx(center, tip, 2.0f, col);
                Vector2 perp = { -d.y, d.x };
                DrawLineEx(tip, { tip.x - d.x*10 + perp.x*6, tip.y - d.y*10 + perp.y*6 }, 2.0f, col);
                DrawLineEx(tip, { tip.x - d.x*10 - perp.x*6, tip.y - d.y*10 - perp.y*6 }, 2.0f, col);
            }
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            const char* shapes[] = { "Box", "Circle" };
            int sh = (int)shape;
            ImGui::Text("Shape");
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##AEShape", &sh, shapes, 2)) { if (snapshotCb) snapshotCb(); shape = (Shape)sh; }
            ImGui::PopItemWidth();

            if (shape == Shape::Box)
            {
                ImGui::Text("Size");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat2("##AESize", &size.x, 1.0f, 1.0f, 100000.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
            }
            else
            {
                ImGui::Text("Radius");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat("##AERadius", &radius, 1.0f, 1.0f, 100000.0f, "%.0f");
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
            }

            ImGui::Text("Offset");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##AEOffset", &offset.x, 1.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Separator();
            const char* modes[] = { "Directional", "Radial" };
            int m = (int)mode;
            ImGui::Text("Mode");
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##AEMode", &m, modes, 2)) { if (snapshotCb) snapshotCb(); mode = (Mode)m; }
            ImGui::PopItemWidth();

            if (mode == Mode::Directional)
            {
                ImGui::Text("Direction");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat2("##AEDir", &direction.x, 0.01f, -1.0f, 1.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
            }
            else
            {
                ImGui::Checkbox("Radial Falloff", &radialFalloff);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::TextDisabled("Positive strength pushes out; negative pulls in.");
            }

            ImGui::Text("Strength (px/s\xc2\xb2)");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##AEStr", &strength, 5.0f, -100000.0f, 100000.0f, "%.0f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Separator();
            ImGui::Checkbox("Show Debug", &showDebug);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        }

        std::string getName() const override { return "AreaEffect2D"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<AreaEffect2DComponent>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j     = Component::serialize();
            j["shape"]           = (int)shape;
            j["mode"]            = (int)mode;
            j["size"]            = { size.x, size.y };
            j["radius"]          = radius;
            j["offset"]          = { offset.x, offset.y };
            j["direction"]       = { direction.x, direction.y };
            j["strength"]        = strength;
            j["radialFalloff"]   = radialFalloff;
            j["layerMask"]       = layerMask;
            j["showDebug"]       = showDebug;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("shape"))         shape         = (Shape)j["shape"].get<int>();
            if (j.contains("mode"))          mode          = (Mode)j["mode"].get<int>();
            if (j.contains("size"))          { size.x = j["size"][0]; size.y = j["size"][1]; }
            if (j.contains("radius"))        radius        = j["radius"].get<float>();
            if (j.contains("offset"))        { offset.x = j["offset"][0]; offset.y = j["offset"][1]; }
            if (j.contains("direction"))     { direction.x = j["direction"][0]; direction.y = j["direction"][1]; }
            if (j.contains("strength"))      strength      = j["strength"].get<float>();
            if (j.contains("radialFalloff")) radialFalloff = j["radialFalloff"].get<bool>();
            if (j.contains("layerMask"))     layerMask     = j["layerMask"].get<int>();
            if (j.contains("showDebug"))     showDebug     = j["showDebug"].get<bool>();
        }
    };
}
