#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"
#include <string>
#include <cstring>

namespace Indium
{
    // --------------------------------------------------------------------
    // SpawnPointComponent
    //
    // A marker for where things should appear — player starts, enemy waves,
    // item drops, respawn locations.  It holds an id + an optional prefab
    // name + a facing rotation, and draws an editor gizmo.  Gameplay code
    // queries spawn points and instantiates at their transform:
    //
    //   for (auto* sp : FindObjectsOfType<SpawnPointComponent>())
    //       if (sp->spawnId == "enemy")
    //           InstantiatePrefab(sp->prefabName);  // then place at sp->Position()
    //
    // Position()/Rotation() give the world-space spawn transform.
    // --------------------------------------------------------------------
    struct SpawnPointComponent : Component
    {
        std::string spawnId    = "default";  // logical group / tag for queries
        std::string prefabName = "";         // optional prefab to spawn here
        float       facing     = 0.0f;       // spawn rotation (degrees), additive to entity
        bool        showGizmo  = true;

        void update(float, Vector2, Scene*) override {}

        // --- Script-facing ---
        [[nodiscard]] Vector2 Position() const { return owner ? owner->getGlobalPosition() : Vector2{0,0}; }
        [[nodiscard]] float   Rotation() const { return (owner ? owner->getGlobalRotation() : 0.0f) + facing; }

        void draw() const override
        {
            if (!showGizmo || !owner) return;
            Vector2 p = owner->getGlobalPosition();
            Color   col = Color{ 80, 200, 255, 220 };
            // Ring + facing arrow
            DrawCircleLinesV(p, 12.0f, col);
            DrawCircleV(p, 3.0f, col);
            float rad = Rotation() * DEG2RAD;
            Vector2 tip = { p.x + cosf(rad) * 22.0f, p.y + sinf(rad) * 22.0f };
            DrawLineEx(p, tip, 2.0f, col);
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            char idBuf[64] = {};
            strncpy(idBuf, spawnId.c_str(), sizeof(idBuf) - 1);
            ImGui::Text("Spawn Id");
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##SPId", idBuf, sizeof(idBuf))) { if (snapshotCb) snapshotCb(); spawnId = idBuf; }
            ImGui::PopItemWidth();

            char pfBuf[128] = {};
            strncpy(pfBuf, prefabName.c_str(), sizeof(pfBuf) - 1);
            ImGui::Text("Prefab Name (optional)");
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##SPPrefab", pfBuf, sizeof(pfBuf))) { if (snapshotCb) snapshotCb(); prefabName = pfBuf; }
            ImGui::PopItemWidth();

            ImGui::Text("Facing (deg)");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##SPFacing", &facing, 1.0f, -360.0f, 360.0f, "%.0f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Checkbox("Show Gizmo", &showGizmo);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        }

        std::string getName() const override { return "SpawnPoint"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<SpawnPointComponent>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["spawnId"]    = spawnId;
            j["prefabName"] = prefabName;
            j["facing"]     = facing;
            j["showGizmo"]  = showGizmo;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("spawnId"))    spawnId    = j["spawnId"].get<std::string>();
            if (j.contains("prefabName")) prefabName = j["prefabName"].get<std::string>();
            if (j.contains("facing"))     facing     = j["facing"].get<float>();
            if (j.contains("showGizmo"))  showGizmo  = j["showGizmo"].get<bool>();
        }
    };
}
