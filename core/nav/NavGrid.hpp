#pragma once
#include "raylib.h"
#include "raymath.h"
#include "../scene/Scene.hpp"
#include "../../2D/component/Collider2D.hpp"
#include <vector>
#include <queue>
#include <cmath>
#include <algorithm>

namespace Indium
{
    // --------------------------------------------------------------------
    // NavGrid — a uniform occupancy grid built from the scene's solid
    // colliders, with 8-directional A* pathfinding on top.
    //
    // Cells overlapped by any non-trigger collider (optionally inflated by
    // an agent radius) are marked blocked. FindPath() returns a list of
    // world-space waypoints from start to goal, with collinear points
    // collapsed so the agent walks smooth straight runs.
    //
    // Cheap enough to rebuild on a repath interval (e.g. a 60×34 grid for a
    // 1920×1080 world at 32px cells). Not meant for thousands of agents.
    // --------------------------------------------------------------------
    class NavGrid
    {
    public:
        // Build the grid covering [0,0]..worldSize. `ignore` is excluded from
        // obstacle marking (the agent's own entity). `agentRadius` inflates
        // obstacles so the path keeps clearance from walls.
        void Build(const Scene& scene, float cellSize, float agentRadius = 0.0f,
                   const Entity* ignore = nullptr, const ::Rectangle* region = nullptr)
        {
            cell_  = (cellSize > 1.0f) ? cellSize : 32.0f;
            cols_  = std::max(1, (int)std::ceil(scene.worldSize.x / cell_));
            rows_  = std::max(1, (int)std::ceil(scene.worldSize.y / cell_));
            blocked_.assign((size_t)cols_ * rows_, false);

            // If a navigation region is supplied, block every cell whose center
            // lies outside it so agents stay within the walkable rectangle.
            if (region)
            {
                for (int cy = 0; cy < rows_; cy++)
                    for (int cx = 0; cx < cols_; cx++)
                    {
                        Vector2 c = CellCenter(cx, cy);
                        if (!CheckCollisionPointRec(c, *region)) blocked_[idx_(cx, cy)] = true;
                    }
            }

            const float inflate = agentRadius;

            for (const auto& e : scene.entities)
            {
                if (e.get() == ignore) continue;
                if (!e->activeInHierarchy()) continue;
                auto* col = e->getComponent<Collider2D>();
                if (!col || col->isTrigger) continue;

                ::Rectangle b = col->getBounds();
                // Inflate by agent radius for clearance
                b.x -= inflate; b.y -= inflate;
                b.width += inflate * 2.0f; b.height += inflate * 2.0f;

                int cx0 = (int)std::floor(b.x / cell_);
                int cy0 = (int)std::floor(b.y / cell_);
                int cx1 = (int)std::floor((b.x + b.width)  / cell_);
                int cy1 = (int)std::floor((b.y + b.height) / cell_);

                for (int cy = cy0; cy <= cy1; cy++)
                    for (int cx = cx0; cx <= cx1; cx++)
                        if (inBounds_(cx, cy)) blocked_[idx_(cx, cy)] = true;
            }
        }

        bool IsBlocked(int cx, int cy) const
        {
            if (!inBounds_(cx, cy)) return true;
            return blocked_[idx_(cx, cy)];
        }

        int Cols() const { return cols_; }
        int Rows() const { return rows_; }
        float CellSize() const { return cell_; }

        Vector2 CellCenter(int cx, int cy) const
        {
            return { (cx + 0.5f) * cell_, (cy + 0.5f) * cell_ };
        }

        // A* from start to goal (world space). Returns smoothed waypoints
        // (excluding the start cell). Empty if unreachable.
        std::vector<Vector2> FindPath(Vector2 startW, Vector2 goalW) const
        {
            std::vector<Vector2> out;
            if (cols_ <= 0 || rows_ <= 0) return out;

            int sx = clampCol_((int)std::floor(startW.x / cell_));
            int sy = clampRow_((int)std::floor(startW.y / cell_));
            int gx = clampCol_((int)std::floor(goalW.x  / cell_));
            int gy = clampRow_((int)std::floor(goalW.y  / cell_));

            // If goal is blocked, snap to the nearest free cell around it.
            if (IsBlocked(gx, gy)) { if (!nearestFree_(gx, gy, gx, gy)) return out; }
            // If start is blocked (spawned inside geometry), snap too.
            if (IsBlocked(sx, sy)) { if (!nearestFree_(sx, sy, sx, sy)) return out; }

            const int N = cols_ * rows_;
            std::vector<float> g(N, INFINITY);
            std::vector<int>   came(N, -1);
            std::vector<bool>  closed(N, false);

            auto h = [&](int cx, int cy)
            {
                float dx = (float)std::abs(cx - gx);
                float dy = (float)std::abs(cy - gy);
                // Octile distance
                return (dx + dy) + (1.41421356f - 2.0f) * std::min(dx, dy);
            };

            using Node = std::pair<float, int>; // (f, index)
            std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

            int startI = idx_(sx, sy);
            int goalI  = idx_(gx, gy);
            g[startI]  = 0.0f;
            open.push({ h(sx, sy), startI });

            const int dirs[8][2] = {
                {1,0},{-1,0},{0,1},{0,-1},
                {1,1},{1,-1},{-1,1},{-1,-1}
            };

            bool found = false;
            while (!open.empty())
            {
                int cur = open.top().second;
                open.pop();
                if (closed[cur]) continue;
                closed[cur] = true;
                if (cur == goalI) { found = true; break; }

                int cx = cur % cols_;
                int cy = cur / cols_;

                for (auto& d : dirs)
                {
                    int nx = cx + d[0];
                    int ny = cy + d[1];
                    if (!inBounds_(nx, ny)) continue;
                    if (blocked_[idx_(nx, ny)]) continue;

                    // Prevent cutting through diagonal wall gaps
                    if (d[0] != 0 && d[1] != 0)
                    {
                        if (blocked_[idx_(cx + d[0], cy)] || blocked_[idx_(cx, cy + d[1])])
                            continue;
                    }

                    int ni = idx_(nx, ny);
                    if (closed[ni]) continue;

                    float step = (d[0] != 0 && d[1] != 0) ? 1.41421356f : 1.0f;
                    float ng   = g[cur] + step;
                    if (ng < g[ni])
                    {
                        g[ni]    = ng;
                        came[ni] = cur;
                        open.push({ ng + h(nx, ny), ni });
                    }
                }
            }

            if (!found) return out;

            // Reconstruct cell path (goal → start), then reverse
            std::vector<int> cells;
            for (int c = goalI; c != -1; c = came[c]) cells.push_back(c);
            std::reverse(cells.begin(), cells.end());

            // Collapse collinear runs so we only keep corner waypoints.
            std::vector<Vector2> pts;
            pts.reserve(cells.size());
            for (int c : cells) pts.push_back(CellCenter(c % cols_, c / cols_));

            if (pts.size() <= 2) { /* keep as-is, but drop the start cell below */ }

            std::vector<Vector2> simplified;
            for (size_t i = 0; i < pts.size(); i++)
            {
                if (i == 0 || i + 1 == pts.size()) { simplified.push_back(pts[i]); continue; }
                Vector2 a = pts[i - 1], b = pts[i], c = pts[i + 1];
                Vector2 d1 = { b.x - a.x, b.y - a.y };
                Vector2 d2 = { c.x - b.x, c.y - b.y };
                // Keep the point only if direction changes (cross product != 0)
                float cross = d1.x * d2.y - d1.y * d2.x;
                if (std::fabs(cross) > 0.001f) simplified.push_back(b);
            }

            // Drop the first waypoint (the start cell center) so the agent
            // heads to the next corner immediately instead of backtracking.
            for (size_t i = 1; i < simplified.size(); i++) out.push_back(simplified[i]);
            // Always make the true goal the final waypoint.
            if (!out.empty()) out.back() = goalW;
            else out.push_back(goalW);
            return out;
        }

    private:
        float cell_ = 32.0f;
        int   cols_ = 0;
        int   rows_ = 0;
        std::vector<bool> blocked_;

        int  idx_(int cx, int cy) const { return cy * cols_ + cx; }
        bool inBounds_(int cx, int cy) const { return cx >= 0 && cy >= 0 && cx < cols_ && cy < rows_; }
        int  clampCol_(int cx) const { return std::clamp(cx, 0, cols_ - 1); }
        int  clampRow_(int cy) const { return std::clamp(cy, 0, rows_ - 1); }

        // Spiral outward from (cx,cy) to find the closest non-blocked cell.
        bool nearestFree_(int cx, int cy, int& outX, int& outY) const
        {
            for (int r = 1; r < std::max(cols_, rows_); r++)
            {
                for (int dy = -r; dy <= r; dy++)
                    for (int dx = -r; dx <= r; dx++)
                    {
                        if (std::abs(dx) != r && std::abs(dy) != r) continue; // ring only
                        int nx = cx + dx, ny = cy + dy;
                        if (inBounds_(nx, ny) && !blocked_[idx_(nx, ny)])
                        { outX = nx; outY = ny; return true; }
                    }
            }
            return false;
        }
    };
}
