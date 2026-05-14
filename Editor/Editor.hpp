/**********************************************************************************************
*
*   Editor - Development environment and scene orchestration
*
*   The Editor acts as the main interface between the user and the engine,
*   managing viewport rendering, scene manipulation, and state transitions
*   between editing and simulation.
*
*   Copyright (c) 2026
*
**********************************************************************************************/

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
#include "../Component/RigidbodyComponent.hpp"
#include "../Scene/Scene.hpp"
#include "../Entity/EntityFactory.hpp"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>

namespace Indium
{
    /**
     * @brief Defines the current operational mode of the engine.
     */
    enum class GameState { Editor, Play };

    /**
     * @brief The core Editor class responsible for the development environment.
     *
     * The Editor acts as the main orchestrator, bridging Raylib's rendering
     * capabilities with ImGui's user interface. It manages scene selection,
     * viewport scaling, entity manipulation, and the "Play/Stop" simulation state.
     */
    class Editor
    {
    private:
        /** @brief Engine configuration settings (window size, titles, etc.). */
        Config              config;

        /** @brief The current state (Editor for building, Play for simulating). */
        GameState           state = GameState::Editor;

        /** @brief The active game world container. */
        Scene               scene;

        /** @brief Index of the currently selected entity in the hierarchy/inspector. */
        int                 selectedIndex = -1;

        /** @brief Utility for creating standardized entity types. */
        EntityFactory       factory;

        /**
         * @brief An off-screen buffer used to render the game world.
         *
         * This allows the game to be rendered into an ImGui window (the Viewport)
         * instead of filling the entire hardware window.
         */
        RenderTexture2D     viewport;

        /** @brief Offset between the mouse cursor and the entity's origin during a drag operation. */
        Vector2             dragOffset = { 0, 0 };

        /** @brief Pointer to the entity currently being dragged by the mouse. */
        Indium::Entity*     draggingEntity = nullptr;

        /** @brief The screen-space position of the ImGui Viewport window. */
        ImVec2              viewportPos = { 0, 0 };

        /** @brief The current dimensions of the ImGui Viewport window. */
        ImVec2              viewportSize = { 0, 0 };

        /** @brief Whether the mouse cursor is currently within the Viewport bounds. */
        bool                viewportHovered = false;


        /**
         * @brief Converts 0-255 RGB values to 0.0-1.0 ImVec4 format.
         *
         * Helper method for consistent color definition in the UI theme.
         */
        ImVec4 RGBA(int r, int g, int b, float a = 1.0f)
        {
            return ImVec4(
                r / 255.0f,
                g / 255.0f,
                b / 255.0f,
                a
            );
        }

    public:
        Editor() = default;

        /** @brief Initializes the engine, graphics context, and editor theme. */
        void Init();

        /** @brief Performs cleanup of graphics resources (RenderTextures, etc.). */
        void Shutdown();

        /** @brief Handles user input and scene logic updates. */
        void Update(float dt);

        /** @brief Executes the main rendering pass (UI + Viewport). */
        void Run();

    private:
        /** @brief Configures the modern, dark-themed visual style for ImGui. */
        void ApplyModernTheme();

        /** @brief Renders the top menu bar (File, Create, Play/Stop). */
        void ShowMainMenuBar();

        /** @brief Renders the side panel listing all entities in the scene. */
        void ShowHierarchy();

        /** @brief Renders the game world into an ImGui window. */
        void ShowViewport();

        /** @brief Renders the property editor for the selected entity. */
        void ShowInspector();

        /** @brief Removes an entity from the scene and resets the selection. */
        void DeleteEntity(Entity& entity);
    };

    /*
     * --- IMPLEMENTATION ---
     * The following methods provide the core logic for the Indium Editor. 
     * They are defined inline to maintain the single-header distribution pattern 
     * while ensuring all dependency types are fully visible to the compiler.
     */

    /** 
     * @brief Prepares the editor's graphical and interface resources.
     */
    inline void Editor::Init()
    {
        // We start with a minimal 1x1 render target. The Run() method will 
        // dynamically resize this to match the actual ImGui panel dimensions.
        viewport = LoadRenderTexture(1, 1);
        ApplyModernTheme();
    }

    /** 
     * @brief Releases hardware resources held by the editor.
     */
    inline void Editor::Shutdown()
    {
        UnloadRenderTexture(viewport);
    }

    /** 
     * @brief Core input and state update logic.
     * 
     * This method handles the translation of raw OS input into engine-specific actions.
     */
    inline void Editor::Update(float dt)
    {
        Vector2 screenMouse = GetMousePosition();

        /**
         * @brief Viewport Coordinate Transformation
         *
         * Because the simulation is rendered to a texture that is then displayed 
         * inside a scaled ImGui window, we must map screen-space mouse coordinates 
         * back into the texture's local coordinate system (world-space).
         */
        float scaleX = (viewportSize.x > 0) ? (float)viewport.texture.width  / viewportSize.x : 1.0f;
        float scaleY = (viewportSize.y > 0) ? (float)viewport.texture.height / viewportSize.y : 1.0f;

        Vector2 mouse = {
            (screenMouse.x - viewportPos.x) * scaleX,
            (screenMouse.y - viewportPos.y) * scaleY
        };

        // Update the simulation only if the state is set to 'Play'
        if (state == GameState::Play) scene.Update(dt);

        /** @brief Handle Entity Selection and Drag Initiation */
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && viewportHovered)
        {
            draggingEntity = nullptr;
            
            // Search for the entity under the cursor. We iterate backwards to 
            // respect the visual draw order (picking the top-most entity first).
            for (int i = (int)scene.entities.size() - 1; i >= 0; i--)
            {
                if (scene.entities[i]->Contains(mouse))
                {
                    draggingEntity  = scene.entities[i].get();
                    dragOffset      = Vector2{ draggingEntity->position.x - mouse.x, draggingEntity->position.y - mouse.y };
                    selectedIndex   = i;
                    break;
                }
            }
        }

        /** @brief Process Active Dragging with Boundary Awareness */
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggingEntity != nullptr)
        {
            float worldW    = scene.worldSize.x;
            float worldH    = scene.worldSize.y;
            float targetX   = mouse.x + dragOffset.x;
            float targetY   = mouse.y + dragOffset.y;

            // Preview the next frame's position to calculate visual bounds
            Vector2 oldPos = draggingEntity->position;
            draggingEntity->position = Vector2{ targetX, targetY };
            ::Rectangle bounds = draggingEntity->getBounds();
            draggingEntity->position = oldPos;

            // Clamp the entity's position so its visual bounds remain within the world
            if (bounds.x < 0)                       targetX -= bounds.x;
            if (bounds.x + bounds.width > worldW)   targetX -= (bounds.x + bounds.width - worldW);
            if (bounds.y < 0)                       targetY -= bounds.y;
            if (bounds.y + bounds.height > worldH)  targetY -= (bounds.y + bounds.height - worldH);

            draggingEntity->position = Vector2{ targetX, targetY };
        }

        // Release the entity once the mouse button is let go
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) draggingEntity = nullptr;
    }

    /** 
     * @brief Orchestrates the multi-pass rendering process.
     */
    inline void Editor::Run()
    {
        /**
         * @brief Adaptive Resource Management
         *
         * If the user resizes the ImGui Viewport panel, we recreate the 
         * RenderTexture to maintain a 1:1 pixel ratio, ensuring sharpness.
         */
        if (viewportSize.x > 0 && viewportSize.y > 0 &&
           (viewportSize.x != (float)viewport.texture.width || viewportSize.y != (float)viewport.texture.height)) {
            UnloadRenderTexture(viewport);
            viewport = LoadRenderTexture((int)viewportSize.x, (int)viewportSize.y);
        }

        // Sync world boundaries with the current render target size
        scene.worldSize = Vector2{ (float)viewport.texture.width, (float)viewport.texture.height };

        /** @brief Pass 1: World Rendering (Off-screen) */
        BeginTextureMode(viewport);
            ClearBackground(Color{ 20, 20, 20, 255 });
            scene.Draw();
            // Draw a debug boundary to visualize the active simulation area
            DrawRectangleLines(0, 0, viewport.texture.width, viewport.texture.height, RED);
        EndTextureMode();

        /** @brief Pass 2: Interface Rendering (On-screen) */
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

    /** 
     * @brief Applies a professional, dark aesthetic to the ImGui environment.
     */
    inline void Editor::ApplyModernTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        // Structural adjustments for a modern feel
        style.WindowRounding    = 0.0f;
        style.FrameRounding     = 3.0f;
        style.PopupRounding     = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.GrabRounding      = 3.0f;
        style.TabRounding       = 3.0f;
        style.WindowPadding     = ImVec2(10, 10);
        style.FramePadding      = ImVec2(10, 6);
        style.WindowBorderSize  = 1.0f;
        style.FrameBorderSize   = 1.0f;
        style.ItemSpacing       = ImVec2(8, 8);
        style.ScrollbarSize     = 10.0f;

        // Dark color palette with subtle highlights
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_MenuBarBg]              = RGBA(10,10,10,0.8);
        colors[ImGuiCol_WindowBg]               = RGBA(18,18,18,1);
        colors[ImGuiCol_ChildBg]                = RGBA(18,18,18,1);
        colors[ImGuiCol_PopupBg]                = RGBA(18,18,18,1);
        colors[ImGuiCol_Border]                 = RGBA(40,40,40,0.5f);

        colors[ImGuiCol_Header]                 = RGBA(18,18,18,1);
        colors[ImGuiCol_HeaderHovered]          = RGBA(30,30,30,1);
        colors[ImGuiCol_HeaderActive]           = RGBA(20,20,20,1);

        colors[ImGuiCol_Button]                 = RGBA(33,33,33,1);
        colors[ImGuiCol_ButtonHovered]          = RGBA(40,40,40,1);
        colors[ImGuiCol_ButtonActive]           = RGBA(33,33,33,1);

        colors[ImGuiCol_FrameBg]                = RGBA(33,33,33,1);
        colors[ImGuiCol_FrameBgHovered]         = RGBA(40,40,40,1);
        colors[ImGuiCol_FrameBgActive]          = RGBA(33,33,33,1);

        colors[ImGuiCol_Tab]                    = RGBA(33,33,33,1);
        colors[ImGuiCol_TabHovered]             = RGBA(40,40,40,1);
        colors[ImGuiCol_TabActive]              = RGBA(33,33,33,1);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.25f, 0.28f, 0.32f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.35f, 0.38f, 0.42f, 1.00f);

        colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.52f, 0.55f, 1.00f);
    }

    /** 
     * @brief Renders the application-level command bar.
     */
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

            // Playback controls: Center-aligned in the menu bar
            float barWidth = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(barWidth / 2.0f);
            if(ImGui::Button(state == GameState::Editor ? "Play" : "Stop", ImVec2(50, 0)))
            {
                if(state == GameState::Editor)
                {
                    scene.Save();
                    state = GameState::Play;
                }
                else
                {
                    scene.Restore();
                    state = GameState::Editor;
                }
            }
            ImGui::EndMainMenuBar();
        }
    }

    /** 
     * @brief Renders the scene tree and object creation shortcuts.
     */
    inline void Editor::ShowHierarchy()
    {
        float menuBarH = ImGui::GetFrameHeight();
        float screenW  = (float)GetScreenWidth();
        float screenH  = (float)GetScreenHeight();
        float panelW   = 250.0f;

        ImGui::SetNextWindowPos(ImVec2(0, menuBarH));
        ImGui::SetNextWindowSize(ImVec2(panelW, screenH - menuBarH));
        ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        
        // Fast creation buttons
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

        // List and select entities in the world
        for (int i = 0; i < (int)scene.entities.size(); i++)
        {
            char label[128];
            snprintf(label, sizeof(label), "%s##%d", scene.entities[i]->name.c_str(), i);
            if (ImGui::Selectable(label, selectedIndex == i)) selectedIndex = i;
        }
        ImGui::End();
    }

    /** 
     * @brief Renders the interactive game world simulation window.
     */
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
        
        // Track the viewport's screen position for accurate mouse ray-casting
        viewportPos.x   = ImGui::GetCursorScreenPos().x;
        viewportPos.y   = ImGui::GetCursorScreenPos().y;
        viewportSize.x  = ImGui::GetContentRegionAvail().x;
        viewportSize.y  = ImGui::GetContentRegionAvail().y;
        viewportHovered = ImGui::IsWindowHovered();

        rlImGuiImageRenderTextureFit(&viewport, false);
        
        ImGui::End();
        ImGui::PopStyleVar();
    }

    /** 
     * @brief Renders the property editor for the currently selected object.
     */
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
            // Recursively draw properties for the entity and all its components
            scene.entities[selectedIndex]->inspect();

            ImGui::Separator();
            
            // Component Addition Popup
            if(ImGui::Button("Add Component", ImVec2(-1, 0))) ImGui::OpenPopup("Component Popup");

            if(ImGui::BeginPopup("Component Popup"))
            {
                if(ImGui::MenuItem("Add Bouncer"))      scene.entities[selectedIndex]->addComponent<BouncerComponent>();
                if(ImGui::MenuItem("Add Rigidbody"))    scene.entities[selectedIndex]->addComponent<RigidbodyComponent>();

                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    /** 
     * @brief Safely deletes an entity and cleans up the selection state.
     */
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
}
