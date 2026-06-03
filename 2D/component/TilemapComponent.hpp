#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "../../core/Component.hpp"
#include "../../core/AssetManager.hpp"
#include "../../core/Entity.hpp"
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

        /** @brief When true, tiles act as static collision geometry: dynamic
         *  Rigidbodies collide with them and Scene OverlapBox/OverlapCircle/Raycast
         *  report them. Per-tilemap, so a background layer can stay non-solid. */
        bool  collisionEnabled = false;

        /** @brief A merged collision rectangle plus its behaviour. A one-way rect only
         *  blocks a body landing on it from above (pass up through, stand on top). */
        struct SolidRect { ::Rectangle rect; bool oneWay = false; };

        /** @brief Per-tileset-index collision overrides. By default every non-empty
         *  tile is Solid; list a tileset index here to make every cell drawn with it
         *  pass-through (decorative) or a one-way platform instead. Serialized, so the
         *  authored classification travels with the scene. */
        std::vector<int> passableTiles_;   // tileset indices that are NON-solid (decorative)
        std::vector<int> oneWayTiles_;     // tileset indices that are one-way platforms

        // ---- Tile data ----
        std::vector<int> tiles;    // size = cols * rows, -1 = empty

        void Resize(int newCols, int newRows)
        {
            std::vector<int> next(newCols * newRows, -1);
            for (int r = 0; r < std::min(rows, newRows); ++r) {for (int c = 0; c < std::min(cols, newCols); ++c) next[r * newCols + c] = GetTile(c, r);}
            cols  = newCols;
            rows  = newRows;
            tiles = std::move(next);
            collisionDirty_ = true;
        }

        void SetTile(int c, int r, int idx)
        {
            if (c < 0 || r < 0 || c >= cols || r >= rows) return;
            if ((int)tiles.size() != cols * rows) tiles.assign(cols * rows, -1);
            tiles[r * cols + c] = idx;
            collisionDirty_ = true;
        }

        int GetTile(int c, int r) const
        {
            if (c < 0 || r < 0 || c >= cols || r >= rows) return -1;
            if ((int)tiles.size() != cols * rows) return -1;
            return tiles[r * cols + c];
        }

        void Fill(int idx)  { tiles.assign(cols * rows, idx); collisionDirty_ = true; }
        void Clear()        { tiles.assign(cols * rows, -1);  collisionDirty_ = true; }

        bool LoadTileset(const std::string& path)
        {
            tilesetPath = path;
            tileset_    = AssetManager::Get().GetTexture(path);
            loaded_     = (tileset_.id > 0);
            return loaded_;
        }

        // ---- Collision ----

        /** @brief Collision behaviour of a tileset index or cell. */
        enum class TileCollision { None, Solid, OneWay };

        /** @brief Behaviour of a tileset index: one-way and pass-through overrides win
         *  over the default (Solid). -1 (empty) is never collidable. */
        TileCollision IndexCollision(int tileIndex) const
        {
            if (tileIndex < 0)         return TileCollision::None;
            if (IsIndexOneWay(tileIndex))   return TileCollision::OneWay;
            if (IsIndexPassable(tileIndex)) return TileCollision::None;
            return TileCollision::Solid;
        }

        /** @brief Collision behaviour of the cell at (c, r). */
        TileCollision CellCollision(int c, int r) const { return IndexCollision(GetTile(c, r)); }

        /** @brief A cell participates in collision when it is Solid or OneWay. */
        bool IsSolidTile(int c, int r) const { return CellCollision(c, r) != TileCollision::None; }

        // --- Per-tileset-index classification (editor / script) ---
        bool IsIndexOneWay(int idx)   const { return std::find(oneWayTiles_.begin(),  oneWayTiles_.end(),  idx) != oneWayTiles_.end(); }
        bool IsIndexPassable(int idx) const { return std::find(passableTiles_.begin(), passableTiles_.end(), idx) != passableTiles_.end(); }

        /** @brief Mark/unmark a tileset index as pass-through (decorative, no collision). */
        void SetIndexPassable(int idx, bool on)
        {
            EraseIndex_(oneWayTiles_, idx);                 // a tile is one class at a time
            if (on) { if (!IsIndexPassable(idx)) passableTiles_.push_back(idx); }
            else    EraseIndex_(passableTiles_, idx);
            collisionDirty_ = true;
        }

        /** @brief Mark/unmark a tileset index as a one-way platform (blocks from above only). */
        void SetIndexOneWay(int idx, bool on)
        {
            EraseIndex_(passableTiles_, idx);
            if (on) { if (!IsIndexOneWay(idx)) oneWayTiles_.push_back(idx); }
            else    EraseIndex_(oneWayTiles_, idx);
            collisionDirty_ = true;
        }

        /** @brief Solid / one-way cells greedy-merged into a minimal set of world-space
         *  rectangles (each tagged one-way or not). The tile-unit merge is cached and
         *  only recomputed when the tile pattern / classification changes; the owner
         *  transform / tile size / scale are reapplied on every call. Inline so
         *  script-DLL copies of the Scene queries resolve it without an engine import. */
        const std::vector<SolidRect>& GetSolidWorldRects() const
        {
            if (collisionDirty_) RebuildCollisionRects_();
            collisionWorldRects_.clear();
            collisionWorldRects_.reserve(collisionLocalRects_.size());
            const Vector2 origin = owner ? owner->getGlobalPosition() : Vector2{ 0.0f, 0.0f };
            const float   w      = (float)tileW * tileScale;
            const float   h      = (float)tileH * tileScale;
            for (const auto& lr : collisionLocalRects_)
                collisionWorldRects_.push_back(SolidRect{ ::Rectangle{ origin.x + lr.rect.x * w, origin.y + lr.rect.y * h, lr.rect.width * w, lr.rect.height * h }, lr.oneWay });
            return collisionWorldRects_;
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
        int       paletteMode_  = 0;   // editor palette click mode: 0 Select, 1 Pass-through, 2 One-Way

        static void EraseIndex_(std::vector<int>& v, int idx)
        {
            v.erase(std::remove(v.begin(), v.end(), idx), v.end());
        }

        // ---- Collision cache (runtime; not serialized) ----
        // Local rects are in TILE units {x,y,w,h}; GetSolidWorldRects() applies the
        // owner transform. Rebuilt only when the tile pattern / classification changes.
        mutable std::vector<SolidRect> collisionLocalRects_;
        mutable std::vector<SolidRect> collisionWorldRects_;
        mutable bool                   collisionDirty_ = true;

        // Greedy-merge runs of same-behaviour cells (Solid with Solid, OneWay with
        // OneWay) into larger rectangles. Merging removes the internal seams that make
        // a body snag when sliding across a flat row; keeping the classes separate
        // means a one-way platform never fuses with the solid ground beneath it.
        void RebuildCollisionRects_() const
        {
            collisionLocalRects_.clear();
            collisionDirty_ = false;
            if (cols <= 0 || rows <= 0) return;
            std::vector<unsigned char> used((size_t)cols * rows, 0);
            for (int r = 0; r < rows; ++r)
            {
                for (int c = 0; c < cols; ++c)
                {
                    const TileCollision cls = CellCollision(c, r);
                    if (cls == TileCollision::None || used[(size_t)r * cols + c]) continue;
                    int w = 1;
                    while (c + w < cols && CellCollision(c + w, r) == cls && !used[(size_t)r * cols + c + w]) ++w;
                    int h = 1;
                    while (r + h < rows)
                    {
                        bool full = true;
                        for (int cc = c; cc < c + w; ++cc)
                            if (CellCollision(cc, r + h) != cls || used[(size_t)(r + h) * cols + cc]) { full = false; break; }
                        if (!full) break;
                        ++h;
                    }
                    for (int rr = r; rr < r + h; ++rr)
                        for (int cc = c; cc < c + w; ++cc)
                            used[(size_t)rr * cols + cc] = 1;
                    collisionLocalRects_.push_back(SolidRect{ ::Rectangle{ (float)c, (float)r, (float)w, (float)h }, cls == TileCollision::OneWay });
                }
            }
        }
    };
}
