#pragma once

#include "raylib.h"
#include "imgui.h"
#include "../include/rlImGui.h"
#include "../src/Config.hpp"
#include "../Entity/Entity.hpp"
#include "../Entity/Circle.hpp"
#include "../Entity/Rectangle.hpp"
#include "../Entity/Plane.hpp"
#include "../Component/Component.hpp"
#include "../Component/BouncerComponent.hpp"
#include "../Scene/Scene.hpp"
#include "../Entity/EntityFactory.hpp"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>

namespace Indium
{
    enum class GameState { Editor, Play };

    class Editor
    {
    private:
        Config              config;
        GameState           state = GameState::Editor;
        Scene               scene;
        int                 selectedIndex = -1;
        EntityFactory       factory;
        RenderTexture2D     viewport;

        Vector2             dragOffset = { 0, 0 };
        Indium::Entity*     draggingEntity = nullptr;

        ImVec2              viewportPos = { 0, 0 };
        ImVec2              viewportSize = { 0, 0 };
        bool                viewportHovered = false;

    public:
        Editor() = default;

        void Init();
        void Shutdown();
        void Update(float dt);
        void Run();

    private:
        void ApplyModernTheme();
        void ShowMainMenuBar();
        void ShowHierarchy();
        void ShowViewport();
        void ShowInspector();
        void DeleteEntity(Entity& entity);
    };

    // --- IMPLEMENTATION ---
    // We put implementations here (at the end of the header) to ensure
    // all Entity/Component types are FULLY defined before we use them.

    inline void Editor::Init()
    {
        // Start with a 1x1 RenderTexture; Run() will resize to match the actual viewport panel.
        // Using GetScreenWidth/Height here is WRONG because ImGui layout hasn't been calculated yet.
        viewport = LoadRenderTexture(1, 1);
        ApplyModernTheme();
    }

    inline void Editor::Shutdown() {

        UnloadRenderTexture(viewport);
    }

    inline void Editor::Update(float dt)
    {
        Vector2 screenMouse = GetMousePosition();

        // Scale mouse coordinates: ImGui panel size may differ from RenderTexture size
        // (e.g., first frame, during resize). This maps panel-space to world-space.
        float scaleX = (viewportSize.x > 0) ? (float)viewport.texture.width  / viewportSize.x : 1.0f;
        float scaleY = (viewportSize.y > 0) ? (float)viewport.texture.height / viewportSize.y : 1.0f;

        Vector2 mouse = {
            (screenMouse.x - viewportPos.x) * scaleX,
            (screenMouse.y - viewportPos.y) * scaleY
        };

        if (state == GameState::Play) scene.Update(dt);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && viewportHovered)
        {
            draggingEntity = nullptr;
            for (int i = (int)scene.entities.size() - 1; i >= 0; i--)
            {
                if (scene.entities[i]->Contains(mouse))
                {
                    draggingEntity = scene.entities[i].get();
                    dragOffset = Vector2{ draggingEntity->position.x - mouse.x, draggingEntity->position.y - mouse.y };
                    selectedIndex = i;
                    break;
                }
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggingEntity != nullptr)
        {
            float worldW = scene.worldSize.x;
            float worldH = scene.worldSize.y;
            float targetX = mouse.x + dragOffset.x;
            float targetY = mouse.y + dragOffset.y;

            // Temporarily set position to calculate bounds
            Vector2 oldPos = draggingEntity->position;
            draggingEntity->position = Vector2{ targetX, targetY };
            ::Rectangle bounds = draggingEntity->getBounds();
            draggingEntity->position = oldPos;

            // Clamp using actual visual bounds, not just position
            if (bounds.x < 0)
                targetX -= bounds.x;
            if (bounds.x + bounds.width > worldW)
                targetX -= (bounds.x + bounds.width - worldW);
            if (bounds.y < 0)
                targetY -= bounds.y;
            if (bounds.y + bounds.height > worldH)
                targetY -= (bounds.y + bounds.height - worldH);

            draggingEntity->position = Vector2{ targetX, targetY };
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) draggingEntity = nullptr;
    }

    inline void Editor::Run()
    {
        // Resize RenderTexture to match the ImGui viewport panel
        if (viewportSize.x > 0 && viewportSize.y > 0 &&
           (viewportSize.x != (float)viewport.texture.width || viewportSize.y != (float)viewport.texture.height)) {
            UnloadRenderTexture(viewport);
            viewport = LoadRenderTexture((int)viewportSize.x, (int)viewportSize.y);
        }

        // Always sync worldSize with actual RenderTexture dimensions.
        // Previously this was inside the resize if-block, so it was stale on first frame.
        scene.worldSize = Vector2{ (float)viewport.texture.width, (float)viewport.texture.height };

        BeginTextureMode(viewport);
            ClearBackground(Color{ 20, 20, 20, 255 });
            scene.Draw();
            DrawRectangleLines(0, 0, viewport.texture.width, viewport.texture.height, RED);
        EndTextureMode();

        BeginDrawing();
            ClearBackground(DARKGRAY);
            rlImGuiBegin();
            ShowMainMenuBar();
            ShowHierarchy();
            ShowViewport();
            ShowInspector();
            rlImGuiEnd();
        EndDrawing();
    }

    inline void Editor::ApplyModernTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    }

    inline void Editor::ShowMainMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Exit")) CloseWindow();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem("Circle"))
                {
                    std::unique_ptr<Entity> e = factory.CreateCircle(scene);
                    scene.entities.push_back(std::move(e));
                }
                if (ImGui::MenuItem("Rectangle"))
                {
                    std::unique_ptr<Entity> e = factory.CreateRectangle(scene);
                    scene.entities.push_back(std::move(e));
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    inline void Editor::ShowHierarchy()
    {
        float menuBarH = ImGui::GetFrameHeight();
        float screenW = (float)GetScreenWidth();
        float screenH = (float)GetScreenHeight();
        float panelW = 250.0f;

        ImGui::SetNextWindowPos(ImVec2(0, menuBarH));
        ImGui::SetNextWindowSize(ImVec2(panelW, screenH - menuBarH));
        ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        if (ImGui::Button("Add Circle", ImVec2(-1, 0)))
        {
            std::unique_ptr<Entity> e = factory.CreateCircle(scene);
            scene.entities.push_back(std::move(e));
        }
        if (ImGui::Button("Add Rectangle", ImVec2(-1, 0)))
        {
            std::unique_ptr<Entity> e = factory.CreateRectangle(scene);
            scene.entities.push_back(std::move(e));
        }
        if (ImGui::Button("Add Plane", ImVec2(-1, 0)))
        {
            std::unique_ptr<Entity> e = factory.CreatePlane(scene);
            scene.entities.push_back(std::move(e));
        }
        ImGui::Separator();
        for (int i = 0; i < (int)scene.entities.size(); i++)
        {
            char label[128];
            snprintf(label, sizeof(label), "%s##%d", scene.entities[i]->name.c_str(), i);
            if (ImGui::Selectable(label, selectedIndex == i)) selectedIndex = i;
        }
        ImGui::End();
    }

    inline void Editor::ShowViewport()
    {
        float menuBarH  = ImGui::GetFrameHeight();
        float screenW   = (float)GetScreenWidth();
        float screenH   = (float)GetScreenHeight();
        float sideW     = 250.0f;
        float vpX       = sideW;
        float vpW       = screenW - (sideW * 2.0f);

        ImGui::SetNextWindowPos(ImVec2(vpX, menuBarH));
        ImGui::SetNextWindowSize(ImVec2(vpW, screenH - menuBarH));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        viewportPos.x   = ImGui::GetCursorScreenPos().x;
        viewportPos.y   = ImGui::GetCursorScreenPos().y;
        viewportSize.x  = ImGui::GetContentRegionAvail().x;
        viewportSize.y  = ImGui::GetContentRegionAvail().y;
        viewportHovered = ImGui::IsWindowHovered();

        // Use Fit variant: prevents overflow when RenderTexture size != panel size
        rlImGuiImageRenderTextureFit(&viewport, false);
        ImGui::End();
        ImGui::PopStyleVar();
    }

    inline void Editor::ShowInspector()
    {
        float menuBarH = ImGui::GetFrameHeight();
        float screenW = (float)GetScreenWidth();
        float screenH = (float)GetScreenHeight();
        float panelW = 250.0f;

        ImGui::SetNextWindowPos(ImVec2(screenW - panelW, menuBarH));
        ImGui::SetNextWindowSize(ImVec2(panelW, screenH - menuBarH));
        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        if (selectedIndex != -1 && selectedIndex < (int)scene.entities.size())
        {
            scene.entities[selectedIndex]->inspect();
            if (ImGui::Button("Add Bouncer",(ImVec2){-1,0}))
            {
                scene.entities[selectedIndex]->addComponent<BouncerComponent>();
            }
        }
        ImGui::End();
    }

    inline void Editor::DeleteEntity(Entity& entity)
    {
        auto it = std::find_if(scene.entities.begin(), scene.entities.end(),
        [&](const std::unique_ptr<Entity>& e) { return e.get() == &entity; });
        if (it != scene.entities.end())
        {
            scene.entities.erase(it);
            selectedIndex = -1;
        }
    }
}
