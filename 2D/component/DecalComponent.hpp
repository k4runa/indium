#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../core/AssetManager.hpp"
#include "../../core/ScriptManager.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "imgui.h"
#include <string>
#include <cstring>
#include <filesystem>

namespace Indium
{
    // --------------------------------------------------------------------
    // DecalComponent
    //
    // A flat texture stamp drawn at the entity's transform — bullet holes,
    // scorch marks, blood splats, footprints, graffiti.  Unlike a Sprite,
    // a decal can fade out over a lifetime and optionally draw additively
    // (for glowing marks / light splats).
    //
    // Spawn one from a script for a transient mark:
    //   auto* d = hit->AddComponent<DecalComponent>();
    //   d->texturePath = "Assets/scorch.png"; d->Load();
    //   d->fadeOut = true; d->lifetime = 3.0f;
    // --------------------------------------------------------------------
    struct DecalComponent : Component
    {
        std::string texturePath = "";
        Color       tint        = { 255, 255, 255, 255 };
        bool        additive    = false;
        bool        fadeOut     = false;
        float       lifetime    = 3.0f;     // seconds (when fadeOut)
        bool        destroyOnEnd = true;    // remove entity when fully faded

        float age_ = 0.0f;                  // runtime

        void start(Scene*) override { age_ = 0.0f; if (!texturePath.empty()) Load(); }

        std::string ResolvePath(const std::string& path) const
        {
            if (path.empty()) return "";
            if (std::filesystem::path(path).is_absolute()) return path;
            std::string proj = ScriptManager::Get().GetActiveProjectPath();
            if (!proj.empty()) return (std::filesystem::path(proj) / path).string();
            return path;
        }

        void Load()
        {
            if (texturePath.empty()) { tex_ = {0}; return; }
            tex_ = AssetManager::Get().GetTexture(ResolvePath(texturePath));
        }

        void update(float dt, Vector2, Scene* scene) override
        {
            if (!fadeOut) return;
            age_ += dt;
            if (age_ >= lifetime && destroyOnEnd && scene && owner)
                scene->DestroyEntity(owner->id);
        }

        void draw() const override
        {
            if (!owner || tex_.id == 0) return;

            float alpha = 1.0f;
            if (fadeOut && lifetime > 0.0f) alpha = 1.0f - (age_ / lifetime);
            if (alpha <= 0.0f) return;
            alpha = (alpha > 1.0f) ? 1.0f : alpha;

            Color c = tint;
            c.a = (unsigned char)(tint.a * alpha);

            Vector2 gPos = owner->getGlobalPosition();
            Vector2 gScl = owner->getGlobalScale();
            float   gRot = owner->getGlobalRotation();

            ::Rectangle src = { 0, 0, (float)tex_.width, (float)tex_.height };
            ::Rectangle dst = { gPos.x, gPos.y, gScl.x, gScl.y };
            Vector2 origin  = { gScl.x * 0.5f, gScl.y * 0.5f };

            if (additive) BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(tex_, src, dst, origin, gRot, c);
            if (additive) EndBlendMode();
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            char buf[512] = {};
            strncpy(buf, texturePath.c_str(), sizeof(buf) - 1);
            ImGui::Text("Texture Path");
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##DecalPath", buf, sizeof(buf))) { if (snapshotCb) snapshotCb(); texturePath = buf; }
            ImGui::PopItemWidth();
            // Drag-drop from the content browser
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    std::string dropped((const char*)payload->Data, payload->DataSize ? payload->DataSize - 1 : 0);
                    std::string ext = std::filesystem::path(dropped).extension().string();
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
                    { if (snapshotCb) snapshotCb(); texturePath = dropped; Load(); }
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::Button("Load##Decal")) Load();
            ImGui::SameLine();
            ImGui::TextDisabled(tex_.id ? "Loaded" : "No texture");

            ImGui::Spacing();
            ImGui::Text("Tint");
            float c[4] = { tint.r/255.f, tint.g/255.f, tint.b/255.f, tint.a/255.f };
            ImGui::PushItemWidth(-1);
            if (ImGui::ColorEdit4("##DecalTint", c))
            {
                tint.r=(unsigned char)(c[0]*255); tint.g=(unsigned char)(c[1]*255);
                tint.b=(unsigned char)(c[2]*255); tint.a=(unsigned char)(c[3]*255);
            }
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Checkbox("Additive", &additive);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::SameLine();
            ImGui::Checkbox("Fade Out", &fadeOut);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            if (fadeOut)
            {
                ImGui::Text("Lifetime (s)");
                ImGui::PushItemWidth(-1);
                ImGui::DragFloat("##DecalLife", &lifetime, 0.05f, 0.05f, 60.0f, "%.2f");
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
                ImGui::Checkbox("Destroy Entity When Faded", &destroyOnEnd);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            }
        }

        std::string getName() const override { return "Decal"; }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<DecalComponent>(*this);
            c->age_ = 0.0f;
            c->tex_ = {0};
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j   = Component::serialize();
            j["texturePath"]   = texturePath;
            j["tint"]          = { tint.r, tint.g, tint.b, tint.a };
            j["additive"]      = additive;
            j["fadeOut"]       = fadeOut;
            j["lifetime"]      = lifetime;
            j["destroyOnEnd"]  = destroyOnEnd;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("texturePath"))  texturePath  = j["texturePath"].get<std::string>();
            if (j.contains("tint"))
            {
                tint.r = j["tint"][0].get<unsigned char>(); tint.g = j["tint"][1].get<unsigned char>();
                tint.b = j["tint"][2].get<unsigned char>(); tint.a = j["tint"][3].get<unsigned char>();
            }
            if (j.contains("additive"))     additive     = j["additive"].get<bool>();
            if (j.contains("fadeOut"))      fadeOut      = j["fadeOut"].get<bool>();
            if (j.contains("lifetime"))     lifetime     = j["lifetime"].get<float>();
            if (j.contains("destroyOnEnd")) destroyOnEnd = j["destroyOnEnd"].get<bool>();
            if (!texturePath.empty()) Load();
        }

    private:
        Texture2D tex_ = {0};
    };
}
