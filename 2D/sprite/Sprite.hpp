#pragma once

#include "raylib.h"
#include "raymath.h"
#include "../../core/Entity.hpp"
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
        bool            textureLoaded = false;
        ::Rectangle     sourceRec; // Raylib's Rectangle struct (using :: to avoid conflict with Indium::Rectangle)

        Sprite()
        {
            name = "New Sprite";
            sourceRec = { 0, 0, 0, 0 };
        }

        ~Sprite()
        {
            if (textureLoaded)
            {
                UnloadTexture(texture);
            }
        }

        /** @brief Loads a texture from file. */
        bool Load(const std::string& path)
        {
            if (textureLoaded) UnloadTexture(texture);
            textureLoaded = false;

            // Try loading as image first for better error handling
            Image img = LoadImage(path.c_str());

            if (img.data != nullptr)
            {
                texture = LoadTextureFromImage(img);
                UnloadImage(img);

                if (texture.id > 0)
                {
                    textureLoaded = true;
                    sourceRec = { 0, 0, (float)texture.width, (float)texture.height };
                    scale = { (float)texture.width, (float)texture.height };
                    return true;
                }
            }

            std::cout << "ERROR: Failed to load texture at " << path << std::endl;
            return false;
        }

        void draw() const override
        {
            if (!textureLoaded)
            {
                // Draw a placeholder if no texture is loaded
                DrawRectanglePro({position.x, position.y, scale.x, scale.y}, {scale.x/2, scale.y/2}, rotation, RED);
                DrawText("No Texture", (int)position.x - 40, (int)position.y, 10, WHITE);
                return;
            }

            ::Rectangle destRec = { position.x, position.y, scale.x, scale.y };
            Vector2 origin = { scale.x / 2.0f, scale.y / 2.0f };

            DrawTexturePro(texture, sourceRec, destRec, origin, rotation, color);
        }

        std::vector<Vector2> getVertices() override
        {
            std::vector<Vector2> vertices(4);
            float hw = scale.x / 2.0f;
            float hh = scale.y / 2.0f;

            float rad = rotation * DEG2RAD;
            float c = cosf(rad);
            float s = sinf(rad);

            Vector2 corners[4] = {
                {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
            };

            for (int i = 0; i < 4; i++) {
                vertices[i].x = position.x + (corners[i].x * c - corners[i].y * s);
                vertices[i].y = position.y + (corners[i].x * s + corners[i].y * c);
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
            float hw = scale.x / 2.0f;
            float hh = scale.y / 2.0f;
            float dx = point.x - position.x;
            float dy = point.y - position.y;

            float rad = -rotation * DEG2RAD;
            float c = cosf(rad);
            float s = sinf(rad);

            float rx = dx * c - dy * s;
            float ry = dx * s + dy * c;

            return (rx >= -hw && rx <= hw && ry >= -hh && ry <= hh);
        }

        void inspect() override
        {
            Entity::inspect();

            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Sprite Properties");

            if (ImGui::Button("Select Texture..."))
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
                ImGui::Text("Texture size: %dx%d", texture.width, texture.height);
                ImGui::Separator();
                ImGui::Text("Source Rectangle");
                ImGui::DragFloat("Src X", &sourceRec.x, 1.0f);
                ImGui::DragFloat("Src Y", &sourceRec.y, 1.0f);
                ImGui::DragFloat("Src W", &sourceRec.width, 1.0f);
                ImGui::DragFloat("Src H", &sourceRec.height, 1.0f);
            }

            ImGui::Separator();
            ImGui::InputFloat("Rotation", &rotation, 1.0f, 10.0f);
            ImGui::InputFloat2("Position", &position.x);
            ImGui::InputFloat2("Scale", &scale.x);

            float col[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
            if (ImGui::ColorEdit4("Tint", col))
            {
                color = { (unsigned char)(col[0] * 255), (unsigned char)(col[1] * 255), (unsigned char)(col[2] * 255), (unsigned char)(col[3] * 255) };
            }
        }

        std::unique_ptr<Entity> clone() override
        {
            // Note: Struct copy is enough for now, but we need to handle texture ownership carefully.
            // Since Load() unloads the previous texture, we need to be sure cloned sprites don't
            // double-unload or use invalid IDs. For a simple editor, this works for now.
            return std::make_unique<Sprite>(*this);
        }
    };
}
