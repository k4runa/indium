#pragma once
#include "../../core/Entity.hpp"
#include "../../core/AssetManager.hpp"
#if __has_include("../../tools/FileBrowser.hpp")
    #include "../../tools/FileBrowser.hpp"
    #define INDIUM_HAS_FILE_BROWSER
#endif
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"
#include <string>
#include <filesystem>

// Fallback when compiled outside the editor (e.g. pulled into a script DLL via
// IndiumEngine.hpp) where the FontAwesome icon header isn't included.
#ifndef ICON_FA_CIRCLE_INFO
    #define ICON_FA_CIRCLE_INFO ""
#endif

namespace Indium
{
    // --------------------------------------------------------------------
    // MeshRendererComponent — draws a 3D model into the 2.5D scene.
    //
    // Strategy: the model is rendered with a private orthographic Camera3D
    // into an off-screen RenderTexture, then that texture is blitted exactly
    // like SpriteRendererComponent does. By the time Scene::Draw's painter
    // sort sees this component it is just a sprite, so it inherits depthLayer/
    // sortingOrder ordering, parallax batching, 2D lighting, and post-process
    // for free — no changes to the core 2D render loop.
    //
    // The off-screen render (BeginTextureMode + BeginMode3D) cannot be nested
    // inside the scene's BeginMode2D / the viewport's BeginTextureMode, so it
    // is driven from RenderIfNeeded(): called from update() (Play / standalone
    // player, where scene.Update runs outside drawing) and from a dedicated
    // editor pre-pass (Editor.cpp, before BeginTextureMode(viewport)) so the
    // preview stays live in Edit mode too. A dirty flag keeps a static prop at
    // sprite cost — the RT is only re-rendered when model/rotation/scale/tint/
    // resolution change.
    // --------------------------------------------------------------------
    struct MeshRendererComponent : Component
    {
        // --- Serialized ---
        std::string modelPath;
        Color       tint          = WHITE;            // modulates the 3D material
        Vector3     eulerRotation = { 0.0f, 0.0f, 0.0f }; // degrees — the 2.5D tilt
        float       modelScale    = 1.0f;
        int         rtResolution  = 256;              // off-screen target size (px)

        // --- Runtime (never serialized; reset on clone) ---
        Model          model{};
        bool           modelLoaded = false;
        RenderTexture2D target_{};
        Camera3D        cam3d_{};
        bool           dirty_   = true;
        bool           rtValid_ = false;

        bool Load(const std::string& path)
        {
            modelLoaded = false;
            modelPath   = path;
            model       = AssetManager::Get().GetModel(path);
            if (model.meshCount > 0)
            {
                modelLoaded = true;
                dirty_      = true;
                // Give the on-screen quad a sensible default footprint when the mesh is
                // first assigned (deserialize restores the authored scale afterwards).
                if (owner) owner->scale = { (float)rtResolution, (float)rtResolution };
                return true;
            }
            return false;
        }

        // Idempotent: ensures the off-screen target exists and, when dirty, re-renders
        // the model into it. Safe to call from both update() and the editor pre-pass.
        void RenderIfNeeded()
        {
            if (!modelLoaded)
            {
                if (modelPath.empty()) return;
                if (!Load(modelPath)) return;   // GetModel throttles missing-file retries
            }
            EnsureTarget_();
            if (rtValid_ && dirty_)
            {
                RenderToTarget_();
                dirty_ = false;
            }
        }

        void update(float, Vector2, Scene*) override { RenderIfNeeded(); }

        void draw() const override
        {
            if (!owner || !owner->activeInHierarchy()) return;

            Vector2 gPos = owner->getGlobalPosition();
            Vector2 gScl = owner->getGlobalScale();
            float   gRot = owner->getGlobalRotation();

            if (!modelLoaded || !rtValid_)
            {
                float drawW = fmaxf(gScl.x, 100.0f);
                float drawH = fmaxf(gScl.y, 100.0f);
                DrawRectanglePro({ gPos.x, gPos.y, drawW, drawH }, { drawW * 0.5f, drawH * 0.5f }, gRot, ColorAlpha(RED, 0.3f));
                DrawText("No Model", (int)gPos.x - 28, (int)gPos.y - 5, 10, WHITE);
                return;
            }

            // RenderTextures are stored bottom-up, so the source rect's height is negated
            // to flip the blit upright (raylib's standard RT-to-screen idiom).
            //
            // Blit with WHITE, NOT owner->color: the RT already holds the fully shaded
            // model with the component's `tint` baked in, and its alpha is the model's
            // silhouette. Modulating by the entity color would hide the mesh on entities
            // whose color is transparent by design — e.g. an Empty is created with
            // color {0,0,0,0}, which made the mesh render invisibly. Use `tint` to recolor.
            ::Rectangle src    = { 0.0f, 0.0f, (float)target_.texture.width, -(float)target_.texture.height };
            ::Rectangle dst    = { gPos.x, gPos.y, gScl.x, gScl.y };
            Vector2     origin = { gScl.x * 0.5f, gScl.y * 0.5f };
            DrawTexturePro(target_.texture, src, dst, origin, gRot, WHITE);
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            if (modelLoaded && rtValid_)
            {
                float previewH = 96.0f;
                // RT is bottom-up — flip V in the preview (uv0 = {0,1}, uv1 = {1,0}).
                ImGui::Image((ImTextureID)(uintptr_t)target_.texture.id, ImVec2(previewH, previewH), ImVec2(0, 1), ImVec2(1, 0));
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "%s", DisplayName_().c_str());
                ImGui::TextDisabled("%d mesh%s", model.meshCount, model.meshCount == 1 ? "" : "es");
                ImGui::EndGroup();
                ImGui::Spacing();
                if (ImGui::Button("Change Model...", ImVec2(-1, 0))) ImGui::OpenPopup("MeshModel Browser");
            }
            else
            {
                ImGui::TextDisabled("(no model)");
                ImGui::Spacing();
                if (ImGui::Button("Select Model...", ImVec2(-1, 0))) ImGui::OpenPopup("MeshModel Browser");
            }

            std::string selectedPath;
#ifdef INDIUM_HAS_FILE_BROWSER
            if (FileBrowser::Draw("MeshModel Browser", selectedPath, { ".glb", ".gltf", ".obj" }))
                Load(selectedPath);
#endif

            // Built-in primitives — one-click defaults that need no asset on disk.
            ImGui::Spacing();
            ImGui::TextDisabled("Primitives");
            float third = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;
            if (ImGui::Button("Cube", ImVec2(third, 0)))   { if (snapshotCb) snapshotCb(); Load("@cube"); }
            ImGui::SameLine();
            if (ImGui::Button("Sphere", ImVec2(third, 0))) { if (snapshotCb) snapshotCb(); Load("@sphere"); }

            ImGui::Spacing();
            ImGui::Text("Rotation (deg)");
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat3("##MeshEuler", &eulerRotation.x, 1.0f)) dirty_ = true;
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            if (ImGui::DragFloat("Model Scale", &modelScale, 0.01f, 0.001f, 1000.0f)) dirty_ = true;
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            float col[4] = { tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f, tint.a / 255.0f };
            if (ImGui::ColorEdit4("Tint", col))
            {
                tint = { (unsigned char)(col[0] * 255), (unsigned char)(col[1] * 255),
                         (unsigned char)(col[2] * 255), (unsigned char)(col[3] * 255) };
                dirty_ = true;
            }
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            if (ImGui::DragInt("RT Resolution", &rtResolution, 8.0f, 32, 2048)) dirty_ = true;
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
        }

        std::string getName() const override { return "MeshRenderer"; }

        std::unique_ptr<Component> clone() const override
        {
            // Copy only the authored fields — a clone must NOT share this instance's
            // RenderTexture / Model handles (those are GPU-owned and freed per-instance).
            auto c = std::make_unique<MeshRendererComponent>();
            c->enabled       = enabled;
            c->modelPath     = modelPath;
            c->tint          = tint;
            c->eulerRotation = eulerRotation;
            c->modelScale    = modelScale;
            c->rtResolution  = rtResolution;
            // Runtime state stays default: modelLoaded=false, rtValid_=false, dirty_=true.
            // The model + target are (re)built lazily on the clone's first RenderIfNeeded().
            return c;
        }

        void destroy(Scene* = nullptr) override { FreeTarget_(); }

        ~MeshRendererComponent() override { FreeTarget_(); }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["modelPath"]     = modelPath;
            j["tint"]          = { tint.r, tint.g, tint.b, tint.a };
            j["eulerRotation"] = { eulerRotation.x, eulerRotation.y, eulerRotation.z };
            j["modelScale"]    = modelScale;
            j["rtResolution"]  = rtResolution;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("tint") && j["tint"].is_array() && j["tint"].size() >= 4)
                tint = { j["tint"][0], j["tint"][1], j["tint"][2], j["tint"][3] };
            if (j.contains("eulerRotation") && j["eulerRotation"].is_array() && j["eulerRotation"].size() >= 3)
                eulerRotation = { j["eulerRotation"][0], j["eulerRotation"][1], j["eulerRotation"][2] };
            if (j.contains("modelScale"))   modelScale   = j["modelScale"].get<float>();
            if (j.contains("rtResolution")) rtResolution = j["rtResolution"].get<int>();
            if (j.contains("modelPath") && !j["modelPath"].get<std::string>().empty())
            {
                // Load() auto-sizes owner->scale to the RT footprint — right on first
                // assign, wrong here: Entity::deserialize already restored the authored
                // scale. Preserve it across the call (same guard as SpriteRenderer).
                Vector2 authoredScale = owner ? owner->scale : Vector2{ 1, 1 };
                Load(j["modelPath"].get<std::string>());
                if (owner) owner->scale = authoredScale;
            }
            dirty_ = true;
        }

    private:
        std::string DisplayName_() const
        {
            if (modelPath == "@cube")   return "Cube (built-in)";
            if (modelPath == "@sphere") return "Sphere (built-in)";
            return std::filesystem::path(modelPath).filename().string();
        }

        void EnsureTarget_()
        {
            if (rtValid_ && target_.texture.width == rtResolution && target_.texture.height == rtResolution) return;
            FreeTarget_();
            target_  = LoadRenderTexture(rtResolution, rtResolution);
            rtValid_ = (target_.texture.id > 0);
            dirty_   = true;
        }

        void FreeTarget_()
        {
            if (rtValid_ && IsWindowReady()) UnloadRenderTexture(target_);
            rtValid_ = false;
            target_  = {};
        }

        void RenderToTarget_()
        {
            // Frame the mesh by its bounding sphere so any rotation stays in view.
            BoundingBox bb = GetModelBoundingBox(model);
            Vector3 center = { (bb.min.x + bb.max.x) * 0.5f,
                               (bb.min.y + bb.max.y) * 0.5f,
                               (bb.min.z + bb.max.z) * 0.5f };
            float radius = Vector3Length(Vector3Subtract(bb.max, bb.min)) * 0.5f;
            if (radius < 0.0001f) radius = 1.0f;
            float scaledRadius = radius * modelScale;

            // Rotate + scale about the bbox center, leaving it framed regardless of pivot.
            Matrix m = MatrixTranslate(-center.x, -center.y, -center.z);
            m = MatrixMultiply(m, MatrixScale(modelScale, modelScale, modelScale));
            m = MatrixMultiply(m, MatrixRotateXYZ({ eulerRotation.x * DEG2RAD,
                                                    eulerRotation.y * DEG2RAD,
                                                    eulerRotation.z * DEG2RAD }));
            m = MatrixMultiply(m, MatrixTranslate(center.x, center.y, center.z));
            model.transform = m;

            cam3d_.position   = { center.x, center.y, center.z + scaledRadius * 4.0f + 1.0f };
            cam3d_.target     = center;
            cam3d_.up         = { 0.0f, 1.0f, 0.0f };
            cam3d_.fovy       = scaledRadius * 2.2f;   // orthographic vertical span (world units)
            cam3d_.projection = CAMERA_ORTHOGRAPHIC;

            // Shade with the shared directional-diffuse shader so primitives (especially
            // the sphere) read as 3D. Falls back to flat/unlit if it failed to compile.
            if (const Shader* lit = AssetManager::Get().GetLitShader())
                for (int i = 0; i < model.materialCount; i++) model.materials[i].shader = *lit;

            BeginTextureMode(target_);
                ClearBackground(BLANK);
                BeginMode3D(cam3d_);
                    DrawModel(model, Vector3Zero(), 1.0f, tint);
                EndMode3D();
            EndTextureMode();
        }
    };

} // namespace Indium
