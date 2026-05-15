#pragma once

#include "raylib.h"
#include "raymath.h"
#include "../../core/Entity.hpp"
#include "../../core/AssetManager.hpp"
#include "../../tools/FileBrowser.hpp"
#include "imgui.h"
#include <memory>
#include <vector>
#include <string>

namespace Indium
{
    /**
     * @brief An entity that renders a 2D texture (sprite).
     */
    struct Sprite : Entity
    {
        Texture2D       texture;
        std::string     texturePath;
        bool            textureLoaded = false;
        ::Rectangle     sourceRec; // Raylib's Rectangle struct

        Sprite()
        {
            name = "New Sprite";
            sourceRec = { 0, 0, 0, 0 };
        }

        // No destructor needed - AssetManager owns the textures
        ~Sprite() = default;

        /** @brief Loads a texture via AssetManager (deduplicated). */
        bool Load(const std::string& path)
        {
            textureLoaded = false;
            texturePath = path;

            texture = AssetManager::Get().GetTexture(path);

            if (texture.id > 0)
            {
                textureLoaded = true;
                sourceRec = { 0, 0, (float)texture.width, (float)texture.height };
                scale = { (float)texture.width, (float)texture.height };
                return true;
            }

            return false;
        }

        void draw() const override
        {
            Vector2 gPos = getGlobalPosition();
            Vector2 gScale = getGlobalScale();
            float gRot = getGlobalRotation();

            if (!textureLoaded)
            {
                // Draw a placeholder if no texture is loaded
                DrawRectanglePro({gPos.x, gPos.y, gScale.x, gScale.y}, {gScale.x/2, gScale.y/2}, gRot, RED);
                DrawText("No Texture", (int)gPos.x - 40, (int)gPos.y, 10, WHITE);
                return;
            }

            ::Rectangle destRec = { gPos.x, gPos.y, gScale.x, gScale.y };
            Vector2 origin = { gScale.x / 2.0f, gScale.y / 2.0f };

            DrawTexturePro(texture, sourceRec, destRec, origin, gRot, color);
        }

        std::vector<Vector2> getVertices() override
        {
            std::vector<Vector2> vertices(4);

            Vector2 gPos = getGlobalPosition();
            Vector2 gScale = getGlobalScale();
            float gRot = getGlobalRotation();

            float hw = gScale.x / 2.0f;
            float hh = gScale.y / 2.0f;

            float rad = gRot * DEG2RAD;
            float c = cosf(rad);
            float s = sinf(rad);

            Vector2 corners[4] = {
                {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
            };

            for (int i = 0; i < 4; i++) {
                vertices[i].x = gPos.x + (corners[i].x * c - corners[i].y * s);
                vertices[i].y = gPos.y + (corners[i].x * s + corners[i].y * c);
            }

            return vertices;
        }

        bool collidesWith(Entity* other) override
        {
            return CheckCollisionRecs(getBounds(), other->getBounds());
        }

        ::Rectangle getBounds() override
        {
            std::vector<Vector2> verts = getVertices();
            float minX = INFINITY, minY = INFINITY, maxX = -INFINITY, maxY = -INFINITY;
            for (const auto& v : verts) {
                minX = fminf(minX, v.x);
                minY = fminf(minY, v.y);
                maxX = fmaxf(maxX, v.x);
                maxY = fmaxf(maxY, v.y);
            }
            return {minX, minY, maxX - minX, maxY - minY};
        }

        bool Contains(Vector2 point) override
        {
            Vector2 gPos = getGlobalPosition();
            Vector2 gScale = getGlobalScale();
            float gRot = getGlobalRotation();

            float hw = gScale.x / 2.0f;
            float hh = gScale.y / 2.0f;
            float dx = point.x - gPos.x;
            float dy = point.y - gPos.y;

            float rad = -gRot * DEG2RAD;
            float c = cosf(rad);
            float s = sinf(rad);

            float rx = dx * c - dy * s;
            float ry = dx * s + dy * c;

            return (rx >= -hw && rx <= hw && ry >= -hh && ry <= hh);
        }

        void inspect() override
        {
            Entity::inspect();

            // --- Sprite Renderer Section ---
            if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent(8.0f);

                if (ImGui::Button("Select Texture...", ImVec2(-1, 0)))
                {
                    ImGui::OpenPopup("Texture Browser");
                }

                std::string selectedPath;
                if (FileBrowser::Draw("Texture Browser", selectedPath, { ".png", ".jpg", ".bmp", ".tga" }))
                {
                    Load(selectedPath);
                }

                if (textureLoaded)
                {
                    ImGui::TextDisabled("Texture: %dx%d", texture.width, texture.height);

                    ImGui::Text("Source Rectangle");
                    ImGui::PushItemWidth(-1);
                    ImGui::DragFloat("##SrcX", &sourceRec.x, 1.0f);
                    ImGui::DragFloat("##SrcY", &sourceRec.y, 1.0f);
                    ImGui::DragFloat("##SrcW", &sourceRec.width, 1.0f);
                    ImGui::DragFloat("##SrcH", &sourceRec.height, 1.0f);
                    ImGui::PopItemWidth();
                }

                ImGui::Unindent(8.0f);
            }
        }

        std::unique_ptr<Entity> clone() override
        {
            return std::make_unique<Sprite>(*this);
        }

        std::string getType() const override
        {
            return "Sprite";
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Entity::serialize();
            j["texturePath"] = texturePath;
            j["sourceRec"] = { sourceRec.x, sourceRec.y, sourceRec.width, sourceRec.height };
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Entity::deserialize(j);

            if (j.contains("sourceRec"))
            {
                sourceRec.x = j["sourceRec"][0];
                sourceRec.y = j["sourceRec"][1];
                sourceRec.width = j["sourceRec"][2];
                sourceRec.height = j["sourceRec"][3];
            }

            if (j.contains("texturePath") && !j["texturePath"].get<std::string>().empty())
            {
                // We directly call Load to recreate the texture from the path
                Load(j["texturePath"].get<std::string>());

                // If sourceRec was saved, Load() might overwrite it with full texture bounds.
                // We should restore the saved sourceRec after loading.
                if (j.contains("sourceRec"))
                {
                    sourceRec.x = j["sourceRec"][0];
                    sourceRec.y = j["sourceRec"][1];
                    sourceRec.width = j["sourceRec"][2];
                    sourceRec.height = j["sourceRec"][3];
                }
            }
        }
    };
}
