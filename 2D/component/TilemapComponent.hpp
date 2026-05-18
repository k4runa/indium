#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include "../../core/Component.hpp"
#include "../../core/AssetManager.hpp"
#include "../../tools/FileBrowser.hpp"
#include "raylib.h"
#include "imgui.h"

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
            for (int r = 0; r < std::min(rows, newRows); ++r)
                for (int c = 0; c < std::min(cols, newCols); ++c)
                    next[r * newCols + c] = GetTile(c, r);
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

        void Fill(int idx) { tiles.assign(cols * rows, idx); }
        void Clear() { tiles.assign(cols * rows, -1); }

        // ---- Tileset loading ----
        bool LoadTileset(const std::string& path)
        {
            tilesetPath = path;
            tileset_    = AssetManager::Get().GetTexture(path);
            loaded_     = (tileset_.id > 0);
            return loaded_;
        }

        // ---- Component interface ----
        void update(float, Vector2, Scene*) override {}

        void draw() const override
        {
            if (!owner || !loaded_ || tiles.empty()) return;

            Vector2 origin = owner->getGlobalPosition();
            float   wTile  = (float)tileW * tileScale;
            float   hTile  = (float)tileH * tileScale;
            int     tsColCount = std::max(1, tileset_.width / tileW);

            for (int r = 0; r < rows; ++r)
            {
                for (int c = 0; c < cols; ++c)
                {
                    int idx = GetTile(c, r);
                    if (idx < 0) continue;

                    int srcC = idx % tsColCount;
                    int srcR = idx / tsColCount;

                    ::Rectangle src  = { (float)(srcC * tileW), (float)(srcR * tileH),
                                         (float)tileW,           (float)tileH };
                    ::Rectangle dest = { origin.x + (float)c * wTile,
                                         origin.y + (float)r * hTile,
                                         wTile, hTile };
                    DrawTexturePro(tileset_, src, dest, {0,0}, 0.0f, WHITE);
                }
            }
        }

        void inspect() override
        {
            // ---- Tileset picker ----
            if (loaded_)
            {
                float previewH = 48.0f;
                float aspect   = (float)tileset_.width / (float)tileset_.height;
                float previewW = std::min(previewH * aspect,
                                          ImGui::GetContentRegionAvail().x - 80.0f);
                ImGui::Image((ImTextureID)(uintptr_t)tileset_.id,
                             ImVec2(previewW, previewH),
                             ImVec2(0,1), ImVec2(1,0));
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::TextColored(ImVec4(0.4f,0.8f,0.4f,1.0f), "%s",
                    std::filesystem::path(tilesetPath).filename().string().c_str());
                int tsColCount = std::max(1, tileset_.width / tileW);
                int tsRowCount = std::max(1, tileset_.height / tileH);
                ImGui::TextDisabled("%d x %d tiles", tsColCount, tsRowCount);
                ImGui::EndGroup();
                ImGui::Spacing();
                if (ImGui::Button("Change Tileset...", ImVec2(-1,0)))
                    ImGui::OpenPopup("TilesetBrowser");
            }
            else
            {
                ImGui::TextDisabled("(no tileset)");
                ImGui::Spacing();
                if (ImGui::Button("Select Tileset...", ImVec2(-1,0)))
                    ImGui::OpenPopup("TilesetBrowser");
            }

            std::string selPath;
            if (FileBrowser::Draw("TilesetBrowser", selPath, {".png",".jpg",".bmp",".tga"}))
                LoadTileset(selPath);

            ImGui::Spacing();
            ImGui::Separator();

            // ---- Grid settings ----
            ImGui::TextDisabled("Grid");
            ImGui::Text("Tile Size (px)");
            int tileSize[2] = {tileW, tileH};
            ImGui::PushItemWidth(-1);
            if (ImGui::DragInt2("##TileSize", tileSize, 1.0f, 1, 512))
            {
                tileW = std::max(1, tileSize[0]);
                tileH = std::max(1, tileSize[1]);
            }
            ImGui::PopItemWidth();

            ImGui::Text("Grid (cols × rows)");
            int gridSz[2] = {cols, rows};
            ImGui::PushItemWidth(-1);
            if (ImGui::DragInt2("##GridSize", gridSz, 1.0f, 1, 512))
                Resize(std::max(1, gridSz[0]), std::max(1, gridSz[1]));
            ImGui::PopItemWidth();

            ImGui::Text("Tile Scale");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##TileScl", &tileScale, 0.01f, 0.01f, 16.0f, "%.2f×");
            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::Separator();

            if (!loaded_) return;

            // ---- Tileset palette ----
            ImGui::TextDisabled("Tile Palette  (click to select)");
            {
                int tsColCount = std::max(1, tileset_.width  / tileW);
                int tsRowCount = std::max(1, tileset_.height / tileH);
                int totalTiles = tsColCount * tsRowCount;

                float avail    = ImGui::GetContentRegionAvail().x;
                float cellPx   = std::min(avail / (float)tsColCount, 32.0f);
                float palW     = cellPx * (float)tsColCount;
                float palH     = cellPx * (float)tsRowCount;

                ImVec2 palPos = ImGui::GetCursorScreenPos();
                ImGui::Image((ImTextureID)(uintptr_t)tileset_.id,
                             ImVec2(palW, palH), ImVec2(0,1), ImVec2(1,0));

                ImDrawList* dl = ImGui::GetWindowDrawList();

                // Grid lines
                for (int c = 0; c <= tsColCount; ++c)
                    dl->AddLine({palPos.x + c*cellPx, palPos.y},
                                {palPos.x + c*cellPx, palPos.y + palH},
                                IM_COL32(80,80,80,120), 1.0f);
                for (int r = 0; r <= tsRowCount; ++r)
                    dl->AddLine({palPos.x, palPos.y + r*cellPx},
                                {palPos.x + palW, palPos.y + r*cellPx},
                                IM_COL32(80,80,80,120), 1.0f);

                // Highlight selected tile
                if (selectedTile_ >= 0 && selectedTile_ < totalTiles)
                {
                    int sc = selectedTile_ % tsColCount;
                    int sr = selectedTile_ / tsColCount;
                    dl->AddRect({palPos.x + sc*cellPx, palPos.y + sr*cellPx},
                                {palPos.x + (sc+1)*cellPx, palPos.y + (sr+1)*cellPx},
                                IM_COL32(0,220,255,255), 0.0f, 0, 2.0f);
                }

                // Click detection on palette
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
                {
                    ImVec2 mp = ImGui::GetIO().MousePos;
                    int pc = (int)((mp.x - palPos.x) / cellPx);
                    int pr = (int)((mp.y - palPos.y) / cellPx);
                    if (pc >= 0 && pc < tsColCount && pr >= 0 && pr < tsRowCount)
                        selectedTile_ = pr * tsColCount + pc;
                }

                ImGui::TextDisabled("Selected: tile %d", selectedTile_);
            }

            ImGui::Spacing();
            ImGui::Separator();

            // ---- Map editor ----
            ImGui::TextDisabled("Map Editor  (LMB paint / RMB erase)");

            {
                float avail  = ImGui::GetContentRegionAvail().x;
                float cellPx = std::min(avail / (float)cols, 20.0f);
                float mapW   = cellPx * (float)cols;
                float mapH   = cellPx * (float)rows;

                ImVec2 mapPos = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##TileMap", ImVec2(mapW, mapH));
                ImDrawList* dl = ImGui::GetWindowDrawList();

                // Draw tile colors
                if (loaded_)
                {
                    int tsColCount = std::max(1, tileset_.width / tileW);
                    float uvW = (float)tileW / (float)tileset_.width;
                    float uvH = (float)tileH / (float)tileset_.height;
                    // Flip V for raylib texture coordinate convention
                    for (int r = 0; r < rows; ++r)
                    {
                        for (int c = 0; c < cols; ++c)
                        {
                            int idx = GetTile(c, r);
                            ImVec2 tl = {mapPos.x + c*cellPx, mapPos.y + r*cellPx};
                            ImVec2 br = {tl.x + cellPx,       tl.y + cellPx};
                            if (idx < 0)
                            {
                                dl->AddRectFilled(tl, br, IM_COL32(40,40,40,180));
                            }
                            else
                            {
                                int sc = idx % tsColCount;
                                int sr = idx / tsColCount;
                                float u0 = sc * uvW;
                                float v0 = 1.0f - (sr + 1) * uvH;
                                float u1 = u0 + uvW;
                                float v1 = v0 + uvH;
                                dl->AddImage((ImTextureID)(uintptr_t)tileset_.id,
                                             tl, br, {u0,v0}, {u1,v1});
                            }
                        }
                    }
                }

                // Grid lines
                for (int c = 0; c <= cols; ++c)
                    dl->AddLine({mapPos.x + c*cellPx, mapPos.y},
                                {mapPos.x + c*cellPx, mapPos.y + mapH},
                                IM_COL32(80,80,80,100), 1.0f);
                for (int r = 0; r <= rows; ++r)
                    dl->AddLine({mapPos.x,        mapPos.y + r*cellPx},
                                {mapPos.x + mapW, mapPos.y + r*cellPx},
                                IM_COL32(80,80,80,100), 1.0f);

                // Painting
                if (ImGui::IsItemHovered())
                {
                    ImVec2 mp = ImGui::GetIO().MousePos;
                    int pc = (int)((mp.x - mapPos.x) / cellPx);
                    int pr = (int)((mp.y - mapPos.y) / cellPx);
                    if (pc >= 0 && pc < cols && pr >= 0 && pr < rows)
                    {
                        // Hover highlight
                        dl->AddRect({mapPos.x + pc*cellPx, mapPos.y + pr*cellPx},
                                    {mapPos.x + (pc+1)*cellPx, mapPos.y + (pr+1)*cellPx},
                                    IM_COL32(255,255,255,160), 0.0f, 0, 1.5f);

                        if (ImGui::IsMouseDown(0))
                            SetTile(pc, pr, selectedTile_);
                        else if (ImGui::IsMouseDown(1))
                            SetTile(pc, pr, -1);
                    }
                }
            }

            ImGui::Spacing();
            if (ImGui::Button("Fill##TM", ImVec2(60,0)))      Fill(selectedTile_);
            ImGui::SameLine();
            if (ImGui::Button("Clear All##TM", ImVec2(80,0))) Clear();
            ImGui::SameLine();
            ImGui::TextDisabled("%d × %d  (%d tiles)", cols, rows, cols * rows);
        }

        std::string getName() const override { return "Tilemap"; }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<TilemapComponent>();
            c->tilesetPath = tilesetPath;
            c->tileW       = tileW;
            c->tileH       = tileH;
            c->cols        = cols;
            c->rows        = rows;
            c->tileScale   = tileScale;
            c->tiles       = tiles;
            if (!tilesetPath.empty()) c->LoadTileset(tilesetPath);
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["tilesetPath"] = tilesetPath;
            j["tileW"]       = tileW;
            j["tileH"]       = tileH;
            j["cols"]        = cols;
            j["rows"]        = rows;
            j["tileScale"]   = tileScale;
            j["tiles"]       = tiles;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("tileW"))     tileW     = j["tileW"].get<int>();
            if (j.contains("tileH"))     tileH     = j["tileH"].get<int>();
            if (j.contains("cols"))      cols      = j["cols"].get<int>();
            if (j.contains("rows"))      rows      = j["rows"].get<int>();
            if (j.contains("tileScale")) tileScale = j["tileScale"].get<float>();
            if (j.contains("tiles"))     tiles     = j["tiles"].get<std::vector<int>>();
            if (j.contains("tilesetPath") && !j["tilesetPath"].get<std::string>().empty())
                LoadTileset(j["tilesetPath"].get<std::string>());
        }

    private:
        Texture2D tileset_     = {};
        bool      loaded_      = false;
        int       selectedTile_ = 0;
    };
}
