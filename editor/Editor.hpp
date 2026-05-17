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
#include "../include/imgui_internal.h"
#include "../include/rlImGui.h"
#include "../src/Config.hpp"
#include "../core/Entity.hpp"
#include "../2D/entity/Circle.hpp"
#include "../2D/entity/Rectangle.hpp"
#include "../2D/entity/Plane.hpp"
#include "../core/AssetManager.hpp"
#include "../core/Component.hpp"
#include "../2D/component/BouncerComponent.hpp"
#include "../2D/component/RigidbodyComponent.hpp"
#include "../2D/component/CameraComponent.hpp"
#include "../2D/component/TriggerComponent.hpp"
#include "../2D/component/AnimatorComponent.hpp"
#include "../core/scene/Scene.hpp"
#include "../core/StoryState.hpp"
#include "../2D/entity/EntityFactory.hpp"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <variant>
#include <type_traits>

#include "Launcher.hpp"
#include "../core/ProjectManager.hpp"
#include "../core/ScriptManager.hpp"
#include "../include/extras/IconsFontAwesome6.h"

namespace Indium
{
    /**
     * @brief Defines the current operational mode of the engine.
     */
    enum class GameState { Launcher, Editor, Play };

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

        /** @brief The current state. */
        GameState           state = GameState::Launcher;

        /** @brief Project Management. */
        ProjectManager      pm;
        std::unique_ptr<Launcher> launcher;


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

        /** @brief The camera used to navigate the scene in Editor mode. */
        Camera2D            editorCamera = { 0 };

        /** @brief Entity index for the currently open Viewport context menu. */
        int                 contextEntityIndex = -1;

        /** @brief Auto-save settings. */
        bool                autoSaveEnabled = false;
        float               autoSaveTimer = 0.0f;
        float               autoSaveInterval = 60.0f; // Save every 60 seconds by default

        /** @brief UI state for Project Settings window. */
        bool                showProjectSettings = false;

        /** @brief Undo/Redo History Stacks */
        std::vector<nlohmann::json> undoStack;
        std::vector<nlohmann::json> redoStack;
        const size_t MaxUndoSteps = 100;

        /** @brief System clipboard abstraction */
        nlohmann::json      entityClipboard;

        /** @brief Console Log Entry */
        struct LogEntry {
            ImVec4 color;
            std::string level;
            std::string message;
            const char* icon;
        };
        std::vector<LogEntry> consoleLogs;

        inline static std::vector<LogEntry>* s_consoleLogs = nullptr;
        static void RaylibTraceCallback(int level, const char* text, va_list args);

        /** @brief Scene management modal state. */
        std::string         sceneRenameTarget;
        std::string         sceneDeleteTarget;
        char                sceneRenameBuffer[64] = {};
        bool                showNewSceneModal_    = false;
        bool                showRenameSceneModal_ = false;
        bool                showDeleteSceneModal_ = false;

        /** @brief Bottom panel height and visibility. */
        float               bottomPanelHeight    = 350.0f; // 350 by default
        float               bottomPanelMaxHeight = 500.0f;
        bool                showBottomPanel      = true;    // true by default
        bool                isResizingBottom     = false;

        /** @brief Side panel widths and resize state. */
        float               hierarchyWidth      = 250.0f;
        float               hierarchyMaxWidth   = 500.0f;
        float               inspectorWidth      = 350.0f;
        float               inspectorMaxWidth   = 500.0f;
        bool                isResizingHierarchy = false;
        bool                isResizingInspector = false;

        // --- Dirty / Save changes state ---
        bool                isDirty = false;
        bool                showUnsavedChangesPopup = false;
        bool                wantsToExit = false;
        bool                wantsToExitToLauncher = false;
        bool                shouldExitImmediately = false;

        // --- Marquee / Box Selection state ---
        bool                isSelectingBox = false;
        Vector2             selectBoxStart = { 0, 0 };

    public:
        Editor() = default;

        /** @brief Checks if the editor window should close, handling unsaved changes popup if necessary. */
        bool ShouldClose();

        /** @brief Initializes the engine, graphics context, and editor theme. */
        void Init(const Config& config);

        /** @brief Performs cleanup of graphics resources (RenderTextures, etc.). */
        void Shutdown();

        /** @brief Handles user input and scene logic updates. */
        void Update(float dt);

        /** @brief Executes the main rendering pass (UI + Viewport). */
        void Run();

    private:
        /** @brief Configures the modern, themed visual style for ImGui. */
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

        /** @brief Renders the project file system. */
        void ShowContentBrowser();

        /** @brief Renders engine and script logs. */
        void ShowConsole();

        /** @brief Renders the story flags/variables blackboard panel. */
        void ShowStoryState();

        /** @brief Removes an entity from the scene and resets the selection. */
        void DeleteEntity(Entity& entity);

        // --- Undo / Redo System ---
        void TakeSnapshot();
        void Undo();
        void Redo();

    private:
        /** @brief Converts Raylib mouse coordinates to the ImGui coordinate space used by editor panels. */
        Vector2 GetRaylibToImGuiScale() const
        {
            Vector2 scale = { 1, 1 };
#if defined(__APPLE__)
        /** APPLE logical resoltion is different from retina resolution */
            Vector2 dpiScale = GetWindowScaleDPI();

            if (dpiScale.x > 0.0f && GetRenderWidth() == GetScreenWidth())
                scale.x = dpiScale.x;
            if (dpiScale.y > 0.0f && GetRenderHeight() == GetScreenHeight())
                scale.y = dpiScale.y;
#endif
            return scale;
        }

        Vector2 GetImGuiSpaceMousePosition() const
        {
            Vector2 mouse = GetMousePosition();
            Vector2 scale = GetRaylibToImGuiScale();
            return Vector2{ mouse.x / scale.x, mouse.y / scale.y };
        }

        Vector2 GetImGuiSpaceMouseDelta() const
        {
            Vector2 delta = GetMouseDelta();
            Vector2 scale = GetRaylibToImGuiScale();
            return Vector2{ delta.x / scale.x, delta.y / scale.y };
        }

        /** @brief Gets the active camera (Editor camera or Entity camera if in Play mode). */
        Camera2D GetActiveCamera()
        {
            Camera2D cam = editorCamera;
            if (state == GameState::Play)
            {
                for (auto& e : scene.entities)
                {
                    for (auto& c : e->components)
                    {
                        if (auto* camComp = dynamic_cast<CameraComponent*>(c.get()))
                        {
                            if (camComp->isPrimary)
                            {
                                cam.target   = e->getGlobalPosition();
                                // Shake offset is in screen-space pixels so the effect is zoom-independent
                                cam.offset   = { viewportSize.x / 2.0f + camComp->GetShakeOffset().x,
                                                 viewportSize.y / 2.0f + camComp->GetShakeOffset().y };
                                cam.zoom     = camComp->zoom;
                                cam.rotation = camComp->GetEffectiveRotation() + camComp->GetShakeAngle();
                                return cam;
                            }
                        }
                    }
                }
            }
            return cam;
        }
     };

    /*
     * --- IMPLEMENTATION ---
     * Implementations are provided inline at the end of the heade r to ensure
     * all Entity and Component types are fully defined, preventing "incomplete type" errors.
     */

    inline bool Editor::ShouldClose()
    {
        if (shouldExitImmediately) return true;

        if (WindowShouldClose())
        {
            if (isDirty)
            {
                // Clear the window close flag in GLFW directly in memory.
                // In GLFW 3, the 'shouldClose' GLFWbool field is located at offset 32
                // (8 bytes next pointer + 6 * 4 bytes GLFWbools).
                void* windowHandle = GetWindowHandle();
                if (windowHandle)
                {
                    int* shouldClosePtr = (int*)((char*)windowHandle + 32);
                    *shouldClosePtr = 0;
                }

                showUnsavedChangesPopup = true;
                wantsToExit = true;
                return false;
            }
            else
            {
                return true;
            }
        }
        return false;
    }

    inline void Editor::Init(const Config& config)
    {
        this->config = config;

        // Initialize with a dummy size; Run() will dynamically resize to fit the UI layout.
        viewport = LoadRenderTexture(1, 1);

        editorCamera.zoom = 1.0f;
        editorCamera.target = { 0, 0 };
        editorCamera.offset = { 0, 0 };
        editorCamera.rotation = 0.0f;

        ApplyTheme(THEME_STYLE);
        launcher = std::make_unique<Launcher>(&pm);

        // Touch the story blackboard so it subscribes to NarrativeEvent before Play.
        StoryState::Get();

        s_consoleLogs = &consoleLogs;
        SetTraceLogCallback(RaylibTraceCallback);

        // Initial Logs
        consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO] ", "Indium Engine v0.1 initialized.", ICON_FA_CIRCLE_INFO});
        consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO] ", "Scripting System: Hot-reload ready.", ICON_FA_CIRCLE_INFO});
    }

    inline void Editor::Shutdown()
    {
        SetTraceLogCallback(nullptr);
        s_consoleLogs = nullptr;
        AssetManager::Get().Clear();
        UnloadRenderTexture(viewport);
    }

    inline void Editor::RaylibTraceCallback(int level, const char* text, va_list args)
    {
        if (!s_consoleLogs) return;

        char buf[1024];
        vsnprintf(buf, sizeof(buf), text, args);

        ImVec4 color;
        std::string label;
        const char* icon;

        switch (level)
        {
            case LOG_WARNING: color = {0.9f, 0.6f, 0.2f, 1.0f}; label = "[WARNING]"; icon = ICON_FA_EXCLAMATION; break;
            case LOG_ERROR:   color = {0.9f, 0.3f, 0.3f, 1.0f}; label = "[ERROR]";   icon = ICON_FA_XMARK;       break;
            case LOG_FATAL:   color = {1.0f, 0.1f, 0.1f, 1.0f}; label = "[FATAL]";   icon = ICON_FA_XMARK;       break;
            default:          color = {0.4f, 0.8f, 0.4f, 1.0f}; label = "[INFO]";    icon = ICON_FA_CIRCLE_INFO; break;
        }

        s_consoleLogs->push_back({color, label, buf, icon});
    }

    inline void Editor::Update(float dt)
    {
        if (state == GameState::Editor && autoSaveEnabled && pm.IsProjectOpen())
        {
            autoSaveTimer += dt;
            if (autoSaveTimer >= autoSaveInterval)
            {
                pm.SaveCurrentProject(scene);
                isDirty = false;
                autoSaveTimer = 0.0f;
            }
        }

        // Update logic
        Vector2 screenMouse = GetImGuiSpaceMousePosition();

        if (viewportHovered && state == GameState::Editor)
        {
            float wheel = GetMouseWheelMove();
            if (wheel != 0)
            {
                Vector2 mouseWorldPos = GetScreenToWorld2D({screenMouse.x - viewportPos.x, screenMouse.y - viewportPos.y}, editorCamera);
                editorCamera.offset = {screenMouse.x - viewportPos.x, screenMouse.y - viewportPos.y};
                editorCamera.target = mouseWorldPos;

                editorCamera.zoom += (wheel * 0.125f);
                if (editorCamera.zoom < 0.1f) editorCamera.zoom = 0.1f;
            }

            if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
            {
                Vector2 delta = GetImGuiSpaceMouseDelta();
                delta = Vector2Scale(delta, -1.0f / editorCamera.zoom);
                editorCamera.target = Vector2Add(editorCamera.target, delta);
            }
        }

        Camera2D activeCamera = GetActiveCamera();

        float scaleX = (viewportSize.x > 0) ? (float)viewport.texture.width  / viewportSize.x : 1.0f;
        float scaleY = (viewportSize.y > 0) ? (float)viewport.texture.height / viewportSize.y : 1.0f;

        Vector2 scaledMouse = {
            (screenMouse.x - viewportPos.x) * scaleX,
            (screenMouse.y - viewportPos.y) * scaleY
        };

        worldMouse = GetScreenToWorld2D(scaledMouse, activeCamera);

        if (state == GameState::Play) scene.Update(dt);
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

        /** @brief Step 1: Render the Game World into the off-screen buffer */
        BeginTextureMode(viewport);
            ClearBackground(Color{ 20, 20, 20, 255 });

            Camera2D activeCamera = GetActiveCamera();
            BeginMode2D(activeCamera);

            // --- Editor Grid (only in Editor mode, not during Play) ---
            if (state == GameState::Editor)
            {
                const float gridStep = 50.0f;
                const Color gridMinor = Color{ 50, 50, 50, 255 };   // fine grid lines
                const Color gridMajor = Color{ 70, 70, 70, 255 };   // every 5th line (250px)

                // Compute visible world bounds from camera
                float halfW = (viewport.texture.width  * 0.5f) / activeCamera.zoom;
                float halfH = (viewport.texture.height * 0.5f) / activeCamera.zoom;
                float worldLeft   = activeCamera.target.x - halfW;
                float worldRight  = activeCamera.target.x + halfW;
                float worldTop    = activeCamera.target.y - halfH;
                float worldBottom = activeCamera.target.y + halfH;

                // Snap to grid
                float startX = floorf(worldLeft  / gridStep) * gridStep;
                float startY = floorf(worldTop   / gridStep) * gridStep;

                int lineIndex = 0;
                for (float x = startX; x < worldRight + gridStep; x += gridStep, lineIndex++)
                {
                    bool major = (std::abs(fmodf(x, gridStep * 5.0f)) < 0.5f);
                    DrawLineV({ x, worldTop - gridStep }, { x, worldBottom + gridStep }, major ? gridMajor : gridMinor);
                }
                lineIndex = 0;
                for (float y = startY; y < worldBottom + gridStep; y += gridStep, lineIndex++)
                {
                    bool major = (std::abs(fmodf(y, gridStep * 5.0f)) < 0.5f);
                    DrawLineV({ worldLeft - gridStep, y }, { worldRight + gridStep, y }, major ? gridMajor : gridMinor);
                }

                // --- World bounds (the "camera output" rectangle) ---
                float wx = -(scene.worldSize.x * 0.5f);
                float wy = -(scene.worldSize.y * 0.5f);
                DrawRectangleLines((int)wx, (int)wy, (int)scene.worldSize.x, (int)scene.worldSize.y, Color{ 255, 255, 255, 60 });
                // Subtle inner fill to distinguish world from outside
                DrawRectangle((int)wx, (int)wy, (int)scene.worldSize.x, (int)scene.worldSize.y, Color{ 255, 255, 255, 6 });
            }

            scene.Draw();

            // --- Selection Outline (world-space — stays in BeginMode2D) ---
            if (state == GameState::Editor)
            {
                if (selectedIndex >= 0 && selectedIndex < (int)scene.entities.size())
                {
                    Entity* sel = scene.entities[selectedIndex].get();
                    if (sel)
                    {
                        const Color outlineColor = Color{ 0, 255, 255, 255 };
                        const float thickness = 2.0f;

                        Circle* circle = dynamic_cast<Circle*>(sel);
                        if (circle)
                        {
                            DrawCircleLinesV(circle->getGlobalPosition(), circle->radius + 2.0f, outlineColor);
                        }
                        else
                        {
                            std::vector<Vector2> verts = sel->getVertices();
                            if (!verts.empty())
                            {
                                for (size_t i = 0; i < verts.size(); i++)
                                {
                                    DrawLineEx(verts[i], verts[(i + 1) % verts.size()], thickness, outlineColor);
                                }
                            }
                            else
                            {
                                DrawRectangleLinesEx(sel->getBounds(), thickness, outlineColor);
                            }
                        }
                    }
                }
            }

            // --- Marquee / Box Selection Outline (world-space) ---
            if (state == GameState::Editor && isSelectingBox)
            {
                Vector2 p1 = selectBoxStart;
                Vector2 p2 = worldMouse;
                float x = std::min(p1.x, p2.x);
                float y = std::min(p1.y, p2.y);
                float w = std::abs(p1.x - p2.x);
                float h = std::abs(p1.y - p2.y);

                // Translucent fill
                DrawRectangleRec(::Rectangle{ x, y, w, h }, Color{ 0, 120, 255, 40 });
                // Light blue border
                DrawRectangleLinesEx(::Rectangle{ x, y, w, h }, 1.0f, Color{ 0, 120, 255, 200 });
            }

            EndMode2D();

            // --- Entity name labels (screen-space — after EndMode2D for constant pixel size) ---
            if (state == GameState::Editor)
            {
                const int fontSize = 10;
                for (int i = 0; i < (int)scene.entities.size(); i++)
                {
                    Entity* e = scene.entities[i].get();
                    if (!e) continue;

                    ::Rectangle bounds = e->getBounds();
                    Vector2 worldPos = { bounds.x + bounds.width * 0.5f, bounds.y + bounds.height };
                    Vector2 screenPos = GetWorldToScreen2D(worldPos, activeCamera);
                    screenPos.y += 4.0f;

                    float tw = (float)MeasureText(e->name.c_str(), fontSize);
                    Color col = (selectedIndex == i) ? Color{ 0, 255, 255, 255 } : Color{ 255, 255, 255, 150 };
                    DrawText(e->name.c_str(), (int)(screenPos.x - tw * 0.5f), (int)screenPos.y, fontSize, col);
                }
            }

        EndTextureMode();

        /** @brief Step 2: Render the Editor UI to the main window */
        BeginDrawing();
            ClearBackground(GRAY);
            rlImGuiBegin();

            // --- Unsaved Changes Modal Popup ---
            if (showUnsavedChangesPopup)
            {
                ImGui::OpenPopup("Save Changes?");
            }

            if (ImGui::BeginPopupModal("Save Changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("You have unsaved changes in your scene!\nDo you want to save them before exiting?");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Save & Exit", ImVec2(100, 0)))
                {
                    pm.SaveCurrentProject(scene);
                    isDirty = false;
                    showUnsavedChangesPopup = false;
                    if (wantsToExitToLauncher)
                    {
                        state = GameState::Launcher;
                        pm.CloseProject();
                        scene.entities.clear();
                        scene.snapshot.clear();
                        wantsToExitToLauncher = false;
                    }
                    else if (wantsToExit)
                    {
                        shouldExitImmediately = true;
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Discard", ImVec2(100, 0)))
                {
                    isDirty = false;
                    showUnsavedChangesPopup = false;
                    if (wantsToExitToLauncher)
                    {
                        state = GameState::Launcher;
                        pm.CloseProject();
                        scene.entities.clear();
                        scene.snapshot.clear();
                        wantsToExitToLauncher = false;
                    }
                    else if (wantsToExit)
                    {
                        shouldExitImmediately = true;
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(100, 0)))
                {
                    showUnsavedChangesPopup = false;
                    wantsToExit = false;
                    wantsToExitToLauncher = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if (state == GameState::Launcher)
            {
                if (launcher->Draw(&scene))
                {
                    state = GameState::Editor;
                    undoStack.clear();
                    redoStack.clear();
                    selectedIndex = -1;
                    isDirty = false;
                }
            }
            else
            {
                float menuBarH = ImGui::GetFrameHeight();
                float screenW = (float)GetScreenWidth();
                float screenH = (float)GetScreenHeight();

                // --- Global Resize Logic (Pre-Layout) ---
                if (showBottomPanel)
                {
                    float currentTopY = screenH - bottomPanelHeight;
                    // Check if mouse is near the top edge of the bottom panel
                    if (ImGui::GetIO().MousePos.y > currentTopY - 5.0f && ImGui::GetIO().MousePos.y < currentTopY + 5.0f && ImGui::GetIO().MousePos.x < screenW)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                        if (ImGui::IsMouseDown(0)) isResizingBottom = true;
                    }

                    if (isResizingBottom) {
                        if (ImGui::IsMouseDown(0))
                        {
                            bottomPanelHeight = screenH - ImGui::GetIO().MousePos.y;
                            if (bottomPanelHeight < 300.0f) bottomPanelHeight = 300.0f;                             /* minimum bottom panel height (300.0f)*/
                            if (bottomPanelHeight > bottomPanelMaxHeight) bottomPanelHeight = bottomPanelMaxHeight; /* maximum bottom panel height (500.0f)*/
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                        }
                        else
                        {
                            isResizingBottom = false;
                        }
                    }
                }

                float bottomH = showBottomPanel ? bottomPanelHeight : 0.0f;
                ShowMainMenuBar();

                // --- Scene Modals (must be outside BeginMainMenuBar context) ---
                if (showNewSceneModal_)    { ImGui::OpenPopup("NewScenePopup");    showNewSceneModal_    = false; }
                if (showRenameSceneModal_) { ImGui::OpenPopup("RenameScenePopup"); showRenameSceneModal_ = false; }
                if (showDeleteSceneModal_) { ImGui::OpenPopup("DeleteScenePopup"); showDeleteSceneModal_ = false; }

                ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                if (ImGui::BeginPopupModal("NewScenePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
                {
                    static char newSceneName[64] = "NewScene";
                    ImGui::Text("Scene Name:");
                    ImGui::SetNextItemWidth(260.0f);
                    ImGui::InputText("##NewSceneName", newSceneName, sizeof(newSceneName));
                    ImGui::Spacing();
                    if (ImGui::Button("Create", ImVec2(126, 0)))
                    {
                        if (newSceneName[0] != '\0')
                        {
                            pm.SaveCurrentProject(scene);
                            isDirty = false;
                            if (pm.CreateNewScene(newSceneName, scene))
                            {
                                selectedIndex = -1;
                                undoStack.clear();
                                redoStack.clear();
                            }
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(126, 0)))
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }

                ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                if (ImGui::BeginPopupModal("RenameScenePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
                {
                    ImGui::Text("New name for '%s':", fs::path(sceneRenameTarget).stem().string().c_str());
                    ImGui::SetNextItemWidth(260.0f);
                    ImGui::InputText("##RenameScene", sceneRenameBuffer, sizeof(sceneRenameBuffer));
                    ImGui::Spacing();
                    if (ImGui::Button("Rename", ImVec2(126, 0)))
                    {
                        if (sceneRenameBuffer[0] != '\0')
                        {
                            pm.RenameScene(sceneRenameTarget, sceneRenameBuffer);
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(126, 0)))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                if (ImGui::BeginPopupModal("DeleteScenePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
                {
                    ImGui::Text("Delete '%s'?", fs::path(sceneDeleteTarget).stem().string().c_str());
                    ImGui::TextDisabled("This cannot be undone.");
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
                    if (ImGui::Button("Delete", ImVec2(126, 0)))
                    {
                        pm.DeleteScene(sceneDeleteTarget);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(126, 0)))
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }

                // Adjust heights of side panels to leave room for bottom panel
                float mainAreaH = screenH - menuBarH - bottomH;

                // --- Side Panel Resize Logic (Pre-Layout) ---
                {
                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    bool inPanelRows = mousePos.y > menuBarH && mousePos.y < menuBarH + mainAreaH;

                    // Hierarchy: drag its right edge
                    if (inPanelRows && mousePos.x > hierarchyWidth - 5.0f && mousePos.x < hierarchyWidth + 5.0f)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        if (ImGui::IsMouseClicked(0)) isResizingHierarchy = true;
                    }
                    if (isResizingHierarchy)
                    {
                        if (ImGui::IsMouseDown(0))
                        {
                            hierarchyWidth = mousePos.x;
                            if(hierarchyWidth >= hierarchyMaxWidth)
                            {
                                hierarchyWidth = hierarchyMaxWidth;
                            }
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        }
                        else
                        {
                            isResizingHierarchy = false;
                        }
                    }

                    // Inspector: drag its left edge
                    float inspectorEdgeX = screenW - inspectorWidth;
                    if (inPanelRows && mousePos.x > inspectorEdgeX - 5.0f && mousePos.x < inspectorEdgeX + 5.0f)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        if (ImGui::IsMouseClicked(0)) isResizingInspector = true;
                    }
                    if (isResizingInspector)
                    {
                        if (ImGui::IsMouseDown(0))
                        {
                            inspectorWidth = screenW - mousePos.x;
                            if(inspectorWidth >= inspectorMaxWidth)
                            {
                                inspectorWidth = inspectorMaxWidth;
                            }
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        }
                        else
                        {
                            isResizingInspector = false;
                        }
                    }

                    // Clamp so neither panel collapses nor starves the viewport
                    const float minPanelW = 150.0f;
                    const float minViewportW = 200.0f;
                    hierarchyWidth = std::clamp(hierarchyWidth, minPanelW, std::max(minPanelW, screenW - inspectorWidth - minViewportW));
                    inspectorWidth = std::clamp(inspectorWidth, minPanelW, std::max(minPanelW, screenW - hierarchyWidth - minViewportW));
                }

                // Hierarchy
                ImGui::SetNextWindowPos(ImVec2(0, menuBarH));
                ImGui::SetNextWindowSize(ImVec2(hierarchyWidth, mainAreaH));
                ShowHierarchy();

                // Inspector
                ImGui::SetNextWindowPos(ImVec2(screenW - inspectorWidth, menuBarH));
                ImGui::SetNextWindowSize(ImVec2(inspectorWidth, mainAreaH));
                ShowInspector();

                // Viewport
                ImGui::SetNextWindowPos(ImVec2(hierarchyWidth, menuBarH));
                ImGui::SetNextWindowSize(ImVec2(screenW - hierarchyWidth - inspectorWidth, mainAreaH));
                ShowViewport();

                // Bottom Panel (Content Browser & Console)
                if (showBottomPanel)
                {
                    ImGui::SetNextWindowPos(ImVec2(0, menuBarH + mainAreaH));
                    ImGui::SetNextWindowSize(ImVec2(screenW, bottomH));

                    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
                    if (ImGui::Begin("BottomPanel", nullptr, windowFlags))
                    {
                        // Tab Bar
                        if (ImGui::BeginTabBar("BottomTabs"))
                        {
                            if (ImGui::BeginTabItem(ICON_FA_FOLDER_OPEN "  Content Browser"))
                            {
                                ShowContentBrowser();
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem(ICON_FA_TERMINAL "  Console"))
                            {
                                ShowConsole();
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem(ICON_FA_FLAG "  Story State"))
                            {
                                ShowStoryState();
                                ImGui::EndTabItem();
                            }

                            // Close Button - Aligned with tabs (No border)
                            ImGui::SameLine(ImGui::GetWindowWidth() - 30);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                            if (ImGui::Button(ICON_FA_XMARK, ImVec2(24, 20))) showBottomPanel = false;
                            ImGui::PopStyleColor();
                            ImGui::PopStyleVar();

                            ImGui::EndTabBar();
                        }
                    }
                    ImGui::End();
                }

                if (showProjectSettings)
                {
                    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowPos(
                        ImVec2(screenW * 0.5f, screenH * 0.5f),
                        ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

                    if (ImGui::Begin(ICON_FA_GEAR "  Project Settings", &showProjectSettings,
                                     ImGuiWindowFlags_NoCollapse))
                    {
                        // --- General ---
                        if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Indent(8.0f);

                            static char projNameBuf[128] = {};
                            static std::string lastProject;
                            if (lastProject != pm.GetCurrentProjectName())
                            {
                                lastProject = pm.GetCurrentProjectName();
                                strncpy(projNameBuf, lastProject.c_str(), sizeof(projNameBuf) - 1);
                            }

                            ImGui::Text("Project Name");
                            ImGui::PushItemWidth(-1);
                            if (ImGui::InputText("##ProjName", projNameBuf, sizeof(projNameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
                            {
                                pm.SetProjectName(projNameBuf);
                            }
                            ImGui::PopItemWidth();

                            ImGui::Spacing();
                            ImGui::Text("Project Path");
                            ImGui::PushItemWidth(-1);
                            ImGui::InputText("##ProjPath", const_cast<char*>(pm.GetCurrentProjectPath().c_str()), pm.GetCurrentProjectPath().size() + 1, ImGuiInputTextFlags_ReadOnly);
                            ImGui::PopItemWidth();

                            ImGui::Unindent(8.0f);
                        }

                        ImGui::Spacing();

                        // --- Scene ---
                        if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Indent(8.0f);

                            const std::vector<std::string> sceneList = pm.GetSceneList();
                            const std::string defaultScene = pm.GetDefaultSceneName();

                            ImGui::Text("Default Startup Scene");
                            ImGui::PushItemWidth(-1);
                            if (ImGui::BeginCombo("##DefaultScene",
                                                   defaultScene.empty() ? "(none)" : fs::path(defaultScene).stem().string().c_str()))
                            {
                                for (const auto& sf : sceneList)
                                {
                                    const std::string stem = fs::path(sf).stem().string();
                                    bool selected = (sf == defaultScene);
                                    if (ImGui::Selectable(stem.c_str(), selected))
                                        pm.SetDefaultScene(sf);
                                    if (selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::PopItemWidth();

                            ImGui::Spacing();
                            ImGui::Text("World Size (px)");
                            ImGui::PushItemWidth(-1);
                            int wSize[2] = { (int)scene.worldSize.x, (int)scene.worldSize.y };
                            bool worldSizeChanged = ImGui::DragInt2("##WorldSize", wSize, 1.0f, 64, 16384);
                            if (ImGui::IsItemActivated()) TakeSnapshot();
                            if (worldSizeChanged)
                            {
                                scene.worldSize.x = (float)wSize[0];
                                scene.worldSize.y = (float)wSize[1];
                            }
                            ImGui::PopItemWidth();

                            ImGui::Unindent(8.0f);
                        }

                        ImGui::Spacing();

                        // --- Auto Save ---
                        if (ImGui::CollapsingHeader("Auto Save", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Indent(8.0f);

                            ImGui::Checkbox("Enable", &autoSaveEnabled);
                            if (autoSaveEnabled)
                            {
                                ImGui::PushItemWidth(-1);
                                ImGui::DragFloat("##AutoSaveInterval", &autoSaveInterval, 1.0f, 10.0f, 3600.0f, "%.0f sec");
                                ImGui::PopItemWidth();
                                ImGui::TextDisabled("Next save in %.0f s", autoSaveInterval - autoSaveTimer);
                            }

                            ImGui::Unindent(8.0f);
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        if (ImGui::Button("Save Project Now", ImVec2(-1, 0)))
                        {
                            pm.SaveCurrentProject(scene);
                            isDirty = false;
                            consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO]", "Project saved.", ICON_FA_FLOPPY_DISK});
                        }
                    }
                    ImGui::End();
                }
            }

            rlImGuiEnd();
        EndDrawing();
    }

    inline void Editor::ApplyDarkTheme(ImVec4* colors)
    {
        /**
         * @brief A modern dark theme for indium engine
         */

        // Define dark theme background and border colors for core ImGui UI elements.
        colors[ImGuiCol_MenuBarBg]              = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_WindowBg]               = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_ChildBg]                = ImVec4(24 / 255.0f, 24 / 255.0f, 24 / 255.0f, 1.0f);
        colors[ImGuiCol_PopupBg]                = ImVec4(24 / 255.0f, 24 / 255.0f, 24 / 255.0f, 1.0f);
        colors[ImGuiCol_Border]                 = ImVec4(40 / 255.0f, 40 / 255.0f, 40 / 255.0f, 1.0f);

        // Title bar background colors for ImGui windows (normal, active, and collapsed states).
        colors[ImGuiCol_TitleBg]                = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);

        // Header background colors for ImGui interactive elements (tree nodes, collapsible headers, selectable sections).
        colors[ImGuiCol_Header]                 = ImVec4(30 / 255.0f, 30 / 255.0f, 30 / 255.0f, 1.0f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(45 / 255.0f, 45 / 255.0f, 45 / 255.0f, 1.0f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(55 / 255.0f, 55 / 255.0f, 55 / 255.0f, 1.0f);

        // Button color states for interactive controls (normal, hovered, active/pressed).
        colors[ImGuiCol_Button]                 = ImVec4(35 / 255.0f, 35 / 255.0f, 35 / 255.0f, 1.0f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(50 / 255.0f, 50 / 255.0f, 50 / 255.0f, 1.0f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(65 / 255.0f, 65 / 255.0f, 65 / 255.0f, 1.0f);

        // Input field (frame) background colors for editable widgets such as text inputs, sliders, and numeric fields.
        colors[ImGuiCol_FrameBg]                = ImVec4(25 / 255.0f, 25 / 255.0f, 25 / 255.0f, 1.0f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(35 / 255.0f, 35 / 255.0f, 35 / 255.0f, 1.0f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(45 / 255.0f, 45 / 255.0f, 45 / 255.0f, 1.0f);

        // Slider grab handle and checkbox checkmark colors for interactive controls.
        colors[ImGuiCol_SliderGrab]             = ImVec4(255 / 255.0f, 255 / 255.0f, 255 / 255.0f, 1.0f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(220 / 255.0f, 220 / 255.0f, 220 / 255.0f, 1.0f);
        colors[ImGuiCol_CheckMark]              = ImVec4(255 / 255.0f, 255 / 255.0f, 255 / 255.0f, 1.0f);

        // Scrollbar styling for ImGui (background, grab handle, and hover state).
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(18 / 255.0f, 18 / 255.0f, 18 / 255.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(40 / 255.0f, 40 / 255.0f, 40 / 255.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(60 / 255.0f, 60 / 255.0f, 60 / 255.0f, 1.0f);

        // Tab colors - Matching the stealth theme
        colors[ImGuiCol_Tab]                    = ImVec4(20 / 255.0f, 20 / 255.0f, 20 / 255.0f, 1.0f);
        colors[ImGuiCol_TabHovered]             = ImVec4(45 / 255.0f, 45 / 255.0f, 45 / 255.0f, 1.0f);
        colors[ImGuiCol_TabActive]              = ImVec4(35 / 255.0f, 35 / 255.0f, 35 / 255.0f, 1.0f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(20 / 255.0f, 20 / 255.0f, 20 / 255.0f, 1.0f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(30 / 255.0f, 30 / 255.0f, 30 / 255.0f, 1.0f);

        // Text color settings for ImGui UI elements (normal and disabled states).
        colors[ImGuiCol_Text]                   = ImVec4(230 / 255.0f, 230 / 255.0f, 230 / 255.0f, 1.0f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(100 / 255.0f, 100 / 255.0f, 100 / 255.0f, 1.0f);
    }
    inline void Editor::ApplyLightTheme(ImVec4* colors)
    {
        /**
         * @brief A modern light theme for indium engine
        */

        // Define light theme background and border colors for various ImGui UI elements
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
    inline void Editor::ApplyTheme(std::string THEME_STYLE)
    {
        /**
         * @brief Applies a complete UI theme configuration for the editor.
         *
         * This function is responsible for setting both the ImGui style parameters
         * (such as spacing, rounding, padding, and borders) and selecting the active
         * color palette based on the requested theme style ("dark" or "light").
         */

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // --- Global Visual Styling ---
        style.WindowRounding    = 0.0f;          // Sharp windows for industrial look
        style.ChildRounding     = 0.0f;          // Sharp child panels
        style.FrameRounding     = 4.0f;          // Rounded buttons and widgets
        style.PopupRounding     = 4.0f;          // Rounded popups
        style.ScrollbarRounding = 12.0f;
        style.GrabRounding      = 4.0f;
        style.TabRounding       = 0.0f;          // Sharp tabs to match windows

        style.WindowBorderSize  = 1.0f;
        style.FrameBorderSize   = 1.0f;
        style.PopupBorderSize   = 1.0f;

        style.WindowPadding     = ImVec2(12.0f, 10.0f);
        style.FramePadding      = ImVec2(8.0f, 5.0f);
        style.ItemSpacing       = ImVec2(8.0f, 7.0f);
        style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
        style.GrabMinSize       = 10.0f;

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
                if (ImGui::MenuItem("Save", "Ctrl+S"))
                {
                    pm.SaveCurrentProject(scene);
                    isDirty = false;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Exit to Launcher"))
                {
                    if (isDirty)
                    {
                        showUnsavedChangesPopup = true;
                        wantsToExitToLauncher = true;
                    }
                    else
                    {
                        state = GameState::Launcher;
                        pm.CloseProject();
                        scene.entities.clear();
                        scene.snapshot.clear();
                    }
                }

                if (ImGui::MenuItem("Exit"))
                {
                    if (isDirty)
                    {
                        showUnsavedChangesPopup = true;
                        wantsToExit = true;
                    }
                    else
                    {
                        shouldExitImmediately = true;
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                ImGui::MenuItem(ICON_FA_TERMINAL "  Bottom Panel", "Ctrl+B", &showBottomPanel);
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_GEAR "  Project Settings")) showProjectSettings = true;
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoStack.empty() && state != GameState::Play))
                    Undo();
                if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, !redoStack.empty() && state != GameState::Play))
                    Redo();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Scripts"))
            {
                if (ImGui::MenuItem("Compile & Reload"))
                {
                    if (pm.IsProjectOpen())
                    {
                        std::string logOutput;
                        if (ScriptManager::Get().CompileScripts(pm.GetCurrentProjectPath(), logOutput))
                        {
                            ScriptManager::Get().LoadLibrary(pm.GetCurrentProjectPath());
                            consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[COMPILER]", logOutput, ICON_FA_CHECK});
                        }
                        else
                        {
                            consoleLogs.push_back({ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "[COMPILER ERROR]", logOutput, ICON_FA_XMARK});
                        }
                    }
                    else
                    {
                        consoleLogs.push_back({ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "[WARNING]", "No project open to compile scripts.", ICON_FA_EXCLAMATION});
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Scenes"))
            {
                if (ImGui::MenuItem("New Scene..."))
                    showNewSceneModal_ = true;

                ImGui::Separator();

                const std::vector<std::string> sceneList = pm.GetSceneList();
                const std::string currentSceneFile = pm.GetCurrentSceneName() + ".scene";
                for (const auto& sceneFile : sceneList)
                {
                    const std::string displayName = fs::path(sceneFile).stem().string();
                    const bool isCurrent = (sceneFile == currentSceneFile);

                    ImGui::PushID(sceneFile.c_str());
                    if (ImGui::MenuItem(displayName.c_str(), nullptr, isCurrent, !isCurrent))
                    {
                        pm.SaveCurrentProject(scene);
                        isDirty = false;
                        if (pm.SwitchScene(sceneFile, scene))
                        {
                            selectedIndex = -1;
                            undoStack.clear();
                            redoStack.clear();
                        }
                    }
                    ImGui::PopID();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Rename Current Scene..."))
                {
                    sceneRenameTarget = currentSceneFile;
                    strncpy(sceneRenameBuffer, pm.GetCurrentSceneName().c_str(), sizeof(sceneRenameBuffer) - 1);
                    showRenameSceneModal_ = true;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                bool canDelete = sceneList.size() > 1;
                if (ImGui::MenuItem("Delete Current Scene...", nullptr, false, canDelete))
                {
                    sceneDeleteTarget = currentSceneFile;
                    showDeleteSceneModal_ = true;
                }
                ImGui::PopStyleColor();

                ImGui::EndMenu();
            }

            // Handle global hotkeys
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
            {
                pm.SaveCurrentProject(scene);
                isDirty = false;
            }
            if (IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_Z))
            {
                Undo();
            }
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_Z))
            {
                Redo();
            }

            // Create menu: factory-based entity creation tools
            // These options allow the user to spawn new objects into the scene
            // using the internal entity factory system.
            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem("Circle"))
                {
                    TakeSnapshot();
                    auto e = factory.CreateCircle(scene);
                    e->position = editorCamera.target;
                    scene.entities.push_back(std::move(e));
                }
                if (ImGui::MenuItem("Rectangle"))
                {
                    TakeSnapshot();
                    auto e = factory.CreateRectangle(scene);
                    e->position = editorCamera.target;
                    scene.entities.push_back(std::move(e));
                }
                if (ImGui::MenuItem("Surface (Plane)"))
                {
                    TakeSnapshot();
                    auto e = factory.CreatePlane(scene);
                    e->position = editorCamera.target;
                    scene.entities.push_back(std::move(e));
                }
                if (ImGui::MenuItem("Image (Sprite)"))
                {
                    TakeSnapshot();
                    auto e = factory.CreateSprite(scene);
                    e->position = editorCamera.target;
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

            // Center Play/Stop button relative to the full menu bar width
            const float playBtnW = 60.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - playBtnW) * 0.5f);

            // Play/Stop toggle button:
            // - In Editor mode: switches to Play mode and saves the scene state
            // - In Play mode: restores the previous scene state and returns to Editor mode
            if (ImGui::Button(state == GameState::Editor ? "  Play  " : "  Stop  ", ImVec2(playBtnW, 0)))
            {
                if (state == GameState::Editor)
                {
                    scene.Save();
                    state = GameState::Play;

                    // Seed the global blackboard with this scene's authored flags
                    // before any script's OnStart runs.
                    StoryState::Get().Clear();
                    StoryState::Get().Seed(scene.storyState);

                    for (auto& e : scene.entities)
                    {
                        for (auto& c : e->components) {
                            c->start(&scene);
                        }
                    }
                }
                else
                {
                    scene.Restore();
                    state = GameState::Editor;
                    selectedIndex = -1;

                    // Discard runtime story mutations; authored values are kept.
                    StoryState::Get().Clear();
                }
            }

            // FPS readout (right-aligned), shown only when enabled in config.json
            if (config.showFps)
            {
                char fpsText[32];
                snprintf(fpsText, sizeof(fpsText), "%d FPS", GetFPS());
                float textW = ImGui::CalcTextSize(fpsText).x;
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - textW - 12.0f);
                ImGui::TextDisabled("%s", fpsText);
            }


            ImGui::EndMainMenuBar();
        }
    }

    inline void Editor::ShowHierarchy()
    {
        ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
        if (ImGui::Button("+ Add Entity", ImVec2(-1, 0))) ImGui::OpenPopup("AddEntityPopup");
        ImGui::PopStyleVar();

        if (ImGui::BeginPopup("AddEntityPopup"))
        {
            ImGui::TextDisabled("Create New...");
            ImGui::Separator();
            if (ImGui::MenuItem("Circle"))          { TakeSnapshot(); auto e = factory.CreateCircle(scene);    e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            if (ImGui::MenuItem("Rectangle"))       { TakeSnapshot(); auto e = factory.CreateRectangle(scene); e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            if (ImGui::MenuItem("Surface (Plane)")) { TakeSnapshot(); auto e = factory.CreatePlane(scene);     e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            if (ImGui::MenuItem("Image (Sprite)"))  { TakeSnapshot(); auto e = factory.CreateSprite(scene);    e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            ImGui::EndPopup();
        }

        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5));

        // --- Scene Root Label ---
        ImGui::TextDisabled("SCENE GRAPH");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 2));

        int entityToDelete = -1;

        // Lambda: recursively draw an entity as a tree node
        std::function<void(Entity*, int)> DrawEntityNode;
        DrawEntityNode = [&](Entity* entity, int index)
        {
            ImGui::PushID(entity->id);

            // --- Premium Stealth Styling ---
            ImGuiStyle& style = ImGui::GetStyle();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            float fullWidth = ImGui::GetContentRegionAvail().x;
            float nodeHeight = 30.0f;

            // Entity Icons - minimalist & consistent
            const char* icon = ICON_FA_CUBE;
            if (entity->getType() == "Circle")    icon = ICON_FA_CIRCLE;
            if (entity->getType() == "Rectangle") icon = ICON_FA_VECTOR_SQUARE;
            if (entity->getType() == "Plane")     icon = ICON_FA_LAYER_GROUP;
            if (entity->getType() == "Sprite")    icon = ICON_FA_IMAGE;

            // 1. Draw Rounded Selection Background (If selected)
            if (selectedIndex == index)
            {
                ImU32 selCol = ImGui::GetColorU32(ImVec4(0.18f, 0.18f, 0.18f, 1.0f)); // Subtle dark grey
                drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + fullWidth, cursorPos.y + nodeHeight), selCol, 4.0f);

                // Optional: thin accent line on the left
                drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + 3, cursorPos.y + nodeHeight), ImColor(100, 100, 100, 255), 4.0f);
            }

            // 2. Node Styling
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));

            // Clear default header colors to use our custom background
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

            bool isLeaf = entity->children.empty();
            if (isLeaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (selectedIndex == index) flags |= ImGuiTreeNodeFlags_Selected;

            // 3. Draw the Node
            char label[256];
            snprintf(label, sizeof(label), "%s %s", icon, entity->name.c_str());
            bool nodeOpen = ImGui::TreeNodeEx("##node", flags, "%s", label);
            bool wasPushed = nodeOpen && !isLeaf;

            // --- Interaction Logic ---
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                selectedIndex = index;

            // Drag Source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload("ENTITY_ID", &entity->id, sizeof(int));
                ImGui::Text("Moving %s", entity->name.c_str());
                ImGui::EndDragDropSource();
            }

            // Drop Target
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID"))
                {
                    int draggedId = *(const int*)payload->Data;
                    if (draggedId != entity->id)
                    {
                        Entity* dragged = scene.FindEntity(draggedId);
                        if (dragged)
                        {
                            bool isDescendant = false;
                            Entity* temp = entity->parent;
                            while (temp) { if (temp->id == draggedId) { isDescendant = true; break; } temp = temp->parent; }

                            if (!isDescendant)
                            {
                                TakeSnapshot();
                                if (dragged->parent) {
                                    auto& sibs = dragged->parent->children;
                                    sibs.erase(std::remove(sibs.begin(), sibs.end(), dragged), sibs.end());
                                }
                                dragged->parent = entity;
                                dragged->parentId = entity->id;
                                entity->children.push_back(dragged);
                                Vector2 gPos = dragged->getGlobalPosition();
                                dragged->setGlobalPosition(gPos);
                            }
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Context Menu
            if (ImGui::BeginPopupContextItem())
            {
                selectedIndex = index;
                ImGui::TextDisabled("Entity: %s", entity->name.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem("Copy", "Ctrl+C"))   entityClipboard = entity->serialize();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
                {
                    TakeSnapshot();
                    auto dup = factory.LoadEntity(entity->serialize());
                    if (dup) {
                        dup->id = scene.nextEntityId++;
                        dup->name = entity->name + " (Copy)";
                        dup->parent = entity->parent;
                        dup->parentId = entity->parentId;
                        if (entity->parent) entity->parent->children.push_back(dup.get());
                        scene.entities.push_back(std::move(dup));
                    }
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Unparent", nullptr, false, entity->parent != nullptr))
                {
                    if (entity->parent)
                    {
                        TakeSnapshot();
                        Vector2 gPos = entity->getGlobalPosition();
                        auto& siblings = entity->parent->children;
                        siblings.erase(std::remove(siblings.begin(), siblings.end(), entity), siblings.end());
                        entity->parent = nullptr;
                        entity->parentId = -1;
                        entity->position = gPos;
                    }
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Del")) entityToDelete = index;

                ImGui::EndPopup();
            }

            // Recurse into children
            if (wasPushed)
            {
                if (!entity->children.empty())
                {
                    for (Entity* child : entity->children)
                    {
                        int childIdx = -1;
                        for (int ci = 0; ci < (int)scene.entities.size(); ci++)
                        {
                            if (scene.entities[ci].get() == child) { childIdx = ci; break; }
                        }
                        if (childIdx != -1) DrawEntityNode(child, childIdx);
                    }
                }
                ImGui::TreePop();
            }

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
            ImGui::PopID();
        };

        // Draw only root entities (those without a parent)
        for (int i = 0; i < (int)scene.entities.size(); i++)
        {
            if (scene.entities[i]->parent == nullptr)
            {
                DrawEntityNode(scene.entities[i].get(), i);
            }
        }

        // --- Drop target for empty space (unparent) ---
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID"))
            {
                int draggedId = *(const int*)payload->Data;
                Entity* dragged = scene.FindEntity(draggedId);
                if (dragged && dragged->parent)
                {
                    TakeSnapshot();
                    Vector2 gPos = dragged->getGlobalPosition();
                    auto& siblings = dragged->parent->children;
                    siblings.erase(std::remove(siblings.begin(), siblings.end(), dragged), siblings.end());
                    dragged->parent = nullptr;
                    dragged->parentId = -1;
                    dragged->position = gPos;
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Execute deletion safely outside the loop
        if (entityToDelete != -1) DeleteEntity(*scene.entities[entityToDelete]);

        // --- Keyboard Shortcuts ---
        if (!ImGui::GetIO().WantTextInput && state == GameState::Editor)
        {
            bool hasSelection = selectedIndex >= 0 && selectedIndex < (int)scene.entities.size();

            if (hasSelection && IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_C))
                entityClipboard = scene.entities[selectedIndex]->serialize();

            if (hasSelection && IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D))
            {
                TakeSnapshot();
                Entity* ent = scene.entities[selectedIndex].get();
                auto dup = factory.LoadEntity(ent->serialize());
                if (dup) {
                    dup->id = scene.nextEntityId++;
                    dup->name = ent->name + " (Copy)";
                    dup->parent = ent->parent;
                    dup->parentId = ent->parentId;
                    if (ent->parent) ent->parent->children.push_back(dup.get());
                    scene.entities.push_back(std::move(dup));
                }
            }

            if (!entityClipboard.is_null() && IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V))
            {
                TakeSnapshot();
                auto pasted = factory.LoadEntity(entityClipboard);
                if (pasted) {
                    pasted->id = scene.nextEntityId++;
                    scene.entities.push_back(std::move(pasted));
                }
            }

            if (hasSelection && (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)))
            {
                TakeSnapshot();
                DeleteEntity(*scene.entities[selectedIndex]);
            }
        }

        // Right-click context menu for empty space in the Hierarchy
        if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::BeginMenu("Add Entity"))
            {
                if (ImGui::MenuItem("Circle"))          { TakeSnapshot(); auto e = factory.CreateCircle(scene);    e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
                if (ImGui::MenuItem("Rectangle"))       { TakeSnapshot(); auto e = factory.CreateRectangle(scene); e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
                if (ImGui::MenuItem("Surface (Plane)")) { TakeSnapshot(); auto e = factory.CreatePlane(scene);     e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
                if (ImGui::MenuItem("Image (Sprite)"))  { TakeSnapshot(); auto e = factory.CreateSprite(scene);    e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !entityClipboard.is_null()))
            {
                TakeSnapshot();
                auto pasted = factory.LoadEntity(entityClipboard);
                if (pasted)
                {
                    pasted->id = scene.nextEntityId++;
                    scene.entities.push_back(std::move(pasted));
                }
            }
            ImGui::EndPopup();
        }
        ImGui::End();
    }

    inline void Editor::ShowViewport()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImVec2 viewportAvail = ImGui::GetContentRegionAvail();
        ImVec2 renderArea = ImVec2(std::max(viewportAvail.x, 1.0f), std::max(viewportAvail.y, 1.0f));

        ImGui::BeginChild("ViewportRender", renderArea, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Capture the viewport's position and size for mouse coordinate mapping
        viewportPos.x   = ImGui::GetCursorScreenPos().x;
        viewportPos.y   = ImGui::GetCursorScreenPos().y;
        viewportSize.x  = ImGui::GetContentRegionAvail().x;
        viewportSize.y  = ImGui::GetContentRegionAvail().y;
        viewportHovered = ImGui::IsWindowHovered() && !isResizingHierarchy && !isResizingInspector && !isResizingBottom;

        // Render the texture into the ImGui window
        rlImGuiImageRenderTextureFit(&viewport, false);

        // --- Viewport Interaction Logic ---
        if (viewportHovered)
        {
            // Update worldMouse accurately based on current frame viewport data
            Camera2D activeCamera = GetActiveCamera();
            Vector2 screenMouse = GetMousePosition();

            float scaleX = (viewportSize.x > 0) ? (float)viewport.texture.width  / viewportSize.x : 1.0f;
            float scaleY = (viewportSize.y > 0) ? (float)viewport.texture.height / viewportSize.y : 1.0f;

            Vector2 scaledMouse = {
                (screenMouse.x - viewportPos.x) * scaleX,
                (screenMouse.y - viewportPos.y) * scaleY
            };

            worldMouse = GetScreenToWorld2D(scaledMouse, activeCamera);

            // 1. Camera Panning (Middle Mouse)
            if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
            {
                Vector2 delta = GetMouseDelta();
                delta = Vector2Scale(delta, -1.0f / editorCamera.zoom);
                editorCamera.target = Vector2Add(editorCamera.target, delta);
            }

            // 2. Camera Zoom (Mouse Wheel) — zooms toward cursor position
            if (state == GameState::Editor)
            {
                float wheel = GetMouseWheelMove();
                if (wheel != 0.0f)
                {
                    float oldZoom = editorCamera.zoom;
                    float newZoom = Clamp(oldZoom * (1.0f + wheel * 0.1f), 0.05f, 32.0f);
                    // Keep the world point under the cursor stationary during zoom
                    editorCamera.target.x = worldMouse.x - (worldMouse.x - editorCamera.target.x) * (oldZoom / newZoom);
                    editorCamera.target.y = worldMouse.y - (worldMouse.y - editorCamera.target.y) * (oldZoom / newZoom);
                    editorCamera.zoom = newZoom;
                }
            }

            // 3. Selection (Left Click)
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                draggingEntity = nullptr;
                isSelectingBox = false;
                bool picked = false;
                for (int i = (int)scene.entities.size() - 1; i >= 0; i--)
                {
                    if (scene.entities[i]->Contains(worldMouse))
                    {
                        if (state == GameState::Editor)
                        {
                            TakeSnapshot(); // Record undo state before we start dragging
                            draggingEntity = scene.entities[i].get();
                            dragOffset = Vector2{ draggingEntity->position.x - worldMouse.x, draggingEntity->position.y - worldMouse.y };
                        }
                        selectedIndex = i;
                        picked = true;
                        break;
                    }
                }
                if (!picked)
                {
                    selectedIndex = -1; // Deselect when clicking empty space
                    if (state == GameState::Editor)
                    {
                        isSelectingBox = true;
                        selectBoxStart = worldMouse;
                    }
                }
            }

            // 4. Context Menu (Right Click)
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
            {
                contextEntityIndex = -1;
                for (int i = (int)scene.entities.size() - 1; i >= 0; i--)
                {
                    if (scene.entities[i]->Contains(worldMouse))
                    {
                        contextEntityIndex = i;
                        selectedIndex = i;
                        break;
                    }
                }
            }

            // 5. Dragging Logic
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggingEntity != nullptr)
            {
                draggingEntity->position.x = worldMouse.x + dragOffset.x;
                draggingEntity->position.y = worldMouse.y + dragOffset.y;
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
            {
                draggingEntity = nullptr;
                if (isSelectingBox)
                {
                    isSelectingBox = false;
                    Vector2 p1 = selectBoxStart;
                    Vector2 p2 = worldMouse;
                    float x = std::min(p1.x, p2.x);
                    float y = std::min(p1.y, p2.y);
                    float w = std::abs(p1.x - p2.x);
                    float h = std::abs(p1.y - p2.y);

                    if (w > 2.0f && h > 2.0f)
                    {
                        ::Rectangle r = { x, y, w, h };
                        bool found = false;
                        for (int i = (int)scene.entities.size() - 1; i >= 0; i--)
                        {
                            if (CheckCollisionRecs(r, scene.entities[i]->getBounds()))
                            {
                                selectedIndex = i;
                                found = true;
                                break;
                            }
                        }
                        if (!found) selectedIndex = -1;
                    }
                }
            }
        }

        // Right-click Context Menu for Viewport
        // Restore proper padding — viewport window uses {0,0} for seamless rendering,
        // which would make the popup menu text flush against the edges without this override.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
        if (ImGui::BeginPopupContextWindow("ViewportContext", ImGuiPopupFlags_MouseButtonRight))
        {
            if(state != GameState::Play)
            {
                if (contextEntityIndex != -1)
                {
                    Entity* contextEntity = scene.entities[contextEntityIndex].get();
                    ImGui::TextColored(ImVec4(0, 1, 1, 1), "%s", contextEntity->name.c_str());
                    ImGui::Separator();

                    if (ImGui::MenuItem("Copy", "Ctrl+C"))
                    {
                        entityClipboard = contextEntity->serialize();
                    }
                    if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
                    {
                        TakeSnapshot();
                        auto dup = factory.LoadEntity(contextEntity->serialize());
                        if (dup)
                        {
                            dup->id = scene.nextEntityId++;
                            dup->name = contextEntity->name + " (Copy)";
                            dup->position = Vector2Add(dup->position, {10, 10});
                            scene.entities.push_back(std::move(dup));
                        }
                    }
                    if (ImGui::MenuItem("Delete", "Del"))
                    {
                        DeleteEntity(*contextEntity);
                    }
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Scene Actions");
                    ImGui::Separator();

                    if (ImGui::BeginMenu("Add Entity"))
                    {
                        if (ImGui::MenuItem("Circle")) {
                            TakeSnapshot();
                            auto e = factory.CreateCircle(scene);
                            e->position = worldMouse;
                            scene.entities.push_back(std::move(e));
                        }
                        if (ImGui::MenuItem("Rectangle")) {
                            TakeSnapshot();
                            auto e = factory.CreateRectangle(scene);
                            e->position = worldMouse;
                            scene.entities.push_back(std::move(e));
                        }
                        if (ImGui::MenuItem("Surface (Plane)")) {
                            TakeSnapshot();
                            auto e = factory.CreatePlane(scene);
                            e->position = worldMouse;
                            scene.entities.push_back(std::move(e));
                        }
                        if (ImGui::MenuItem("Image (Sprite)")) {
                            TakeSnapshot();
                            auto e = factory.CreateSprite(scene);
                            e->position = worldMouse;
                            scene.entities.push_back(std::move(e));
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::MenuItem("Paste", "Ctrl+V", false, !entityClipboard.is_null()))
                    {
                        TakeSnapshot();
                        auto pasted = factory.LoadEntity(entityClipboard);
                        if (pasted)
                        {
                            pasted->id = scene.nextEntityId++;
                            pasted->position = worldMouse;
                            scene.entities.push_back(std::move(pasted));
                        }
                    }
                }
            }
            else
            {
                ImGui::TextDisabled("No actions available in Play Mode");
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(); // restore WindowPadding from viewport override

        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::End();
    }

    inline void Editor::ShowInspector()
    {
        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        if (selectedIndex != -1 && selectedIndex < (int)scene.entities.size())
        {
            Entity* inspected = scene.entities[selectedIndex].get();
            inspected->pendingRemoveComponentIndex = -1;

            inspected->inspect();

            if (inspected->pendingRemoveComponentIndex != -1)
            {
                TakeSnapshot();
                inspected->removeComponent(inspected->pendingRemoveComponentIndex);
                inspected->pendingRemoveComponentIndex = -1;
            }

            ImGui::Separator();
            if(ImGui::Button("Add Component", ImVec2(-1, 0))) ImGui::OpenPopup("Component Popup");

            if(ImGui::BeginPopup("Component Popup"))
            {
                if(ImGui::MenuItem("Add Rigidbody")) { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<RigidbodyComponent>(); }
                if(ImGui::MenuItem("Add Bouncer"))   { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<BouncerComponent>(); }
                if(ImGui::MenuItem("Add Camera"))    { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<CameraComponent>(); }
                if(ImGui::MenuItem("Add Trigger"))   { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<TriggerComponent>(); }
                if(ImGui::MenuItem("Add Animator"))  { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<AnimatorComponent>(); }

                ImGui::Separator();
                ImGui::TextDisabled("Scripts");

                const auto& scripts = ScriptManager::Get().GetAvailableScripts();
                if (scripts.empty())
                {
                    ImGui::TextDisabled("(No scripts compiled)");
                }
                else
                {
                    for (const auto& sName : scripts)
                    {
                        if (ImGui::MenuItem(sName.c_str()))
                        {
                            Component* newComp = ScriptManager::Get().InstantiateScript(sName);
                            if (newComp)
                            {
                                TakeSnapshot();
                                auto* ptr = scene.entities[selectedIndex]->addComponent(std::unique_ptr<Component>(newComp));
                                if (state == GameState::Play) ptr->start(&scene);
                            }
                        }
                    }
                }

                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    inline void Editor::DeleteEntity(Entity& entity)
    {
        TakeSnapshot();

        std::function<void(Entity&)> doDelete = [&](Entity& ent) {
            // Recursively delete all children
            std::vector<Entity*> childrenCopy = ent.children;
            for (Entity* child : childrenCopy)
            {
                doDelete(*child);
            }

            auto it = std::find_if(scene.entities.begin(), scene.entities.end(),
            [&](const std::unique_ptr<Entity>& e) { return e.get() == &ent; });

            if (it != scene.entities.end())
            {
                // Detach from parent before removal
                if (ent.parent)
                {
                    auto& sibs = ent.parent->children;
                    sibs.erase(std::remove(sibs.begin(), sibs.end(), &ent), sibs.end());
                }
                scene.entities.erase(it);
            }
        };

        doDelete(entity);
        selectedIndex = -1;
    }

    inline void Editor::TakeSnapshot()
    {
        if (state == GameState::Play) return; // Don't record undo states during gameplay

        undoStack.push_back(scene.serialize());
        if (undoStack.size() > MaxUndoSteps)
        {
            undoStack.erase(undoStack.begin());
        }
        redoStack.clear(); // A new action invalidates the redo history
        isDirty = true;
    }

    inline void Editor::Undo()
    {
        if (undoStack.empty() || state == GameState::Play) return;

        redoStack.push_back(scene.serialize());

        nlohmann::json prevState = undoStack.back();
        undoStack.pop_back();

        scene.entities.clear();
        scene.nextEntityId = prevState.contains("nextEntityId") ? prevState["nextEntityId"].get<int>() : 1;

        if (prevState.contains("worldSize"))
        {
            scene.worldSize.x = prevState["worldSize"][0].get<float>();
            scene.worldSize.y = prevState["worldSize"][1].get<float>();
        }

        if (prevState.contains("entities"))
        {
            for (const auto& ej : prevState["entities"])
            {
                auto entity = factory.LoadEntity(ej);
                if (entity) scene.entities.push_back(std::move(entity));
            }
            scene.RebuildHierarchy();
        }

        scene.storyState = prevState.contains("storyState")
            ? StoryValueMapFromJson(prevState["storyState"])
            : std::map<std::string, StoryValue>{};

        selectedIndex = -1;
    }

    inline void Editor::Redo()
    {
        if (redoStack.empty() || state == GameState::Play) return;

        undoStack.push_back(scene.serialize());

        nlohmann::json nextState = redoStack.back();
        redoStack.pop_back();

        scene.entities.clear();
        scene.nextEntityId = nextState.contains("nextEntityId") ? nextState["nextEntityId"].get<int>() : 1;

        if (nextState.contains("worldSize"))
        {
            scene.worldSize.x = nextState["worldSize"][0].get<float>();
            scene.worldSize.y = nextState["worldSize"][1].get<float>();
        }

        if (nextState.contains("entities"))
        {
            for (const auto& ej : nextState["entities"])
            {
                auto entity = factory.LoadEntity(ej);
                if (entity) scene.entities.push_back(std::move(entity));
            }
            scene.RebuildHierarchy();
        }

        scene.storyState = nextState.contains("storyState")
            ? StoryValueMapFromJson(nextState["storyState"])
            : std::map<std::string, StoryValue>{};

        selectedIndex = -1;
    }

    inline void Editor::ShowContentBrowser()
    {
        if (!pm.IsProjectOpen())
        {
            ImGui::TextDisabled("No project open.");
            return;
        }

        static std::string selectedFolder;
        static std::string lastKnownProjectPath;

        if (lastKnownProjectPath != pm.GetCurrentProjectPath())
        {
            lastKnownProjectPath = pm.GetCurrentProjectPath();
            selectedFolder = pm.GetCurrentProjectPath();
        }

        float availH = ImGui::GetContentRegionAvail().y;
        static float treeWidth = 180.0f;

        // --- Left Side: Folder Tree ---
        ImGui::BeginChild("FolderTree", ImVec2(treeWidth, availH), true, ImGuiWindowFlags_HorizontalScrollbar);

        std::function<void(const fs::path&, bool)> DrawFolderTree;
        DrawFolderTree = [&](const fs::path& path, bool isRoot) {
            std::string name = path.filename().string();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (selectedFolder == path.string()) flags |= ImGuiTreeNodeFlags_Selected;
            if (isRoot) flags |= ImGuiTreeNodeFlags_DefaultOpen;

            bool hasSubdirs = false;
            if (fs::exists(path))
            {
                for (auto& entry : fs::directory_iterator(path))
                {
                    if (entry.is_directory()) { hasSubdirs = true; break; }
                }
            }
            if (!hasSubdirs) flags |= ImGuiTreeNodeFlags_Leaf;

            std::string label = std::string(ICON_FA_FOLDER "  ") + name;
            bool open = ImGui::TreeNodeEx(label.c_str(), flags);
            if (ImGui::IsItemClicked()) selectedFolder = path.string();

            if (open)
            {
                if (fs::exists(path))
                {
                    for (auto& entry : fs::directory_iterator(path))
                    {
                        if (entry.is_directory()) DrawFolderTree(entry.path(), false);
                    }
                }
                ImGui::TreePop();
            }
        };

        DrawFolderTree(pm.GetCurrentProjectPath(), true);
        ImGui::EndChild();

        // --- Resize Handle ---
        ImGui::SameLine();
        ImGui::Button("##treeSplitter", ImVec2(4.0f, availH));
        if (ImGui::IsItemActive())
            treeWidth += ImGui::GetIO().MouseDelta.x;
        treeWidth = std::clamp(treeWidth, 100.0f, 400.0f);
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        ImGui::SameLine();

        // --- Right Side: Content View ---
        ImGui::BeginChild("ContentView", ImVec2(0, availH), false);

        // Navigation bar & Actions
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
        if (ImGui::Button(ICON_FA_PLUS "  Create Script")) ImGui::OpenPopup("NewScriptModal");
        ImGui::PopStyleVar();

        ImGui::SameLine();
        if (selectedFolder != pm.GetCurrentProjectPath())
        {
            if (ImGui::Button(ICON_FA_ARROW_LEFT)) {
                fs::path p(selectedFolder);
                if (p.has_parent_path() && p.parent_path().string() >= pm.GetCurrentProjectPath())
                    selectedFolder = p.parent_path().string();
            }
            ImGui::SameLine();
        }
        ImGui::TextDisabled(ICON_FA_FOLDER_OPEN "  %s", fs::relative(selectedFolder, pm.GetCurrentProjectPath()).string().c_str());
        ImGui::Separator();

        // New Script Modal
        if (ImGui::BeginPopupModal("NewScriptModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static char scriptName[64] = "NewScript";
            ImGui::InputText("Script Name", scriptName, 64);
            ImGui::Spacing();

            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                std::string sName(scriptName);
                std::string projectPath = pm.GetCurrentProjectPath();
                std::string scriptDir = projectPath + "/scripts";

                // Ensure scripts directory exists
                if (!fs::exists(scriptDir)) fs::create_directories(scriptDir);

                std::string filePath = scriptDir + "/" + sName + ".cpp";
                if (!fs::exists(filePath)) {
                    // 1. Create the template script
                    std::ofstream f(filePath);
                    f << "#include \"IndiumEngine.hpp\"\n\n"
                      << "using namespace Indium;\n\n"
                      << "class " << sName << " : public NativeScript {\n"
                      << "public:\n"
                      << "    // IND_PROP(type, name, defaultValue) — exposes a variable to the Inspector.\n"
                      << "    // Do NOT declare the variable separately; IND_PROP does it for you.\n"
                      << "    IND_PROP(float, speed, 200.0f);\n\n"
                      << "    void OnStart() override {\n"
                      << "        // Called once when the game starts (or when this entity is spawned).\n"
                      << "    }\n\n"
                      << "    void OnUpdate(float dt) override {\n"
                      << "        // Called every frame. 'entity' is the owning entity.\n"
                      << "        // Spawn:       auto* c = Spawn<Circle>(\"name\");\n"
                      << "        // Find:        Entity* e = FindByName(\"Player\");\n"
                      << "        // Destroy:     Destroy();  or  Destroy(target);\n"
                      << "        // Components:  auto* rb = GetComponent<RigidbodyComponent>();\n"
                      << "        // Story:       StoryState::Get().SetFlag(\"key\");\n"
                      << "    }\n\n"
                      << "    void OnDraw() const override {\n"
                      << "        // Called every frame for custom drawing (world space).\n"
                      << "        // DrawText(\"Hi\", entity->position.x, entity->position.y, 20, WHITE);\n"
                      << "    }\n\n"
                      << "    void OnDestroy() override {\n"
                      << "        // Called just before this entity is removed from the scene.\n"
                      << "    }\n"
                      << "};\n\n"
                      << "REGISTER_SCRIPT(" << sName << ")\n";
                    f.close();

                    consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO] ", "Created script: " + sName + ".cpp", ICON_FA_CIRCLE_CHECK});
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        // --- Grid: Unity-style compact tiles ---
        float cellSize = 80.0f;
        float itemSpacing = 6.0f;
        float gridWidth = ImGui::GetContentRegionAvail().x;
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        if (fs::exists(selectedFolder))
        {
            int itemIdx = 0;
            for (auto& entry : fs::directory_iterator(selectedFolder))
            {
                auto path = entry.path();
                std::string name = path.filename().string();

                // Flow layout: place items side by side, wrap when needed
                float nextX = (cellSize + itemSpacing) * itemIdx;
                int perRow = (int)(gridWidth / (cellSize + itemSpacing));
                if (perRow < 1) perRow = 1;

                if (itemIdx % perRow != 0)
                    ImGui::SameLine(0.0f, itemSpacing);

                ImGui::PushID(name.c_str());

                bool isDir = entry.is_directory();
                const char* icon = ICON_FA_FILE;
                ImVec4 iconColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                std::string ext = path.extension().string();

                if (isDir)                                { icon = ICON_FA_FOLDER;    iconColor = ImVec4(0.95f, 0.75f, 0.2f, 1.0f); }
                else if (ext == ".cpp" || ext == ".hpp")  { icon = ICON_FA_FILE_CODE; iconColor = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);  }
                else if (ext == ".scene")                 { icon = ICON_FA_MAP;       iconColor = ImVec4(0.4f, 0.8f, 0.4f, 1.0f);  }
                else if (ext == ".png" || ext == ".jpg")  { icon = ICON_FA_IMAGE;     iconColor = ImVec4(0.8f, 0.5f, 0.9f, 1.0f);  }

                // --- Render tile with InvisibleButton (no BeginChild overhead) ---
                ImVec2 tileSize = ImVec2(cellSize, cellSize + 16.0f);
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 p1 = ImVec2(p0.x + tileSize.x, p0.y + tileSize.y);

                ImGui::InvisibleButton("##tile", tileSize);
                bool hovered = ImGui::IsItemHovered();
                bool clicked = ImGui::IsItemClicked();

                // Hover background
                if (hovered)
                    drawList->AddRectFilled(p0, p1, ImColor(1.0f, 1.0f, 1.0f, 0.06f), 4.0f);

                // Icon (centered, large)
                float oldScale = ImGui::GetFont()->Scale;
                ImGui::GetFont()->Scale *= 3.0f;
                ImGui::PushFont(ImGui::GetFont());
                float iconW = ImGui::CalcTextSize(icon).x;
                float iconH = ImGui::CalcTextSize(icon).y;
                ImGui::PopFont();
                ImGui::GetFont()->Scale = oldScale;

                ImVec2 iconPos = ImVec2(
                    p0.x + (cellSize - iconW) * 0.5f,
                    p0.y + (cellSize - iconH) * 0.5f - 4.0f
                );

                // Draw icon directly via DrawList (avoids layout issues)
                ImGui::GetFont()->Scale *= 3.0f;
                ImGui::PushFont(ImGui::GetFont());
                drawList->AddText(iconPos, ImGui::GetColorU32(iconColor), icon);
                ImGui::PopFont();
                ImGui::GetFont()->Scale = oldScale;

                // Text label (centered below icon)
                std::string displayName = name;
                float textW = ImGui::CalcTextSize(name.c_str()).x;
                if (textW > cellSize - 4.0f)
                {
                    for (int len = (int)name.size() - 1; len > 3; len--)
                    {
                        displayName = name.substr(0, len) + "..";
                        if (ImGui::CalcTextSize(displayName.c_str()).x <= cellSize - 4.0f) break;
                    }
                    textW = ImGui::CalcTextSize(displayName.c_str()).x;
                }

                ImVec2 textPos = ImVec2(
                    p0.x + (cellSize - textW) * 0.5f,
                    p1.y - ImGui::GetTextLineHeight() - 2.0f
                );
                drawList->AddText(textPos, ImColor(180, 180, 180), displayName.c_str());

                // Click handler
                if (clicked && isDir) selectedFolder = path.string();

                ImGui::PopID();
                itemIdx++;
            }
        }

        ImGui::EndChild();
    }

    inline void Editor::ShowConsole()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        if (ImGui::Button(ICON_FA_TRASH "  Clear")) { consoleLogs.clear(); }
        ImGui::SameLine();
        ImGui::TextDisabled("   " ICON_FA_TERMINAL "  System Console");
        ImGui::PopStyleVar();

        ImGui::Separator();

        ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& log : consoleLogs)
        {
            ImGui::TextColored(log.color, "%s %s", log.icon, log.level.c_str());
            ImGui::SameLine();
            ImGui::Text("%s", log.message.c_str());
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
    }

    inline void Editor::ShowStoryState()
    {
        if (!pm.IsProjectOpen())
        {
            ImGui::TextDisabled("No project open.");
            return;
        }

        const bool playing = (state == GameState::Play);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        ImGui::TextDisabled("   " ICON_FA_FLAG "  %s",
            playing ? "Runtime Blackboard (live — discarded on Stop)"
                    : "Authored Scene Flags (seeded into the blackboard on Play)");
        ImGui::PopStyleVar();
        ImGui::Separator();

        // Snapshot the entries so the backing store can be mutated while iterating.
        std::vector<std::pair<std::string, StoryValue>> entries;
        if (playing)
            for (const auto& kv : StoryState::Get().Values()) entries.emplace_back(kv.first, kv.second);
        else
            for (const auto& kv : scene.storyState) entries.emplace_back(kv.first, kv.second);

        const float addRowH = ImGui::GetFrameHeightWithSpacing() + 8.0f;
        ImGui::BeginChild("StoryStateScroll", ImVec2(0, -addRowH), false);

        if (entries.empty())
        {
            ImGui::TextDisabled("(No story variables defined)");
        }
        else if (ImGui::BeginTable("StoryStateTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Key",   ImGuiTableColumnFlags_WidthStretch, 0.45f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
            ImGui::TableSetupColumn("##rm",  ImGuiTableColumnFlags_WidthFixed,   26.0f);
            ImGui::TableHeadersRow();

            std::string removeKey;

            for (auto& entry : entries)
            {
                const std::string& key   = entry.first;
                StoryValue&        value = entry.second;

                ImGui::TableNextRow();
                ImGui::PushID(key.c_str());

                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(key.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                bool changed = false;
                std::visit([&](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, bool>)
                    {
                        bool b = arg;
                        if (ImGui::Checkbox("##val", &b)) { value = b; changed = true; }
                    }
                    else if constexpr (std::is_same_v<T, int>)
                    {
                        int n = arg;
                        if (ImGui::DragInt("##val", &n)) { value = n; changed = true; }
                    }
                    else if constexpr (std::is_same_v<T, float>)
                    {
                        float f = arg;
                        if (ImGui::DragFloat("##val", &f, 0.1f)) { value = f; changed = true; }
                    }
                    else if constexpr (std::is_same_v<T, std::string>)
                    {
                        char buf[256] = {};
                        strncpy(buf, arg.c_str(), sizeof(buf) - 1);
                        if (ImGui::InputText("##val", buf, sizeof(buf))) { value = std::string(buf); changed = true; }
                    }
                }, value);
                const bool activated = ImGui::IsItemActivated();
                ImGui::PopItemWidth();

                if (playing)
                {
                    if (changed) StoryState::Get().Set(key, value);
                }
                else
                {
                    if (activated) TakeSnapshot();
                    if (changed)   scene.storyState[key] = value;
                }

                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton(ICON_FA_XMARK)) removeKey = key;

                ImGui::PopID();
            }

            ImGui::EndTable();

            if (!removeKey.empty())
            {
                if (playing) StoryState::Get().Remove(removeKey);
                else { TakeSnapshot(); scene.storyState.erase(removeKey); }
            }
        }

        ImGui::EndChild();

        // --- Add variable row ---
        ImGui::Separator();

        static char newKey[64]  = "";
        static int  newType     = 0; // 0 Flag(bool), 1 Int, 2 Float, 3 Text(string)
        const char* typeNames[] = { "Flag", "Int", "Float", "Text" };

        ImGui::TextDisabled("New:");
        ImGui::SameLine();
        ImGui::PushItemWidth(150.0f);
        ImGui::InputText("##newkey", newKey, sizeof(newKey));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(90.0f);
        ImGui::Combo("##newtype", &newType, typeNames, IM_ARRAYSIZE(typeNames));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Add"))
        {
            const std::string k = newKey;
            if (!k.empty())
            {
                StoryValue v{ false };
                if      (newType == 1) v = 0;
                else if (newType == 2) v = 0.0f;
                else if (newType == 3) v = std::string{};

                if (playing) StoryState::Get().Set(k, v);
                else { TakeSnapshot(); scene.storyState[k] = v; }
                newKey[0] = '\0';
            }
        }
    }
}
