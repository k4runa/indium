/**********************************************************************************************
*
*   Indium v0.1 - A modular and lightweight 2D Game Engine built on Raylib
*
*   CORE MODULE: Editor
*       - High-level engine orchestration
*       - Integrated ImGui Viewport with coordinate mapping
*       - Modern dark-themed Inspector and Hierarchy
*       - Scene Snapshot (Play/Stop) state management
*
*   LICENSE: MIT
*
*   Copyright (c) 2026 Indium Engine Team
*
**********************************************************************************************/

#pragma once

#include "raylib.h"
#include "imgui.h"
#include "../include/rlImGui.h"
#include "../src/Config.hpp"
#include "../core/Entity.hpp"
#include "../2D/entity/Circle.hpp"
#include "../2D/entity/Rectangle.hpp"
#include "../2D/entity/Plane.hpp"
#include "../core/Component.hpp"
#include "../2D/component/BouncerComponent.hpp"
#include "../2D/component/RigidbodyComponent.hpp"
#include "../core/scene/Scene.hpp"
#include "../2D/entity/EntityFactory.hpp"
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
     * The Editor acts as the main orchestrator, bridging Raylib's renderi ng
     * capabilities with ImGui's user interface. It manages scene selecti on,
     * viewport scaling, entity manipulation, and the "Play/Stop" simulation state.
     */
    class Editor
    {
    private:
        /** @brief Default theme style, dark / light */
        std::string THEME_STYLE = "dark";

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
         * @brief An off-screen buffer used to render the game w orld.
          *
         * This allows the game to be rendered into an ImGui window (the Vi ewport)
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

        /** @brief Cached world-space mouse position for context menus. */
        Vector2             worldMouse = { 0, 0 };

        /** @brief Entity index for the currently open Viewport context menu. */
        int                 contextEntityIndex = -1;

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
        /** @brief Configures the modern, hemed visual style for ImGui. */
        void ApplyTheme(std::string THEME_STYLE);
        void ApplyDarkTheme(ImVec4* colors);
        void ApplyLightTheme(ImVec4* colors);

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
     * Implementations are provided inline at the end of the heade r to ensure
     * all Entity and Component types are fully defined, preventing "incomplete type" errors.
     */

    inline void Editor::Init()
    {
        // Initialize with a dummy size; Run() will dynamically resize to fit the UI layout.
        viewport = LoadRenderTexture(1, 1);
        ApplyTheme(THEME_STYLE);
    }

    inline void Editor ::Shutdown()
    {
        UnloadRenderTexture(viewport);
    }

    inline void Editor::Update(float dt)
    {
        Vector2 screenMouse = GetMousePosition();

        /**
         * @brief Coordinate Mapping Lo gic
         *
         * Since the game world is rendered into a RenderTexture that  is then scaled
         * to fit an ImGui panel, we must map screen-space mouse  coordinates back
         * to world-space coordinates for accurate clicking and dragging.
         */
        float scaleX = (viewportSize.x > 0) ? (float)viewport.texture.width  / viewportSize.x : 1.0f;
        float scaleY = (viewportSize.y > 0) ? (float)viewport.texture.height / viewportSize.y : 1.0f;

        worldMouse = {
            (screenMouse.x - viewportPos.x) * scaleX,
            (screenMouse.y - viewportPos.y) * scaleY
        };

        if (state == GameState::Play) scene.Update(dt);

        /** @brief Context Menu State Capture */
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && viewportHovered)
        {
            contextEntityIndex = -1;
            for (int i = (int)scene.entities.size() - 1; i >= 0; i--)
            {
                if (scene.entities[i]->Contains(worldMouse))
                {
                    contextEntityIndex = i;
                    selectedIndex = i; // Optionally select it too
                    break;
                }
            }
        }

        /** @brief Selection and Drag Initiation */
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && viewportHovered)
        {
            draggingEntity = nullptr;
            // Iterate backwards (top-to-bottom) so we pick the entity rendered on top
            for (int i = (int)scene.entities.size() - 1; i >= 0; i--)
            {
                if (scene.entities[i]->Contains(worldMouse))
                {
                    if (state == GameState::Editor)
                    {
                        draggingEntity  = scene.entities[i].get();
                        dragOffset      = Vector2{ draggingEntity->position.x - worldMouse.x, draggingEntity->position.y - worldMouse.y };
                    }
                    selectedIndex   = i;
                    break;
                }
            }
        }

        /** @brief Active Dragging Logic with Boundary Clamping */
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggingEntity != nullptr)
        {
            float worldW    = scene.worldSize.x;
            float worldH    = scene.worldSize.y;
            float targetX   = worldMouse.x + dragOffset.x;
            float targetY   = worldMouse.y + dragOffset.y;

            // Preview the new position to calculate visual bounds
            Vector2 oldPos = draggingEntity->position;
            draggingEntity->position = Vector2{ targetX, targetY };
            ::Rectangle bounds = draggingEntity->getBounds();
            draggingEntity->position = oldPos;

            // Prevent the entity from being dragged outside the simulation area
            if (bounds.x < 0)                       targetX -= bounds.x;
            if (bounds.x + bounds.width > worldW)   targetX -= (bounds.x + bounds.width - worldW);
            if (bounds.y < 0)                       targetY -= bounds.y;
            if (bounds.y + bounds.height > worldH)  targetY -= (bounds.y + bounds.height - worldH);

            draggingEntity->position = Vector2{ targetX, targetY };
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) draggingEntity = nullptr;
    }

    inline void Editor::Run()
     {
        /**
         * @brief Dynamic Viewport R esizing
         *
         * If the ImGui viewport panel has been resized by th e user, we recreate
         * the RenderTexture to match the new dimensions. T his ensures that the
         * internal resolution always matches the visual output.
         */
        if (viewportSize.x > 0 && viewportSize.y > 0 &&
           (viewportSize.x != (float)viewport.texture.width || viewportSize.y != (float)viewport.texture.height)) {
            UnloadRenderTexture(viewport);
            viewport = LoadRenderTexture((int)viewportSize.x, (int)viewportSize.y);
        }

        // Keep the scene boundaries synced with the current render target size
        scene.worldSize = Vector2{ (float)viewport.texture.width, (float)viewport.texture.height };

        /** @brief Step 1: Render the Game World into the off-screen buffer */
        BeginTextureMode(viewport);
            ClearBackground(Color{ 20, 20, 20, 255 });
            scene.Draw();
        EndTextureMode();

        /** @brief Step 2: Render the Editor UI to the main window */
        BeginDrawing();
            ClearBackground(GRAY);
            rlImGuiBegin();
                ShowMainMenuBar();
                ShowHierarchy();
                ShowViewport();
                ShowInspector();
            rlImGuiEnd();
        EndDrawing();
    }

    inline void Editor::ApplyDarkTheme(ImVec4* colors)
    {
        /**
         * @brief A modern dark theme for indium engine
         */

        // Define dark theme background and border colors for core ImGui UI elements.
        // These values establish the primary visual foundation of the dark mode interface,
        // using deep neutral grays to reduce eye strain while maintaining clear separation
        // between UI layers such as menus, windows, popups, and child panels.
        colors[ImGuiCol_MenuBarBg]              = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_WindowBg]               = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_ChildBg]                = ImVec4(24 / 255.0f, 24 / 255.0f, 24 / 255.0f, 1.0f);
        colors[ImGuiCol_PopupBg]                = ImVec4(24 / 255.0f, 24 / 255.0f, 24 / 255.0f, 1.0f);
        colors[ImGuiCol_Border]                 = ImVec4(40 / 255.0f, 40 / 255.0f, 40 / 255.0f, 1.0f);

        // Title bar background colors for ImGui windows (normal, active, and collapsed states).
        // All title states are intentionally set to the same very dark tone to create a uniform,
        // minimal, and distraction-free interface. This design choice removes visual noise between
        // active/inactive/collapsed states while maintaining consistency across the entire UI.
        // The result is a flat, modern dark aesthetic that prioritizes content over chrome.
        colors[ImGuiCol_TitleBg]                = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);

        // Header background colors for ImGui interactive elements (tree nodes, collapsible headers, selectable sections).
        // These states define how UI headers respond visually to user interaction:
        // - Default state uses a dark neutral gray for subtle separation from background layers.
        // - Hovered state becomes lighter to indicate interactivity and cursor focus.
        // - Active state becomes even brighter to confirm selection or expansion.
        // The gradual brightness increase improves usability while preserving a consistent dark UI aesthetic.
        colors[ImGuiCol_Header]                 = ImVec4(30 / 255.0f, 30 / 255.0f, 30 / 255.0f, 1.0f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(45 / 255.0f, 45 / 255.0f, 45 / 255.0f, 1.0f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(55 / 255.0f, 55 / 255.0f, 55 / 255.0f, 1.0f);

        // Button color states for interactive controls (normal, hovered, active/pressed).
        // The default button color is a dark gray, designed to blend naturally into the interface
        // without drawing excessive attention. Hover state provides clear feedback through a lighter tone,
        // while the active state becomes brighter to confirm a click or press action.
        // This layered contrast approach ensures usability while maintaining a restrained dark UI style.
        colors[ImGuiCol_Button]                 = ImVec4(35 / 255.0f, 35 / 255.0f, 35 / 255.0f, 1.0f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(50 / 255.0f, 50 / 255.0f, 50 / 255.0f, 1.0f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(65 / 255.0f, 65 / 255.0f, 65 / 255.0f, 1.0f);

        // Input field (frame) background colors for editable widgets such as text inputs, sliders, and numeric fields.
        // The default state uses a very dark background to integrate seamlessly with the overall UI.
        // Hover state slightly brightens the field to indicate interactivity and readiness for input.
        // Active state becomes more prominent to clearly show focus and keyboard interaction.
        // These subtle transitions improve usability while maintaining a consistent dark-themed design language.
        colors[ImGuiCol_FrameBg]                = ImVec4(25 / 255.0f, 25 / 255.0f, 25 / 255.0f, 1.0f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(35 / 255.0f, 35 / 255.0f, 35 / 255.0f, 1.0f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(45 / 255.0f, 45 / 255.0f, 45 / 255.0f, 1.0f);

        // Slider grab handle and checkbox checkmark colors for interactive controls.
        // In dark theme, the slider grabber is set to bright white to ensure strong visibility
        // against dark backgrounds, making it easy to locate and manipulate precisely.
        // When active, it becomes slightly dimmed to indicate ongoing interaction.
        // The checkbox checkmark is also rendered in white to maximize contrast and readability,
        // ensuring clear state indication without relying on color saturation.
        colors[ImGuiCol_SliderGrab]             = ImVec4(255 / 255.0f, 255 / 255.0f, 255 / 255.0f, 1.0f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(220 / 255.0f, 220 / 255.0f, 220 / 255.0f, 1.0f);
        colors[ImGuiCol_CheckMark]              = ImVec4(255 / 255.0f, 255 / 255.0f, 255 / 255.0f, 1.0f);

        // Scrollbar styling for ImGui (background, grab handle, and hover state).
        // The scrollbar background uses near-black tones to remain unobtrusive within the interface.
        // The grab handle is slightly lighter to ensure visibility without breaking the dark aesthetic.
        // On hover, the handle becomes brighter to provide clear interaction feedback.
        // This subtle contrast system improves usability while preserving a minimal dark UI style.
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(18 / 255.0f, 18 / 255.0f, 18 / 255.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(40 / 255.0f, 40 / 255.0f, 40 / 255.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(60 / 255.0f, 60 / 255.0f, 60 / 255.0f, 1.0f);

        // Text color settings for ImGui UI elements (normal and disabled states).
        // The main text color is a light gray (near white) to ensure high readability on dark backgrounds
        // while avoiding pure white, which can feel visually harsh during extended use.
        // Disabled text is rendered in a darker gray to clearly indicate inactive or unavailable elements,
        // maintaining a clear visual hierarchy between active and inactive UI components.
        colors[ImGuiCol_Text]                   = ImVec4(230 / 255.0f, 230 / 255.0f, 230 / 255.0f, 1.0f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(100 / 255.0f, 100 / 255.0f, 100 / 255.0f, 1.0f);
    }
    inline void Editor::ApplyLightTheme(ImVec4* colors)
    {
        /**
         * @brief A modern light theme for indium engine
        */

        // Define light theme background and border colors for various ImGui UI elements
        // Menu bar, main window, child windows, popups, and borders are set to subtle gray/white tones
        // to create a clean, minimal, and soft visual appearance.

        colors[ImGuiCol_MenuBarBg]              = ImVec4(235 / 255.0f, 235 / 255.0f, 235 / 255.0f, 1.0f);
        colors[ImGuiCol_WindowBg]               = ImVec4(245 / 255.0f, 245 / 255.0f, 245 / 255.0f, 1.0f);
        colors[ImGuiCol_ChildBg]                = ImVec4(250 / 255.0f, 250 / 255.0f, 250 / 255.0f, 1.0f);
        colors[ImGuiCol_PopupBg]                = ImVec4(255 / 255.0f, 255 / 255.0f, 255 / 255.0f, 1.0f);
        colors[ImGuiCol_Border]                 = ImVec4(190 / 255.0f, 190 / 255.0f, 190 / 255.0f, 1.0f);

        // Title bar background colors for ImGui windows (normal, active, and collapsed states).
        colors[ImGuiCol_TitleBg]                = ImVec4(235 / 255.0f, 235 / 255.0f, 235 / 255.0f, 1.0f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(235 / 255.0f, 235 / 255.0f, 235 / 255.0f, 1.0f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(235 / 255.0f, 235 / 255.0f, 235 / 255.0f, 1.0f);

        // Header background colors for ImGui elements (e.g., tree nodes, collapsible headers, and selectable sections).
        colors[ImGuiCol_Header]                 = ImVec4(205 / 255.0f, 205 / 255.0f, 205 / 255.0f, 1.0f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(185 / 255.0f, 185 / 255.0f, 185 / 255.0f, 1.0f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(165 / 255.0f, 165 / 255.0f, 165 / 255.0f, 1.0f);

        // Button color states for ImGui interactive controls (normal, hovered, and active/pressed).
        colors[ImGuiCol_Button]                 = ImVec4(220 / 255.0f, 220 / 255.0f, 220 / 255.0f, 1.0f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(200 / 255.0f, 200 / 255.0f, 200 / 255.0f, 1.0f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(180 / 255.0f, 180 / 255.0f, 180 / 255.0f, 1.0f);

        // Input field (frame) background colors for ImGui editable widgets such as text inputs, sliders, and numeric fields.
        colors[ImGuiCol_FrameBg]                = ImVec4(255 / 255.0f, 255 / 255.0f, 255 / 255.0f, 1.0f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(235 / 255.0f, 235 / 255.0f, 235 / 255.0f, 1.0f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(220 / 255.0f, 220 / 255.0f, 220 / 255.0f, 1.0f);

        // Slider grab handle and checkbox checkmark colors for ImGui interactive controls.
        colors[ImGuiCol_SliderGrab]             = ImVec4(90 / 255.0f, 90 / 255.0f, 90 / 255.0f, 1.0f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(60 / 255.0f, 60 / 255.0f, 60 / 255.0f, 1.0f);
        colors[ImGuiCol_CheckMark]              = ImVec4(40 / 255.0f, 40 / 255.0f, 40 / 255.0f, 1.0f);

        // Scrollbar styling for ImGui (background, grab handle, and hover state).
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(230 / 255.0f, 230 / 255.0f, 230 / 255.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(180 / 255.0f, 180 / 255.0f, 180 / 255.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(150 / 255.0f, 150 / 255.0f, 150 / 255.0f, 1.0f);

        // Text color settings for ImGui UI elements (normal and disabled states).
        colors[ImGuiCol_Text]                   = ImVec4(25 / 255.0f, 25 / 255.0f, 25 / 255.0f, 1.0f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(120 / 255.0f, 120 / 255.0f, 120 / 255.0f, 1.0f);
    }
    inline void Editor::ApplyTheme(std::string THEME_STYLE = "dark")
    {
        /**
         * @brief Applies a complete UI theme configuration for the editor.
         *
         * This function is responsible for setting both the ImGui style parameters
         * (such as spacing, rounding, padding, and borders) and selecting the active
         * color palette based on the requested theme style ("dark" or "light").
         */

        ImGuiStyle& style = ImGui::GetStyle();

        // Aesthetic Adjustments: Rounded corners and comfortable spacing
        // These style settings define the overall UI feel, balancing readability,
        // usability, and modern visual design principles.
        // - Rounding controls how soft or sharp UI elements appear.
        // - Padding defines internal spacing within widgets and windows.
        // - Spacing defines distance between UI elements for clarity.
        style.WindowRounding    = 0.0f;          // Sharp window edges for a clean, minimal frame style
        style.FrameRounding     = 6.0f;          // Soft rounded frames for interactive widgets
        style.PopupRounding     = 6.0f;          // Consistent rounding for popup windows
        style.ScrollbarRounding = 6.0f;          // Smooth rounded scrollbar design
        style.GrabRounding      = 12.0f;         // Highly rounded slider/drag handles for better visual focus
        style.TabRounding       = 4.0f;          // Slight rounding for tab elements to maintain hierarchy

        // Padding and spacing configuration for layout consistency
        // These values ensure UI elements are not visually cramped and maintain
        // a comfortable reading and interaction experience.
        style.WindowPadding     = ImVec2(12, 12); // Inner spacing inside windows
        style.FramePadding      = ImVec2(8, 6);   // Padding inside buttons, inputs, and frames
        style.ItemSpacing       = ImVec2(10, 12); // Spacing between consecutive UI items

        // Border configuration
        // Borders are disabled to achieve a flat, modern UI aesthetic without heavy outlines
        // that could visually clutter the interface.
        style.WindowBorderSize  = 0.0f;           // No window borders for a cleaner look
        style.FrameBorderSize   = 0.0f;           // No frame borders for minimal design

        // Scrollbar sizing
        // Defines the thickness of scrollbars, balancing usability and visual subtlety.
        style.ScrollbarSize     = 12.0f;

        // Color Palette
        // Pointer to ImGui color array used by the active style.
        ImVec4* colors = style.Colors;

        // Theme selection logic
        // Based on the provided THEME_STYLE string, the corresponding color palette
        // is applied. This allows dynamic switching between dark and light modes.
        if(THEME_STYLE == "dark")
        {
            ApplyDarkTheme(colors); // Apply modern dark theme palette
        }
        else if(THEME_STYLE == "light")
        {
            ApplyLightTheme(colors); // Apply modern light theme palette
        }
        else
        {
            // Fallback behavior:
            // If an unsupported or invalid theme name is provided, the system
            // defaults to a safe dark theme to ensure UI consistency and avoid
            // uninitialized or undefined styling states.
            THEME_STYLE = "dark";
            ApplyDarkTheme(colors);
        }
    }

    inline void Editor::ShowMainMenuBar()
    {
        /**
         * @brief Renders the main menu bar of the editor UI.
         *
         * This function builds the top-level application menu using ImGui.
         * It provides access to file operations, entity creation tools,
         * theme switching, and runtime state control (Play/Stop).
         */

        if (ImGui::BeginMainMenuBar())
        {
            // File menu: application-level actions such as exiting the editor
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Exit"))
                    CloseWindow(); // Gracefully closes the application window

                ImGui::EndMenu();
            }

            // Create menu: factory-based entity creation tools
            // These options allow the user to spawn new objects into the scene
            // using the internal entity factory system.
            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem("Circle"))
                {
                    // Create a circle entity and add it to the current scene
                    std::unique_ptr<Entity> e = factory.CreateCircle(scene);
                    scene.entities.push_back(std::move(e));
                }

                if (ImGui::MenuItem("Rectangle"))
                {
                    // Create a rectangle entity and add it to the current scene
                    std::unique_ptr<Entity> e = factory.CreateRectangle(scene);
                    scene.entities.push_back(std::move(e));
                }

                if (ImGui::MenuItem("Sprite"))
                {
                    // Create a sprite entity and add it to the current scene
                    std::unique_ptr<Entity> e = factory.CreateSprite(scene);
                    scene.entities.push_back(std::move(e));
                }

                ImGui::EndMenu();
            }

            // Theme menu: allows runtime switching between available UI themes
            // This updates ImGui styling dynamically without restarting the application.
            if (ImGui::BeginMenu("Theme"))
            {
                if (ImGui::MenuItem("Dark theme"))
                {
                    ApplyTheme("dark"); // Switch to dark mode
                }

                if (ImGui::MenuItem("Light theme"))
                {
                    ApplyTheme("light"); // Switch to light mode
                }

                ImGui::EndMenu();
            }

            // Center the Play/Stop button in the menu bar
            // This calculates available horizontal space and positions the button
            // in the middle of the menu bar for a balanced UI layout.
            float barWidth = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(barWidth / 2.0f);

            // Play/Stop toggle button:
            // - In Editor mode: switches to Play mode and saves the scene state
            // - In Play mode: restores the previous scene state and returns to Editor mode
            if (ImGui::Button(state == GameState::Editor ? "Play" : "Stop", ImVec2(50, 0)))
            {
                if (state == GameState::Editor)
                {
                    scene.Save();                 // Preserve current editor state
                    state = GameState::Play;      // Enter runtime/play mode
                }
                else
                {
                    scene.Restore();              // Restore saved editor state
                    state = GameState::Editor;    // Return to editor mode
                }
            }

            ImGui::EndMainMenuBar();
        }
    }

    inline void Editor::ShowHierarchy()
    {
        float menuBarH = ImGui::GetFrameHeight();
        float screenW  = (float)GetScreenWidth();
        float screenH  = (float)GetScreenHeight();
        float panelW   = 250.0f;

        ImGui::SetNextWindowPos(ImVec2(0, menuBarH));
        ImGui::SetNextWindowSize(ImVec2(panelW, screenH - menuBarH));
        ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        if (ImGui::Button("Add", ImVec2(-1, 0))) ImGui::OpenPopup("AddEntityPopup");

        if (ImGui::BeginPopup("AddEntityPopup"))
        {
            if (ImGui::MenuItem("Circle"))      scene.entities.push_back(factory.CreateCircle(scene));
            if (ImGui::MenuItem("Rectangle"))   scene.entities.push_back(factory.CreateRectangle(scene));
            if (ImGui::MenuItem("Plane"))       scene.entities.push_back(factory.CreatePlane(scene));
            if (ImGui::MenuItem("Sprite"))      scene.entities.push_back(factory.CreateSprite(scene));
            ImGui::EndPopup();
        }

        ImGui::Separator();

        int entityToDelete = -1;
        for (int i = 0; i < (int)scene.entities.size(); i++)
        {
            char label[128];
            snprintf(label, sizeof(label), "%s##%d", scene.entities[i]->name.c_str(), i);
            if (ImGui::Selectable(label, selectedIndex == i)) selectedIndex = i;

            // Right-click context menu for individual entities
            if (ImGui::BeginPopupContextItem())
            {
                selectedIndex = i;
                if (ImGui::MenuItem("Delete Entity")) entityToDelete = i;
                ImGui::EndPopup();
            }
        }

        // Execute deletion safely outside the loop
        if (entityToDelete != -1) DeleteEntity(*scene.entities[entityToDelete]);

        // Right-click context menu for empty space in the Hierarchy
        if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::BeginMenu("Add Entity"))
            {
                if (ImGui::MenuItem("Circle"))      scene.entities.push_back(factory.CreateCircle(scene));
                if (ImGui::MenuItem("Rectangle"))   scene.entities.push_back(factory.CreateRectangle(scene));
                if (ImGui::MenuItem("Plane"))       scene.entities.push_back(factory.CreatePlane(scene));
                if (ImGui::MenuItem("Sprite"))      scene.entities.push_back(factory.CreateSprite(scene));
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
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

        // Capture the viewport's position and size for mouse coordinate mapping
        viewportPos.x   = ImGui::GetCursorScreenPos().x;
        viewportPos.y   = ImGui::GetCursorScreenPos().y;
        viewportSize.x  = ImGui::GetContentRegionAvail().x;
        viewportSize.y  = ImGui::GetContentRegionAvail().y;
        viewportHovered = ImGui::IsWindowHovered();

        // Render the texture into the ImGui window, fitting it to the available space
        rlImGuiImageRenderTextureFit(&viewport, false);

        // Pop the 0,0 padding so context menu has normal padding
        ImGui::PopStyleVar();

        // Right-click Context Menu for Viewport
        if (ImGui::BeginPopupContextWindow("ViewportContext", ImGuiPopupFlags_MouseButtonRight))
        {
            if(state != GameState::Play)
            {
                if (contextEntityIndex != -1)
                {
                    // We right clicked on an entity
                    ImGui::TextColored(ImVec4(0,1,1,1), "%s", scene.entities[contextEntityIndex]->name.c_str());
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete"))
                    {
                        DeleteEntity(*scene.entities[contextEntityIndex]);
                    }
                }
                else
                {
                    // We right clicked on empty space
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Add Entity");
                    ImGui::Separator();

                    if (ImGui::MenuItem("Circle")) {
                        auto e = factory.CreateCircle(scene);
                        e->position = worldMouse;
                        scene.entities.push_back(std::move(e));
                    }
                    if (ImGui::MenuItem("Rectangle")) {
                        auto e = factory.CreateRectangle(scene);
                        e->position = worldMouse;
                        scene.entities.push_back(std::move(e));
                    }
                    if (ImGui::MenuItem("Plane")) {
                        auto e = factory.CreatePlane(scene);
                        e->position = worldMouse;
                        scene.entities.push_back(std::move(e));
                    }
                    if (ImGui::MenuItem("Sprite")) {
                        auto e = factory.CreateSprite(scene);
                        e->position = worldMouse;
                        scene.entities.push_back(std::move(e));
                    }
                }
            }
            else
            {
                ImGui::TextDisabled("No actions available");
            }
            ImGui::EndPopup();
        }

        ImGui::End();
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
            // Draw Entity and Component properties
            scene.entities[selectedIndex]->inspect();

            ImGui::Separator();
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
