#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "../../core/Component.hpp"
#include "../../core/AssetManager.hpp"
#include "raylib.h"

namespace Indium
{
    /**
     * @brief Grid-based tilemap renderer.
     *
     * Loads a tileset texture and draws a cols×rows grid of tiles in world space.
     * Tile index -1 = empty. Tiles are indexed left→right, top→bottom in the tileset.
     *
     * Script usage:
     *   auto* tm = owner->getComponent<TilemapComponent>();
     *   tm->SetTile(3, 2, 5);   // column 3, row 2, tile index 5
     *   int t = tm->GetTile(3, 2);
     */
    struct TilemapComponent : Component
    {
        std::string tilesetPath = "";
        int   tileW     = 16;
        int   tileH     = 16;
        int   cols      = 20;
        int   rows      = 12;
        float tileScale = 1.0f;    // uniform world-space scale per tile

        // ---- Tile data ----
        std::vector<int> tiles;    // size = cols * rows, -1 = empty

        void Resize(int newCols, int newRows)
        {
            std::vector<int> next(newCols * newRows, -1);
            for (int r = 0; r < std::min(rows, newRows); ++r) {for (int c = 0; c < std::min(cols, newCols); ++c) next[r * newCols + c] = GetTile(c, r);}
            cols  = newCols;
            rows  = newRows;
            tiles = std::move(next);
        }

        void SetTile(int c, int r, int idx)
        {
            if (c < 0 || r < 0 || c >= cols || r >= rows) return;
            if ((int)tiles.size() != cols * rows) tiles.assign(cols * rows, -1);
            tiles[r * cols + c] = idx;
        }

        int GetTile(int c, int r) const
        {
            if (c < 0 || r < 0 || c >= cols || r >= rows) return -1;
            if ((int)tiles.size() != cols * rows) return -1;
            return tiles[r * cols + c];
        }

        void Fill(int idx)  { tiles.assign(cols * rows, idx); }
        void Clear()        { tiles.assign(cols * rows, -1); }

        bool LoadTileset(const std::string& path)
        {
            tilesetPath = path;
            tileset_    = AssetManager::Get().GetTexture(path);
            loaded_     = (tileset_.id > 0);
            return loaded_;
        }

        // ---- Component interface ----
        void update(float, Vector2, Scene*) override {}
        void draw() const override;
        void inspect(std::function<void()> snapshotCb = {}) override;
        std::string getName() const override;
        std::unique_ptr<Component> clone() const override;
        nlohmann::json serialize() const override;
        void deserialize(const nlohmann::json& j) override;

    private:
        Texture2D tileset_      = {};
        bool      loaded_       = false;
        int       selectedTile_ = 0;
    };
}
