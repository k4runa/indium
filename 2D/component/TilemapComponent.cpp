#include "TilemapComponent.hpp"
#include "../../core/Entity.hpp"
#include "../../core/Screen.hpp"
#include "../../tools/FileBrowser.hpp"
#include "imgui.h"
#include <filesystem>
#include <cstdint>

namespace Indium
{
    void TilemapComponent::draw() const
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

                ::Rectangle src  = { (float)(srcC * tileW), (float)(srcR * tileH), (float)tileW, (float)tileH };
                ::Rectangle dest = { origin.x + (float)c * wTile, origin.y + (float)r * hTile, wTile, hTile };
                DrawTexturePro(tileset_, src, dest, {0,0}, 0.0f, WHITE);
            }
        }

        // Editor-only collision overlay: outline the merged solid rectangles so the
        // author can see exactly what blocks the player. Hidden during Play (the
        // editor clears DebugGizmos), like the collider / trigger gizmos.
        if (collisionEnabled && Screen::DebugGizmos())
            for (const auto& cr : GetSolidWorldRects())
            {
                // Solid = cyan box; one-way = orange box with a bright top edge (the
                // only side that blocks), so the author can tell them apart at a glance.
                if (cr.oneWay)
                {
                    DrawRectangleLinesEx(cr.rect, 1.0f, Color{ 255, 170, 40, 130 });
                    DrawLineEx({ cr.rect.x, cr.rect.y }, { cr.rect.x + cr.rect.width, cr.rect.y }, 2.5f, Color{ 255, 170, 40, 230 });
                }
                else
                {
                    DrawRectangleLinesEx(cr.rect, 1.5f, Color{ 0, 200, 255, 180 });
                }
            }
    }

    void TilemapComponent::inspect(std::function<void()> snapshotCb)
    {
        // ---- Tileset picker ----
        if (loaded_)
        {
            float previewH = 48.0f;
            float aspect   = (float)tileset_.width / (float)tileset_.height;
            float previewW = std::min(previewH * aspect, ImGui::GetContentRegionAvail().x - 80.0f);
            ImGui::Image((ImTextureID)(uintptr_t)tileset_.id, ImVec2(previewW, previewH), ImVec2(0,0), ImVec2(1,1));
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::TextColored(ImVec4(0.4f,0.8f,0.4f,1.0f), "%s", std::filesystem::path(tilesetPath).filename().string().c_str());
            int tsColCount = std::max(1, tileset_.width / tileW);
            int tsRowCount = std::max(1, tileset_.height / tileH);
            ImGui::TextDisabled("%d x %d tiles", tsColCount, tsRowCount);
            ImGui::EndGroup();
            ImGui::Spacing();
            if (ImGui::Button("Change Tileset...", ImVec2(-1,0))) ImGui::OpenPopup("TilesetBrowser");
        }
        else
        {
            ImGui::TextDisabled("(no tileset)");
            ImGui::Spacing();
            if (ImGui::Button("Select Tileset...", ImVec2(-1,0))) ImGui::OpenPopup("TilesetBrowser");
        }

        std::string selPath;
        if (FileBrowser::Draw("TilesetBrowser", selPath, {".png",".jpg",".bmp",".tga"})) LoadTileset(selPath);

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
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Grid (cols × rows)");
        int gridSz[2] = {cols, rows};
        ImGui::PushItemWidth(-1);
        if (ImGui::DragInt2("##GridSize", gridSz, 1.0f, 1, 512)) Resize(std::max(1, gridSz[0]), std::max(1, gridSz[1]));
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Text("Tile Scale");
        ImGui::PushItemWidth(-1);
        ImGui::DragFloat("##TileScl", &tileScale, 0.01f, 0.01f, 16.0f, "%.2f×");
        if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        ImGui::PopItemWidth();

        ImGui::Spacing();
        ImGui::TextDisabled("Collision");
        {
            bool col = collisionEnabled;
            if (ImGui::Checkbox("Solid tiles", &col))
            {
                if (snapshotCb) snapshotCb();
                collisionEnabled = col;
                collisionDirty_  = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Non-empty tiles become static solids that dynamic\nRigidbodies collide with. Leave off for background layers.\nMark individual tiles Pass-through or One-Way in the palette below.");
        }

        ImGui::Spacing();
        ImGui::Separator();

        if (!loaded_) return;

        // ---- Tileset palette ----
        ImGui::TextDisabled("Tile Palette");
        // Click mode: paint-select, or classify a tileset index for collision.
        ImGui::RadioButton("Select", &paletteMode_, 0); ImGui::SameLine();
        ImGui::RadioButton("Pass-through", &paletteMode_, 1); ImGui::SameLine();
        ImGui::RadioButton("One-Way", &paletteMode_, 2);
        if (paletteMode_ == 0)      ImGui::TextDisabled("Click a tile to paint with it.");
        else if (paletteMode_ == 1) ImGui::TextDisabled("Click tiles to toggle pass-through (decorative, no collision).");
        else                        ImGui::TextDisabled("Click tiles to toggle one-way (stand on top, jump up through).");
        {
            int tsColCount = std::max(1, tileset_.width  / tileW);
            int tsRowCount = std::max(1, tileset_.height / tileH);
            int totalTiles = tsColCount * tsRowCount;

            float avail    = ImGui::GetContentRegionAvail().x;
            float cellPx   = std::min(avail / (float)tsColCount, 32.0f);
            float palW     = cellPx * (float)tsColCount;
            float palH     = cellPx * (float)tsRowCount;

            ImVec2 palPos = ImGui::GetCursorScreenPos();
            // Default UVs (0,0)-(1,1): a file-loaded raylib texture is already upright
            // in ImGui (only RenderTextures need a V-flip). Must match the row-0-at-top
            // layout that the grid lines, overlays and click-mapping below assume.
            ImGui::Image((ImTextureID)(uintptr_t)tileset_.id, ImVec2(palW, palH), ImVec2(0,0), ImVec2(1,1));

            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Classification overlays: pass-through = grey wash, one-way = orange top bar.
            for (int t = 0; t < totalTiles; ++t)
            {
                int tc = t % tsColCount, tr = t / tsColCount;
                ImVec2 a = { palPos.x + tc * cellPx, palPos.y + tr * cellPx };
                ImVec2 b = { a.x + cellPx, a.y + cellPx };
                if      (IsIndexPassable(t)) dl->AddRectFilled(a, b, IM_COL32(35, 35, 35, 160));
                else if (IsIndexOneWay(t))   dl->AddLine({ a.x, a.y + 1.5f }, { b.x, a.y + 1.5f }, IM_COL32(255, 170, 40, 255), 3.0f);
            }

            // Grid lines
            for (int c = 0; c <= tsColCount; ++c) dl->AddLine({palPos.x + c*cellPx, palPos.y}, {palPos.x + c*cellPx, palPos.y + palH}, IM_COL32(80,80,80,120), 1.0f);
            for (int r = 0; r <= tsRowCount; ++r) dl->AddLine({palPos.x, palPos.y + r*cellPx}, {palPos.x + palW, palPos.y + r*cellPx}, IM_COL32(80,80,80,120), 1.0f);
            // Highlight selected tile
            if (selectedTile_ >= 0 && selectedTile_ < totalTiles)
            {
                int sc = selectedTile_ % tsColCount;
                int sr = selectedTile_ / tsColCount;
                dl->AddRect({palPos.x + sc*cellPx, palPos.y + sr*cellPx}, {palPos.x + (sc+1)*cellPx, palPos.y + (sr+1)*cellPx}, IM_COL32(0,220,255,255), 0.0f, 0, 2.0f);
            }

            // Click detection on palette — behaviour depends on the active mode.
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
            {
                ImVec2 mp = ImGui::GetIO().MousePos;
                int pc = (int)((mp.x - palPos.x) / cellPx);
                int pr = (int)((mp.y - palPos.y) / cellPx);
                if (pc >= 0 && pc < tsColCount && pr >= 0 && pr < tsRowCount)
                {
                    int idx = pr * tsColCount + pc;
                    if      (paletteMode_ == 0) selectedTile_ = idx;
                    else if (paletteMode_ == 1) { if (snapshotCb) snapshotCb(); SetIndexPassable(idx, !IsIndexPassable(idx)); }
                    else                        { if (snapshotCb) snapshotCb(); SetIndexOneWay(idx,   !IsIndexOneWay(idx)); }
                }
            }

            if (paletteMode_ == 0) ImGui::TextDisabled("Selected: tile %d", selectedTile_);
            else                   ImGui::TextDisabled("Pass-through: %d   One-Way: %d", (int)passableTiles_.size(), (int)oneWayTiles_.size());
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
                for (int r = 0; r < rows; ++r)
                {
                    for (int c = 0; c < cols; ++c)
                    {
                        int idx = GetTile(c, r);
                        ImVec2 tl = {mapPos.x + c*cellPx, mapPos.y + r*cellPx};
                        ImVec2 br = {tl.x + cellPx,       tl.y + cellPx};
                        if (idx < 0) { dl->AddRectFilled(tl, br, IM_COL32(40,40,40,180)); }
                        else
                        {
                            int sc = idx % tsColCount;
                            int sr = idx / tsColCount;
                            // Upright top-down source cell — matches DrawTexturePro in the
                            // scene and the (0,0)-(1,1) palette; no V-flip for a file texture.
                            float u0 = sc * uvW;
                            float v0 = sr * uvH;
                            float u1 = u0 + uvW;
                            float v1 = v0 + uvH;
                            dl->AddImage((ImTextureID)(uintptr_t)tileset_.id, tl, br, {u0,v0}, {u1,v1});
                        }
                    }
                }
            }

            // Grid lines
            for (int c = 0; c <= cols; ++c) dl->AddLine({mapPos.x + c*cellPx, mapPos.y}, {mapPos.x + c*cellPx, mapPos.y + mapH}, IM_COL32(80,80,80,100), 1.0f);
            for (int r = 0; r <= rows; ++r) dl->AddLine({mapPos.x,mapPos.y + r*cellPx}, {mapPos.x + mapW, mapPos.y + r*cellPx},IM_COL32(80,80,80,100), 1.0f);

            // Painting
            if (ImGui::IsItemHovered())
            {
                ImVec2 mp = ImGui::GetIO().MousePos;
                int pc = (int)((mp.x - mapPos.x) / cellPx);
                int pr = (int)((mp.y - mapPos.y) / cellPx);
                if (pc >= 0 && pc < cols && pr >= 0 && pr < rows)
                {
                    // Hover highlight
                    dl->AddRect({mapPos.x + pc*cellPx, mapPos.y + pr*cellPx}, {mapPos.x + (pc+1)*cellPx, mapPos.y + (pr+1)*cellPx},IM_COL32(255,255,255,160), 0.0f, 0, 1.5f);
                    if      (ImGui::IsMouseClicked(0) && snapshotCb) snapshotCb();
                    else if (ImGui::IsMouseClicked(1) && snapshotCb) snapshotCb();
                    if      (ImGui::IsMouseDown(0)) SetTile(pc, pr, selectedTile_);
                    else if (ImGui::IsMouseDown(1)) SetTile(pc, pr, -1);
                }
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Fill##TM", ImVec2(60,0)))      { if (snapshotCb) snapshotCb(); Fill(selectedTile_); }
        ImGui::SameLine();
        if (ImGui::Button("Clear All##TM", ImVec2(80,0))) { if (snapshotCb) snapshotCb(); Clear(); }
        ImGui::SameLine();
        ImGui::TextDisabled("%d × %d  (%d tiles)", cols, rows, cols * rows);
    }

    std::string TilemapComponent::getName() const { return "Tilemap"; }

    std::unique_ptr<Component> TilemapComponent::clone() const
    {
        auto c = std::make_unique<TilemapComponent>();
        c->enabled     = enabled;
        c->tilesetPath = tilesetPath;
        c->tileW       = tileW;
        c->tileH       = tileH;
        c->cols        = cols;
        c->rows        = rows;
        c->tileScale   = tileScale;
        c->collisionEnabled = collisionEnabled;
        c->passableTiles_   = passableTiles_;
        c->oneWayTiles_     = oneWayTiles_;
        c->tiles       = tiles;
        if (!tilesetPath.empty()) c->LoadTileset(tilesetPath);
        return c;
    }

    nlohmann::json TilemapComponent::serialize() const
    {
        nlohmann::json j = Component::serialize();
        j["tilesetPath"] = tilesetPath;
        j["tileW"]       = tileW;
        j["tileH"]       = tileH;
        j["cols"]        = cols;
        j["rows"]        = rows;
        j["tileScale"]   = tileScale;
        j["collisionEnabled"] = collisionEnabled;
        j["passableTiles"]    = passableTiles_;
        j["oneWayTiles"]      = oneWayTiles_;
        j["tiles"]       = tiles;
        return j;
    }

    void TilemapComponent::deserialize(const nlohmann::json& j)
    {
        Component::deserialize(j);
        if (j.contains("tileW"))     tileW     = j["tileW"].get<int>();
        if (j.contains("tileH"))     tileH     = j["tileH"].get<int>();
        if (j.contains("cols"))      cols      = j["cols"].get<int>();
        if (j.contains("rows"))      rows      = j["rows"].get<int>();
        if (j.contains("tileScale")) tileScale = j["tileScale"].get<float>();
        if (j.contains("collisionEnabled")) collisionEnabled = j["collisionEnabled"].get<bool>();
        if (j.contains("passableTiles")) passableTiles_ = j["passableTiles"].get<std::vector<int>>();
        if (j.contains("oneWayTiles"))   oneWayTiles_   = j["oneWayTiles"].get<std::vector<int>>();
        if (j.contains("tiles"))     tiles     = j["tiles"].get<std::vector<int>>();
        if (j.contains("tilesetPath") && !j["tilesetPath"].get<std::string>().empty()) LoadTileset(j["tilesetPath"].get<std::string>());
        collisionDirty_ = true;
    }
}
