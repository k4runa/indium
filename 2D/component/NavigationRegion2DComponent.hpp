#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "imgui.h"

namespace Indium
{
    // --------------------------------------------------------------------
    // NavigationRegion2DComponent
    //
    // Defines the walkable rectangle for pathfinding.  A NavigationAgent2D
    // that finds a region in the scene restricts its A* grid to this area —
    // cells outside the region are treated as non-walkable.  Without a
    // region, agents path across the whole world bounds.
    //
    // Use it to keep agents inside a room/arena, or to carve the playable
    // area out of a larger world.
    // --------------------------------------------------------------------
    struct NavigationRegion2DComponent : Component
    {
        Vector2 size      = { 800.0f, 600.0f };
        Vector2 offset    = { 0.0f, 0.0f };
        bool    showDebug = true;

        void update(float, Vector2, Scene*) override {}

        // World-space walkable rectangle.
        ::Rectangle WorldRect() const
        {
            Vector2 p = owner ? owner->getGlobalPosition() : Vector2{0,0};
            return { p.x + offset.x - size.x * 0.5f, p.y + offset.y - size.y * 0.5f, size.x, size.y };
        }

        void draw() const override
        {
            if (!showDebug || !owner) return;
            ::Rectangle r = WorldRect();
            DrawRectangleLinesEx(r, 2.0f, Color{ 90, 180, 255, 150 });
            DrawText("nav region", (int)r.x + 4, (int)r.y + 4, 10, Color{ 90, 180, 255, 150 });
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::TextDisabled("Restricts NavigationAgent2D pathfinding\nto this rectangle.");
            ImGui::Spacing();
            ImGui::Text("Size");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##NRSize", &size.x, 1.0f, 1.0f, 100000.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Text("Offset");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat2("##NROff", &offset.x, 1.0f);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Checkbox("Show Debug", &showDebug);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        }

        std::string getName() const override { return "NavigationRegion2D"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<NavigationRegion2DComponent>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["size"]      = { size.x, size.y };
            j["offset"]    = { offset.x, offset.y };
            j["showDebug"] = showDebug;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("size"))      { size.x = j["size"][0]; size.y = j["size"][1]; }
            if (j.contains("offset"))    { offset.x = j["offset"][0]; offset.y = j["offset"][1]; }
            if (j.contains("showDebug")) showDebug = j["showDebug"].get<bool>();
        }
    };
}
