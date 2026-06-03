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
    // PathFollowerComponent
    //
    // Moves the owning entity along a sequence of waypoints at a constant
    // speed.  Waypoints are stored in LOCAL space (relative to the entity's
    // position when Play starts) so a path authored in the editor travels
    // with the entity if you move it before pressing Play.
    //
    // Loop modes:
    //   Once     — stop at the last waypoint
    //   Loop     — jump back to the first waypoint after the last
    //   PingPong — reverse direction at each end
    //
    // Optional faceDirection rotates the entity to point along its travel
    // direction (useful for patrolling enemies, moving platforms, etc.).
    // --------------------------------------------------------------------
    struct PathFollowerComponent : Component
    {
        enum class LoopMode { Once, Loop, PingPong };

        std::vector<Vector2> waypoints   = { {0, 0}, {200, 0} }; // local-space points
        float                speed       = 150.0f;   // pixels / second
        float                arriveDist  = 4.0f;     // distance to consider a waypoint reached
        LoopMode             loopMode    = LoopMode::Loop;
        bool                 faceDirection = false;  // rotate entity toward travel dir
        bool                 autoStart   = true;     // begin moving on Play
        bool                 showDebug   = true;

        // --- Runtime state (not serialized) ---
        Vector2 origin_     = {0, 0};   // entity position captured at start
        int     targetIdx_  = 0;
        int     dir_        = 1;        // +1 forward, -1 backward (PingPong)
        bool    moving_     = false;
        bool    originSet_  = false;

        void start(Scene*) override
        {
            if (owner && !originSet_) { origin_ = owner->position; originSet_ = true; }
            targetIdx_ = (waypoints.size() > 1) ? 1 : 0;
            dir_       = 1;
            moving_    = autoStart;
        }

        // --- Script-facing controls ---
        void  Play()           { moving_ = true; }
        void  Pause()          { moving_ = false; }
        void  Stop()           { moving_ = false; targetIdx_ = (waypoints.size() > 1) ? 1 : 0; dir_ = 1; }
        void  GoToWaypoint(int i) { if (i >= 0 && i < (int)waypoints.size()) targetIdx_ = i; }
        [[nodiscard]] bool IsMoving()        const { return moving_; }
        [[nodiscard]] int  CurrentWaypoint() const { return targetIdx_; }

        // World-space position of waypoint i.
        Vector2 worldPoint_(int i) const
        {
            if (i < 0 || i >= (int)waypoints.size()) return origin_;
            return { origin_.x + waypoints[i].x, origin_.y + waypoints[i].y };
        }

        void update(float dt, Vector2, Scene*) override
        {
            if (!owner || !moving_ || waypoints.size() < 2) return;
            if (!originSet_) { origin_ = owner->position; originSet_ = true; }

            Vector2 target = worldPoint_(targetIdx_);
            Vector2 delta  = { target.x - owner->position.x, target.y - owner->position.y };
            float   dist   = Vector2Length(delta);

            if (dist <= arriveDist)
            {
                // Snap to the waypoint and pick the next index based on loop mode
                owner->position = target;
                advanceTarget_();
                return;
            }

            Vector2 dir   = Vector2Scale(delta, 1.0f / dist);
            float   step  = speed * dt;
            if (step > dist) step = dist;
            owner->position.x += dir.x * step;
            owner->position.y += dir.y * step;

            if (faceDirection)
                owner->rotation = atan2f(dir.y, dir.x) * RAD2DEG;
        }

        void advanceTarget_()
        {
            int n = (int)waypoints.size();
            switch (loopMode)
            {
                case LoopMode::Once:
                    if (targetIdx_ + 1 < n) targetIdx_++;
                    else moving_ = false;
                    break;
                case LoopMode::Loop:
                    targetIdx_ = (targetIdx_ + 1) % n;
                    break;
                case LoopMode::PingPong:
                    if (targetIdx_ + dir_ < 0 || targetIdx_ + dir_ >= n)
                        dir_ = -dir_;
                    targetIdx_ += dir_;
                    break;
            }
        }

        void draw() const override
        {
            if (!showDebug || !owner) return;
            // Use the live origin in the editor (not yet playing) so the path
            // preview follows the entity as it's dragged around.
            Vector2 org = originSet_ ? origin_ : owner->position;
            auto wp = [&](int i){ return Vector2{ org.x + waypoints[i].x, org.y + waypoints[i].y }; };

            for (int i = 0; i + 1 < (int)waypoints.size(); i++)
                DrawLineEx(wp(i), wp(i + 1), 1.5f, Color{255, 160, 40, 180});
            if (loopMode == LoopMode::Loop && waypoints.size() > 2)
                DrawLineEx(wp((int)waypoints.size() - 1), wp(0), 1.5f, Color{255, 160, 40, 80});
            for (int i = 0; i < (int)waypoints.size(); i++)
                DrawCircleV(wp(i), 4.0f, Color{255, 200, 80, 220});
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::Text("Speed");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##PFSpeed", &speed, 1.0f, 0.0f, 5000.0f, "%.0f px/s");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            const char* modes[] = { "Once", "Loop", "PingPong" };
            int m = (int)loopMode;
            ImGui::Text("Loop Mode");
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##PFLoop", &m, modes, 3)) { if (snapshotCb) snapshotCb(); loopMode = (LoopMode)m; }
            ImGui::PopItemWidth();

            ImGui::Checkbox("Face Direction", &faceDirection);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::SameLine();
            ImGui::Checkbox("Auto Start", &autoStart);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            ImGui::Separator();
            ImGui::Text("Waypoints  (%d)  — local offsets", (int)waypoints.size());
            for (int i = 0; i < (int)waypoints.size(); i++)
            {
                ImGui::PushID(i);
                float avail = ImGui::GetContentRegionAvail().x;
                ImGui::SetNextItemWidth(avail - 28.0f);
                ImGui::DragFloat2("##WP", &waypoints[i].x, 1.0f);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::SameLine();
                bool canDel = (int)waypoints.size() > 2;
                if (!canDel) ImGui::BeginDisabled();
                if (ImGui::SmallButton("x"))
                {
                    if (snapshotCb) snapshotCb();
                    waypoints.erase(waypoints.begin() + i);
                    ImGui::PopID();
                    if (!canDel) ImGui::EndDisabled();
                    break;
                }
                if (!canDel) ImGui::EndDisabled();
                ImGui::PopID();
            }
            if (ImGui::Button("+ Add Waypoint", ImVec2(-1, 0)))
            {
                if (snapshotCb) snapshotCb();
                Vector2 last = waypoints.empty() ? Vector2{0,0} : waypoints.back();
                waypoints.push_back({last.x + 50.0f, last.y});
            }

            ImGui::Separator();
            ImGui::Checkbox("Show Debug", &showDebug);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        }

        std::string getName() const override { return "PathFollower"; }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<PathFollowerComponent>(*this);
            // Reset runtime state on the copy
            c->originSet_ = false;
            c->moving_    = false;
            c->targetIdx_ = 0;
            c->dir_       = 1;
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j   = Component::serialize();
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& p : waypoints) arr.push_back({p.x, p.y});
            j["waypoints"]     = arr;
            j["speed"]         = speed;
            j["arriveDist"]    = arriveDist;
            j["loopMode"]      = (int)loopMode;
            j["faceDirection"] = faceDirection;
            j["autoStart"]     = autoStart;
            j["showDebug"]     = showDebug;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("waypoints"))
            {
                waypoints.clear();
                for (const auto& p : j["waypoints"])
                    waypoints.push_back({p[0].get<float>(), p[1].get<float>()});
            }
            if (j.contains("speed"))         speed         = j["speed"].get<float>();
            if (j.contains("arriveDist"))    arriveDist    = j["arriveDist"].get<float>();
            if (j.contains("loopMode"))      loopMode      = (LoopMode)j["loopMode"].get<int>();
            if (j.contains("faceDirection")) faceDirection = j["faceDirection"].get<bool>();
            if (j.contains("autoStart"))     autoStart     = j["autoStart"].get<bool>();
            if (j.contains("showDebug"))     showDebug     = j["showDebug"].get<bool>();
        }
    };
}
