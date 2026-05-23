#pragma once
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cmath>
#include "raylib.h"

namespace Indium
{
    class SpatialGrid
    {
    public:
        void Clear() { cells_.clear(); }
        void SetCellSize(float size) { cellSize_ = size > 0.0f ? size : 64.0f; }

        void Insert(int idx, ::Rectangle aabb)
        {
            int x0 = cellCoord(aabb.x),               y0 = cellCoord(aabb.y);
            int x1 = cellCoord(aabb.x + aabb.width),  y1 = cellCoord(aabb.y + aabb.height);
            for (int cx = x0; cx <= x1; ++cx) for (int cy = y0; cy <= y1; ++cy) cells_[pack(cx, cy)].push_back(idx);
        }

        std::vector<int> Query(::Rectangle aabb) const
        {
            std::vector<int> result;
            int x0 = cellCoord(aabb.x),               y0 = cellCoord(aabb.y);
            int x1 = cellCoord(aabb.x + aabb.width),  y1 = cellCoord(aabb.y + aabb.height);
            for (int cx = x0; cx <= x1; ++cx)
                for (int cy = y0; cy <= y1; ++cy)
                {
                    auto it = cells_.find(pack(cx, cy));
                    if (it != cells_.end()) result.insert(result.end(), it->second.begin(), it->second.end());
                }
            return result;
        }

    private:
        float cellSize_ = 128.0f;
        std::unordered_map<uint64_t, std::vector<int>> cells_;

        int cellCoord(float v) const { return static_cast<int>(floorf(v / cellSize_)); }

        static uint64_t pack(int x, int y)
        {
            return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(y));
        }
    };
}
