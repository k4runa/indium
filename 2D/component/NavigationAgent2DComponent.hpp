#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../core/scene/Scene.hpp"
#include "../../core/nav/NavGrid.hpp"
#include "NavigationRegion2DComponent.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"
#include <string>
#include <cstring>
#include <vector>

namespace Indium
{
    // --------------------------------------------------------------------
    // NavigationAgent2DComponent
    //
    // Steers the owning entity toward a destination, automatically routing
    // around solid colliders using grid-based A* (see core/nav/NavGrid).
    //
    // Destination is either:
    //   - an entity (by name) that it chases, repathing as the target moves
    //   - a fixed world point
    //
    // The grid is rebuilt from the scene on a repath interval, so moving
    // obstacles are handled (at the cost of a periodic rebuild). Movement is
    // simple constant-speed seeking along the returned waypoints.
    //
    // Scripts can also drive it:
    //   auto* ag = owner->getComponent<NavigationAgent2DComponent>();
    //   ag->SetDestination({x, y});
    // --------------------------------------------------------------------
    struct NavigationAgent2DComponent : Component
    {
        // --- Destination ---
        bool        useTargetEntity = false;
        std::string targetEntityName;
        Vector2     destination     = {0, 0};   // world point (when not chasing an entity)

        // --- Steering ---
        float speed          = 200.0f;   // pixels / second
        float stopDistance   = 8.0f;     // stop when this close to the final goal
        float arriveDist     = 6.0f;     // distance to advance to the next waypoint
        bool  faceDirection  = false;    // rotate entity toward travel direction

        // --- Pathing ---
        float cellSize       = 32.0f;    // nav grid resolution
        float agentRadius    = 16.0f;    // clearance kept from obstacles
        float repathInterval = 0.4f;     // seconds between path rebuilds
        bool  showDebug      = true;

        // --- Runtime state (not serialized) ---
        std::vector<Vector2> path_;
        int     pathIdx_     = 0;
        float   repathTimer_ = 0.0f;
        bool    arrived_     = false;

        // Script-facing
        void SetDestination(Vector2 worldPoint)
        {
            useTargetEntity = false;
            destination     = worldPoint;
            repathTimer_    = 0.0f; // force repath next update
            arrived_        = false;
        }
        void SetTarget(Entity* e)
        {
            useTargetEntity  = true;
            targetEntityName = e ? e->name : "";
            repathTimer_     = 0.0f;
            arrived_         = false;
        }
        void Stop() { path_.clear(); pathIdx_ = 0; arrived_ = true; }
        [[nodiscard]] bool HasArrived() const { return arrived_; }
        [[nodiscard]] int  RemainingWaypoints() const { return (int)path_.size() - pathIdx_; }

        void start(Scene*) override
        {
            path_.clear();
            pathIdx_     = 0;
            repathTimer_ = 0.0f;
            arrived_     = false;
        }

        Vector2 resolveGoal_(Scene* scene) const
        {
            if (useTargetEntity && !targetEntityName.empty())
            {
                for (const auto& e : scene->entities)
                    if (e->name == targetEntityName) return e->getGlobalPosition();
            }
            return destination;
        }

        void update(float dt, Vector2, Scene* scene) override
        {
            if (!owner || !scene) return;

            Vector2 goal = resolveGoal_(scene);
            Vector2 pos  = owner->getGlobalPosition();

            // Already at the goal?
            if (Vector2Distance(pos, goal) <= stopDistance)
            {
                arrived_ = true;
                path_.clear();
                return;
            }
            arrived_ = false;

            // --- Repath on a timer ---
            repathTimer_ -= dt;
            if (repathTimer_ <= 0.0f || path_.empty())
            {
                repathTimer_ = repathInterval;

                // Restrict the grid to a NavigationRegion2D if one exists in the scene.
                ::Rectangle region{}; bool hasRegion = false;
                for (const auto& e : scene->entities)
                {
                    if (!e->activeInHierarchy()) continue;
                    if (auto* nr = e->getComponent<NavigationRegion2DComponent>())
                    { region = nr->WorldRect(); hasRegion = true; break; }
                }

                NavGrid grid;
                grid.Build(*scene, cellSize, agentRadius, owner, hasRegion ? &region : nullptr);
                path_    = grid.FindPath(pos, goal);
                pathIdx_ = 0;
            }

            if (path_.empty()) return;
            if (pathIdx_ >= (int)path_.size()) pathIdx_ = (int)path_.size() - 1;

            // --- Seek the current waypoint ---
            Vector2 wp    = path_[pathIdx_];
            Vector2 delta = { wp.x - pos.x, wp.y - pos.y };
            float   dist  = Vector2Length(delta);

            if (dist <= arriveDist)
            {
                if (pathIdx_ + 1 < (int)path_.size()) { pathIdx_++; return; }
            }

            if (dist > 0.001f)
            {
                Vector2 dir  = Vector2Scale(delta, 1.0f / dist);
                float   step = speed * dt;
                if (step > dist) step = dist;
                owner->position.x += dir.x * step;
                owner->position.y += dir.y * step;
                if (faceDirection) owner->rotation = atan2f(dir.y, dir.x) * RAD2DEG;
            }
        }

        void draw() const override
        {
            if (!showDebug || path_.empty() || !owner) return;
            Vector2 prev = owner->getGlobalPosition();
            for (int i = pathIdx_; i < (int)path_.size(); i++)
            {
                DrawLineEx(prev, path_[i], 1.5f, Color{80, 220, 120, 180});
                DrawCircleV(path_[i], 3.0f, Color{120, 255, 150, 220});
                prev = path_[i];
            }
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::Checkbox("Chase Target Entity", &useTargetEntity);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            if (useTargetEntity)
            {
                char buf[128] = {};
                strncpy(buf, targetEntityName.c_str(), sizeof(buf) - 1);
                ImGui::Text("Target Entity");
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##NavTarget", buf, sizeof(buf))) { if (snapshotCb) snapshotCb(); targetEntityName = buf; }
                ImGui::PopItemWidth();
            }
            else
            {
                ImGui::Text("Destination (world)");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat2("##NavDest", &destination.x, 1.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
            }

            ImGui::Separator();
            ImGui::Text("Speed");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##NavSpeed", &speed, 1.0f, 0.0f, 5000.0f, "%.0f px/s");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Checkbox("Face Direction", &faceDirection);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            ImGui::Separator();
            ImGui::TextDisabled("Pathfinding");
            ImGui::Text("Cell Size");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##NavCell", &cellSize, 1.0f, 4.0f, 256.0f, "%.0f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Text("Agent Radius (clearance)");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##NavRadius", &agentRadius, 0.5f, 0.0f, 512.0f, "%.0f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();
            ImGui::Text("Repath Interval (s)");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##NavRepath", &repathInterval, 0.05f, 0.05f, 5.0f, "%.2f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Separator();
            ImGui::Checkbox("Show Debug Path", &showDebug);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            if (!path_.empty()) ImGui::TextDisabled("Waypoints: %d  (at %d)", (int)path_.size(), pathIdx_);
        }

        std::string getName() const override { return "NavigationAgent2D"; }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<NavigationAgent2DComponent>(*this);
            c->path_.clear();
            c->pathIdx_     = 0;
            c->repathTimer_ = 0.0f;
            c->arrived_     = false;
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j      = Component::serialize();
            j["useTargetEntity"]  = useTargetEntity;
            j["targetEntityName"] = targetEntityName;
            j["destination"]      = { destination.x, destination.y };
            j["speed"]            = speed;
            j["stopDistance"]     = stopDistance;
            j["arriveDist"]       = arriveDist;
            j["faceDirection"]    = faceDirection;
            j["cellSize"]         = cellSize;
            j["agentRadius"]      = agentRadius;
            j["repathInterval"]   = repathInterval;
            j["showDebug"]        = showDebug;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("useTargetEntity"))  useTargetEntity  = j["useTargetEntity"].get<bool>();
            if (j.contains("targetEntityName")) targetEntityName = j["targetEntityName"].get<std::string>();
            if (j.contains("destination"))      { destination.x = j["destination"][0]; destination.y = j["destination"][1]; }
            if (j.contains("speed"))            speed            = j["speed"].get<float>();
            if (j.contains("stopDistance"))     stopDistance     = j["stopDistance"].get<float>();
            if (j.contains("arriveDist"))       arriveDist       = j["arriveDist"].get<float>();
            if (j.contains("faceDirection"))    faceDirection    = j["faceDirection"].get<bool>();
            if (j.contains("cellSize"))         cellSize         = j["cellSize"].get<float>();
            if (j.contains("agentRadius"))      agentRadius      = j["agentRadius"].get<float>();
            if (j.contains("repathInterval"))   repathInterval   = j["repathInterval"].get<float>();
            if (j.contains("showDebug"))        showDebug        = j["showDebug"].get<bool>();
        }
    };
}
