#pragma once
#include "../../core/Entity.hpp"
#include "../../core/AssetManager.hpp"
#include "../../tools/FileBrowser.hpp"
#include "AnimatorComponent.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"
#include <string>

namespace Indium
{
    // --------------------------------------------------------------------
    // SpriteRendererComponent  —  draws a 2D texture on its owner entity.
    // Handles texture loading, source rect, AnimatorComponent integration,
    // and inspector UI (thumbnail + file browser + source rect editor).
    // --------------------------------------------------------------------
    struct SpriteRendererComponent : Component
    {
        Texture2D   texture;
        std::string texturePath;
        bool        textureLoaded = false;
        ::Rectangle sourceRec     = {0, 0, 0, 0};

        bool Load(const std::string& path)
        {
            textureLoaded = false;
            texturePath   = path;
            texture       = AssetManager::Get().GetTexture(path);
            if (texture.id > 0)
            {
                textureLoaded = true;
                sourceRec     = { 0, 0, (float)texture.width, (float)texture.height };
                if (owner)
                    owner->scale = { (float)texture.width, (float)texture.height };
                return true;
            }
            return false;
        }

        void update(float, Vector2, Scene*) override
        {
            if (!owner) return;
            auto* anim = owner->getComponent<AnimatorComponent>();
            if (anim && anim->playing && !anim->currentClip.empty())
                sourceRec = anim->getCurrentSourceRect();
        }

        void draw() const override
        {
            if (!owner || !owner->activeInHierarchy()) return;

            Vector2 gPos  = owner->getGlobalPosition();
            Vector2 gScl  = owner->getGlobalScale();
            float   gRot  = owner->getGlobalRotation();
            Color   col   = owner->color;

            if (!textureLoaded)
            {
                float drawW = fmaxf(gScl.x, 100.0f);
                float drawH = fmaxf(gScl.y, 100.0f);
                DrawRectanglePro({ gPos.x, gPos.y, drawW, drawH },
                                 { drawW * 0.5f, drawH * 0.5f }, gRot,
                                 ColorAlpha(RED, 0.3f));

                // Outline
                float hw = drawW * 0.5f, hh = drawH * 0.5f;
                float rad = gRot * DEG2RAD;
                float c = cosf(rad), s = sinf(rad);
                Vector2 corners[4] = { {-hw,-hh},{hw,-hh},{hw,hh},{-hw,hh} };
                for (int i = 0; i < 4; i++)
                {
                    Vector2 a = { gPos.x + corners[i].x * c - corners[i].y * s,
                                  gPos.y + corners[i].x * s + corners[i].y * c };
                    Vector2 b = { gPos.x + corners[(i+1)%4].x * c - corners[(i+1)%4].y * s,
                                  gPos.y + corners[(i+1)%4].x * s + corners[(i+1)%4].y * c };
                    DrawLineEx(a, b, 2.0f, RED);
                }
                DrawText("No Texture", (int)gPos.x - 30, (int)gPos.y - 5, 10, WHITE);
                return;
            }

            ::Rectangle destRec = { gPos.x, gPos.y, gScl.x, gScl.y };
            Vector2 origin = { gScl.x * 0.5f, gScl.y * 0.5f };
            DrawTexturePro(texture, sourceRec, destRec, origin, gRot, col);
        }

        void inspect() override
        {
            if (textureLoaded)
            {
                float previewH = 64.0f;
                float aspect   = (float)texture.width / (float)texture.height;
                float previewW = fminf(previewH * aspect, ImGui::GetContentRegionAvail().x - 80.0f);
                ImGui::Image((ImTextureID)(uintptr_t)texture.id,
                             ImVec2(previewW, previewH),
                             ImVec2(0, 1), ImVec2(1, 0));
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "%s",
                                   fs::path(texturePath).filename().string().c_str());
                ImGui::TextDisabled("%d x %d px", texture.width, texture.height);
                ImGui::EndGroup();
                ImGui::Spacing();
                if (ImGui::Button("Change Texture...", ImVec2(-1, 0)))
                    ImGui::OpenPopup("SprTex Browser");
            }
            else
            {
                ImGui::TextDisabled("(no texture)");
                ImGui::Spacing();
                if (ImGui::Button("Select Texture...", ImVec2(-1, 0)))
                    ImGui::OpenPopup("SprTex Browser");
            }

            std::string selectedPath;
            if (FileBrowser::Draw("SprTex Browser", selectedPath, { ".png", ".jpg", ".bmp", ".tga" }))
                Load(selectedPath);

            if (textureLoaded && ImGui::CollapsingHeader("Source Rectangle"))
            {
                ImGui::Indent(8.0f);
                ImGui::DragFloat("X##SrcX",      &sourceRec.x,      1.0f, 0.0f, (float)texture.width);
                if (ImGui::IsItemActivated() && Entity::_snapshotCb) Entity::_snapshotCb();
                ImGui::DragFloat("Y##SrcY",      &sourceRec.y,      1.0f, 0.0f, (float)texture.height);
                if (ImGui::IsItemActivated() && Entity::_snapshotCb) Entity::_snapshotCb();
                ImGui::DragFloat("Width##SrcW",  &sourceRec.width,  1.0f, 1.0f, (float)texture.width,  "%.0f");
                if (ImGui::IsItemActivated() && Entity::_snapshotCb) Entity::_snapshotCb();
                ImGui::DragFloat("Height##SrcH", &sourceRec.height, 1.0f, 1.0f, (float)texture.height, "%.0f");
                if (ImGui::IsItemActivated() && Entity::_snapshotCb) Entity::_snapshotCb();
                ImGui::Unindent(8.0f);
            }
        }

        std::string getName() const override { return "SpriteRenderer"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<SpriteRendererComponent>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["texturePath"] = texturePath;
            j["sourceRec"]   = { sourceRec.x, sourceRec.y, sourceRec.width, sourceRec.height };
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("sourceRec"))
            {
                sourceRec.x      = j["sourceRec"][0];
                sourceRec.y      = j["sourceRec"][1];
                sourceRec.width  = j["sourceRec"][2];
                sourceRec.height = j["sourceRec"][3];
            }
            if (j.contains("texturePath") && !j["texturePath"].get<std::string>().empty())
            {
                Load(j["texturePath"].get<std::string>());
                // Restore saved sourceRec — Load() overwrites it with full texture bounds
                if (j.contains("sourceRec"))
                {
                    sourceRec.x      = j["sourceRec"][0];
                    sourceRec.y      = j["sourceRec"][1];
                    sourceRec.width  = j["sourceRec"][2];
                    sourceRec.height = j["sourceRec"][3];
                }
            }
        }
    };

} // namespace Indium
