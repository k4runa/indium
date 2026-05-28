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
#include "../2D/component/Collider2D.hpp"
#include "../2D/component/ShapeRendererComponent.hpp"
#include "../2D/component/SpriteRendererComponent.hpp"
#include "../2D/component/AudioSourceComponent.hpp"
#include "../2D/component/TextRendererComponent.hpp"
#include "../2D/component/ParticleSystemComponent.hpp"
#include "../2D/component/TilemapComponent.hpp"
#include "../2D/component/InteractableComponent.hpp"
#include "../2D/component/PlayerInteractorComponent.hpp"
#include "../core/scene/Scene.hpp"
#include "../core/StoryState.hpp"
#include "../core/PrefabManager.hpp"
#include "../2D/entity/EntityFactory.hpp"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <set>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <variant>
#include <type_traits>

#include "Launcher.hpp"
#include "../core/ProjectManager.hpp"
#include "../core/ScriptManager.hpp"
#include "../core/InputManager.hpp"
#include "../core/Screen.hpp"
#include "../core/DialogueManager.hpp"
#include "../include/extras/IconsFontAwesome6.h"

namespace Indium
{
    /**
     * @brief Defines the current operational mode of the engine.
     */
    enum class GameState { Launcher, Editor, Play, Pause };

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

        /** @brief Active viewport tab: 0 = Scene (editor camera), 1 = Game (camera component). */
        int                 viewportTab_ = 0;

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
        nlohmann::json              entityClipboard;
        std::vector<nlohmann::json> multiClipboard_;

        /** @brief Console Log Entry */
        struct LogEntry
        {
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
        float               bottomPanelHeight    = 350.0f;    // 350 by default
        float               bottomPanelMaxHeight = 500.0f;
        bool                showBottomPanel      = true;
        bool                isResizingBottom     = false;

        /** @brief Side panel widths and resize state. */
        float               hierarchyWidth      = 350.0f;
        float               hierarchyMaxWidth   = 600.0f;
        float               inspectorWidth      = 400.0f;
        float               inspectorMaxWidth   = 600.0f;
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

        // --- Multi-selection ---
        std::vector<int>    multiSelection_;        // indices into scene.entities
        Vector2             multiDragStartMouse_  = {0, 0};
        std::vector<Vector2> multiDragStartPos_;   // per-entity world positions at drag start

        // --- Transform tool ---
        enum class TransformTool { Move, Rotate, Rect, Universal };
        TransformTool activeTool_ = TransformTool::Move;

        // --- Grid & snap ---
        bool  showGrid_   = true;
        bool  snapEnabled_ = false;
        float snapSize_    = 32.0f;

        // --- Input Manager panel ---
        bool        showInputManager_    = false;
        bool        capturingKey_        = false;
        std::string capturingAction_;
        char        newActionNameBuf_[64] = {};

        // --- Parallax panel ---
        // Editor-local preview toggle for the Scene tab. The scene-side toggle
        // (parallaxEnabled) is persisted to the .scene file and governs runtime
        // / Game tab behavior; this one only controls whether the Scene tab also
        // renders with parallax (default off so editing stays WYSIWYG).
        bool        parallaxPreviewSceneTab_ = false;
        int         newParallaxLayer_        = -1;     // panel "add new layer" inputs
        float       newParallaxFactor_       = 0.75f;  // = DefaultParallaxFactor(-1)

        // --- Prefab state ---
        char  prefabNameBuf_[128]  = {};
        bool  showSavePrefabModal_ = false;
        int   prefabSourceIndex_   = -1;

        enum class HandleType {
            None, Body,
            H_TL, H_TM, H_TR, H_RM, H_BR, H_BM, H_BL, H_LM,
            H_Rotate
        };
        HandleType activeHandle_              = HandleType::None;
        Vector2    handleDragStartMouse_      = {0, 0};
        Vector2    handleDragStartGlobalPos_  = {0, 0};
        Vector2    handleDragStartScale_      = {0, 0};
        float      handleDragStartRot_        = 0.0f;

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

        /** @brief Renders the per-depthLayer parallax configuration panel. */
        void ShowParallax();

        /** @brief Renders the Input Action Mapping configuration window. */
        void ShowInputManager();

        /** @brief Removes an entity from the scene and resets the selection. */
        void DeleteEntity(Entity& entity);

        /** @brief Deletes several entities given indices into scene.entities.
         *  Safe against DeleteEntity's child cascade: indices are resolved to stable
         *  pointers before any erase, and each is deleted only if it still exists. */
        void DeleteEntitiesAt(const std::vector<int>& indices);

        // --- Undo / Redo System ---
        void TakeSnapshot();
        void Undo();
        void Redo();

        // --- Entity Clipboard Actions ---
        void CopySelected();
        void PasteAt(Vector2 pos);
        void DuplicateSelected(int index);

    private:
        /** @brief Converts Raylib mouse coordinates to the ImGui coordinate space used by editor panels. */
        Vector2 GetRaylibToImGuiScale() const
        {
            Vector2 scale = { 1, 1 };
#if defined(__APPLE__)
        /** APPLE logical resoltion is different from retina resolution */
            Vector2 dpiScale = GetWindowScaleDPI();

            if (dpiScale.x > 0.0f && GetRenderWidth() == GetScreenWidth())   scale.x = dpiScale.x;
            if (dpiScale.y > 0.0f && GetRenderHeight() == GetScreenHeight()) scale.y = dpiScale.y;
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

        /** @brief Returns the primary CameraComponent's camera, or editorCamera if none exists. */
        Camera2D GetGameCamera() const
        {
            for (const auto& e : scene.entities)
            {
                for (const auto& c : e->components)
                {
                    if (const auto* camComp = dynamic_cast<const CameraComponent*>(c.get()))
                    {
                        if (camComp->isPrimary)
                        {
                            Camera2D cam  = editorCamera;
                            cam.target    = e->getGlobalPosition();
                            cam.offset    = (Vector2){ viewportSize.x / 2.0f + camComp->GetShakeOffset().x, viewportSize.y / 2.0f + camComp->GetShakeOffset().y };
                            cam.zoom      = camComp->zoom;
                            cam.rotation  = camComp->GetEffectiveRotation() + camComp->GetShakeAngle();
                            return cam;
                        }
                    }
                }
            }
            return editorCamera;
        }

        /**
         * @brief Returns the camera for the current active viewport tab.
         * Scene tab → editorCamera. Game tab / Play / Pause → game camera.
         */
        Camera2D GetActiveCamera() const
        {
            if (viewportTab_ == 1 || state == GameState::Play || state == GameState::Pause) return GetGameCamera();
            return editorCamera;
        }

        static bool CtrlDown() { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

        void CreateEntityAt(const std::string& type, Vector2 pos)
        {
            TakeSnapshot();
            std::unique_ptr<Entity> e;
            if      (type == "Circle")    e = factory.CreateCircle(scene);
            else if (type == "Rectangle") e = factory.CreateRectangle(scene);
            else if (type == "Surface")   e = factory.CreatePlane(scene);
            else if (type == "Sprite")    e = factory.CreateSprite(scene);
            else if (type == "Camera")    e = factory.CreateCamera(scene);
            if (e) { e->position = pos; scene.entities.push_back(std::move(e)); }
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
        if (!WindowShouldClose()) return false;
        if (!isDirty) return true;

        // Intercept the OS window-close event: clear GLFW's internal
        // shouldClose flag so the window stays open while the
        // "Save Changes?" popup is shown.
        //
        // raylib 5.5 embeds GLFW 3.4 statically and does not export
        // glfwSetWindowShouldClose from its dylib, so we patch the flag
        // directly.
        //
        // _GLFWwindow layout (GLFW 3.4, 64-bit):
        //   +0  next*              8 B
        //   +8  resizable          4 B  (GLFWbool = int)
        //   +12 decorated          4 B
        //   +16 autoIconify        4 B
        //   +20 floating           4 B
        //   +24 focusOnShow        4 B
        //   +28 mousePassthrough   4 B  (added in GLFW 3.4)
        //   +32 shouldClose        4 B  <- cleared here
        //
        // Re-verify this offset if GLFW or raylib is upgraded.
        static_assert(sizeof(void*) == 8, "GLFW shouldClose is at offset 32 only on 64-bit builds");

        if (void* handle = GetWindowHandle())
        {
            int* baseFlagsPtr = reinterpret_cast<int*>(static_cast<char*>(handle) + 8);

            // All six fields at +8..+28 are GLFWbool (0 or 1). shouldClose at
            // +32 must currently be non-zero because WindowShouldClose() just
            // returned true. If the struct layout has shifted, the chance that
            // all six neighbors coincidentally hold 0 or 1 is vanishingly small.
            auto isGLFWBool = [](int v) { return v == 0 || v == 1; };

            const bool layoutLooksValid =
                baseFlagsPtr[6] != 0 &&         // shouldClose       (+32)
                isGLFWBool(baseFlagsPtr[0]) &&  // resizable         (+8)
                isGLFWBool(baseFlagsPtr[1]) &&  // decorated         (+12)
                isGLFWBool(baseFlagsPtr[2]) &&  // autoIconify       (+16)
                isGLFWBool(baseFlagsPtr[3]) &&  // floating          (+20)
                isGLFWBool(baseFlagsPtr[4]) &&  // focusOnShow       (+24)
                isGLFWBool(baseFlagsPtr[5]);    // mousePassthrough  (+28)

            if (layoutLooksValid) { baseFlagsPtr[6] = 0; }
            else { TraceLog(LOG_WARNING, "EDITOR: GLFW struct layout mismatch detected. Refusing to patch."); }
        }

        showUnsavedChangesPopup = true;
        wantsToExit = true;
        return false;
    }

    static std::string PrefsPath()
    {
        const char* home = std::getenv("HOME");
        return std::string(home ? home : ".") + "/.indium_prefs.json";
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

        // Load persisted theme preference
        {
            std::ifstream pf(PrefsPath());
            if (pf.is_open())
            {
                try
                {
                    nlohmann::json prefs = nlohmann::json::parse(pf);
                    if (prefs.contains("theme")) THEME_STYLE = prefs["theme"].get<std::string>();
                } catch (...) {}
            }
        }

        ApplyTheme(THEME_STYLE);
        launcher = std::make_unique<Launcher>(&pm);

        // Touch the story blackboard so it subscribes to NarrativeEvent before Play.
        StoryState::Get();

        s_consoleLogs = &consoleLogs;
        SetTraceLogCallback(RaylibTraceCallback);

        NativeScript::s_prefabLoader = [this](const std::string& name, Scene* scene) -> Entity*
        {
            if (!pm.IsProjectOpen()) return nullptr;
            std::string path = pm.GetCurrentProjectPath() + "/prefabs/" + name + ".prefab";
            nlohmann::json j = PrefabManager::Load(path);
            if (j.is_null()) return nullptr;
            auto e = factory.LoadEntity(j);
            if (!e) return nullptr;
            e->id = scene->nextEntityId++;
            return scene->Instantiate(std::move(e));
        };

        // Initial Logs
        consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO] ", "Indium Engine v0.1 initialized.", ICON_FA_CIRCLE_INFO});
        consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO] ", "Scripting System: Hot-reload ready.", ICON_FA_CIRCLE_INFO});

        // should show bottom panel?
        showBottomPanel = config.showBottomPanel;
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
        if (s_consoleLogs->size() > 2000) s_consoleLogs->erase(s_consoleLogs->begin(), s_consoleLogs->begin() + 500);
    }

    inline void Editor::Update(float dt)
    {
        if (state == GameState::Editor && autoSaveEnabled && pm.IsProjectOpen())
        {
            autoSaveTimer += dt;
            if (autoSaveTimer >= autoSaveInterval)
            {
                if (isDirty) { pm.SaveCurrentProject(scene); isDirty = false; }
                autoSaveTimer = 0.0f;
            }
        }

        // Update logic
        Vector2 screenMouse = GetImGuiSpaceMousePosition();

        Camera2D activeCamera = GetActiveCamera();

        float scaleX = (viewportSize.x > 0) ? (float)viewport.texture.width  / viewportSize.x : 1.0f;
        float scaleY = (viewportSize.y > 0) ? (float)viewport.texture.height / viewportSize.y : 1.0f;

        Vector2 scaledMouse = {
            (screenMouse.x - viewportPos.x) * scaleX,
            (screenMouse.y - viewportPos.y) * scaleY
        };

        worldMouse = GetScreenToWorld2D(scaledMouse, activeCamera);

        // Publish viewport-space input for runtime UI (OnGUI). The one-frame lag on
        // the viewport rect mirrors worldMouse above and is fine for UI hit-testing.
        {
            bool guiPressed = viewportHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            bool guiDown    = viewportHovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
            Screen::Get().Set(viewport.texture.width, viewport.texture.height, scaledMouse, guiPressed, guiDown);
        }

        if (state == GameState::Play)
        {
            // Inject current viewport pixel size so CameraComponent bounds clamp is viewport-aware
            for (auto& e : scene.entities)
            {
                for (auto& c : e->components) {if (auto* cam = dynamic_cast<CameraComponent*>(c.get())) cam->viewportPx_ = {viewportSize.x, viewportSize.y};}
            }

            scene.Update(dt);

            // Drain script-requested scene transitions (NativeScript::LoadScene → scene._pendingSceneLoad).
            if (!scene._pendingSceneLoad.empty())
            {
                std::string target = scene._pendingSceneLoad;
                scene._pendingSceneLoad.clear();
                if (pm.SwitchScene(scene, target))
                {
                    selectedIndex = -1;
                    undoStack.clear();
                    redoStack.clear();
                }
            }
        }
        // Pause: no scene update, rendering continues

        // Keep editor camera synced to scene so SaveCurrentProject always captures the latest view.
        if (state != GameState::Play && state != GameState::Pause)
        {
            scene.editorCameraTarget = editorCamera.target;
            scene.editorCameraZoom   = editorCamera.zoom;
        }
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
        if (viewportSize.x > 0 && viewportSize.y > 0 && (viewportSize.x != (float)viewport.texture.width || viewportSize.y != (float)viewport.texture.height))
        {
            UnloadRenderTexture(viewport);
            viewport = LoadRenderTexture((int)viewportSize.x, (int)viewportSize.y);
        }

        /** @brief Step 1: Render the Game World into the off-screen buffer */
        BeginTextureMode(viewport);

            // Scene tab = dark grey background + editor overlays.
            // Game tab / Play / Pause = black (game background, no editor chrome).
            const bool isSceneTab = (viewportTab_ == 0 && state == GameState::Editor);
            ClearBackground(isSceneTab ? Color{ 20, 20, 20, 255 } : Color{ 0, 0, 0, 255 });

            Camera2D activeCamera = GetActiveCamera();

            // --- Screen-Space Editor Grid (Dynamic LOD with Fading) ---
            if (isSceneTab && showGrid_)
            {
                float zoom = activeCamera.zoom;

                // 1. Determine the main grid step dynamically based on zoom
                // Keeps screen spacing between grid lines always within [30px, 150px]
                float gridStep = 50.0f;
                float minSpacingPx = 30.0f;
                while (gridStep * zoom < minSpacingPx) gridStep *= 5.0f;
                float maxSpacingPx = 150.0f;
                while (gridStep * zoom > maxSpacingPx) gridStep /= 5.0f;

                // 2. Calculate the fade alpha for the sub-grid (1 level below main grid)
                float screenSpacing = gridStep * zoom;
                float subScreenSpacing = screenSpacing / 5.0f;
                float subGridAlpha = 0.0f;
                if (subScreenSpacing > 10.0f)
                {
                    subGridAlpha = (subScreenSpacing - 10.0f) / 20.0f;
                    if (subGridAlpha > 1.0f) subGridAlpha = 1.0f;
                }

                // Base colors for main grid lines
                unsigned char mainMinorAlpha = 35; // Soften lines slightly for ultra-premium dark theme
                unsigned char mainMajorAlpha = 65;

                // 3. Compute visible world bounds
                float halfW       = (viewport.texture.width  * 0.5f) / zoom;
                float halfH       = (viewport.texture.height * 0.5f) / zoom;
                float worldLeft   = activeCamera.target.x - halfW;
                float worldRight  = activeCamera.target.x + halfW;
                float worldTop    = activeCamera.target.y - halfH;
                float worldBottom = activeCamera.target.y + halfH;

                // --- Draw Sub-Grid Lines first (so they render behind main grid) ---
                if (subGridAlpha > 0.0f)
                {
                    float subGridStep = gridStep / 5.0f;
                    float startX = floorf(worldLeft  / subGridStep) * subGridStep;
                    float startY = floorf(worldTop   / subGridStep) * subGridStep;

                    Color subColor = Color{ 40, 40, 40, (unsigned char)(mainMinorAlpha * subGridAlpha) };

                    // Vertical sub-grid
                    for (float x = startX; x < worldRight + subGridStep; x += subGridStep)
                    {
                        // Skip if it overlaps with a main grid line
                        float mainRemainder = x / gridStep;
                        if (std::abs(mainRemainder - roundf(mainRemainder)) < 0.01f) continue;
                        Vector2 screenPos = GetWorldToScreen2D({ x, 0 }, activeCamera);
                        int sx = (int)roundf(screenPos.x);
                        DrawLine(sx, 0, sx, viewport.texture.height, subColor);
                    }

                    // Horizontal sub-grid
                    for (float y = startY; y < worldBottom + subGridStep; y += subGridStep)
                    {
                        // Skip if it overlaps with a main grid line
                        float mainRemainder = y / gridStep;
                        if (std::abs(mainRemainder - roundf(mainRemainder)) < 0.01f) continue;
                        Vector2 screenPos = GetWorldToScreen2D({ 0, y }, activeCamera);
                        int sy = (int)roundf(screenPos.y);
                        DrawLine(0, sy, viewport.texture.width, sy, subColor);
                    }
                }

                // --- Draw Main Grid Lines ---
                float startX = floorf(worldLeft  / gridStep) * gridStep;
                float startY = floorf(worldTop   / gridStep) * gridStep;

                // Vertical main grid
                for (float x = startX; x < worldRight + gridStep; x += gridStep)
                {
                    float majorRemainder = x / (gridStep * 5.0f);
                    bool major = (std::abs(majorRemainder - roundf(majorRemainder)) < 0.01f);
                    Color col = Color{ 45, 45, 45, major ? mainMajorAlpha : mainMinorAlpha };

                    Vector2 screenPos = GetWorldToScreen2D({ x, 0 }, activeCamera);
                    int sx = (int)roundf(screenPos.x);
                    DrawLine(sx, 0, sx, viewport.texture.height, col);
                }

                // Horizontal main grid
                for (float y = startY; y < worldBottom + gridStep; y += gridStep)
                {
                    float majorRemainder = y / (gridStep * 5.0f);
                    bool major = (std::abs(majorRemainder - roundf(majorRemainder)) < 0.01f);
                    Color col = Color{ 45, 45, 45, major ? mainMajorAlpha : mainMinorAlpha };

                    Vector2 screenPos = GetWorldToScreen2D({ 0, y }, activeCamera);
                    int sy = (int)roundf(screenPos.y);
                    DrawLine(0, sy, viewport.texture.width, sy, col);
                }
            }

            // --- Check for primary game camera (used for both rendering gate and overlay) ---
            bool hasGameCamera = false;
            for (const auto& e : scene.entities)
            {
                for (const auto& c : e->components)
                {
                    if (auto* cam = dynamic_cast<const CameraComponent*>(c.get()); cam && cam->isPrimary)
                    {
                        hasGameCamera = true;
                        break;
                    }
                }
                if (hasGameCamera) break;
            }

            // Only render the scene if: Scene tab, or there's a game camera, or simulation is running
            const bool renderScene = (viewportTab_ == 0) || hasGameCamera || (state != GameState::Editor);
            if (renderScene)
            {
                // Parallax: always on in Play/Pause/Game tab; in Scene tab only when
                // the user explicitly opts into preview (default off so dragging in
                // the Scene tab stays WYSIWYG with world coords).
                const bool applyParallax = !isSceneTab || parallaxPreviewSceneTab_;

                // Scene::Draw owns its own BeginMode2D scopes (one per depthLayer when
                // parallax is active, or a single pass otherwise).
                scene.Draw(activeCamera, applyParallax);

                // Per-entity overlays need to follow the visually-shifted entity when
                // parallax is on. layerCamFor() scales the camera target the same way
                // Scene::Draw does, so an outline drawn for an entity on layer L sits
                // exactly over that entity in the viewport.
                auto layerCamFor = [&](int depthLayer) {
                    Camera2D c = activeCamera;
                    if (applyParallax && scene.parallaxEnabled)
                        c.target = scene.ApplyParallax(activeCamera.target, depthLayer);
                    return c;
                };

                // Each overlay opens its own BeginMode2D scope so per-entity gizmos
                // can use layerCamFor(depthLayer) and sit visually over their entity
                // when parallax preview is on. Overlays that aren't tied to a specific
                // entity (the marquee box) use activeCamera directly.

                // --- Selection Outline (world-space, layer-aware) ---
                if (isSceneTab)
                {
                    if (selectedIndex >= 0 && selectedIndex < (int)scene.entities.size())
                    {
                        Entity* sel = scene.entities[selectedIndex].get();
                        if (sel)
                        {
                            BeginMode2D(layerCamFor(sel->depthLayer));
                            const Color outlineColor = Color{ 0, 255, 255, 255 };
                            const float thickness    = activeCamera.zoom <= 0.5 ? 4.0f : 2.0f;
                            const float thicknessC   = activeCamera.zoom <= 0.5 ? 6.0f : 3.0f;
                            auto* cCol               = sel->getComponent<CircleCollider2D>();

                            if (cCol) {DrawCircleLinesV(sel->getGlobalPosition(), cCol->radius + thicknessC, outlineColor);}
                            else
                            {
                                std::vector<Vector2> verts = sel->getVertices();
                                if (!verts.empty()) {for (size_t i = 0; i < verts.size(); i++) DrawLineEx(verts[i], verts[(i + 1) % verts.size()], thickness, outlineColor); }
                                else {DrawRectangleLinesEx(sel->getBounds(), thickness, outlineColor);}
                            }
                            EndMode2D();
                        }
                    }

                    // Multi-selection outlines (blue-cyan, distinct from primary)
                    const Color multiOutlineColor = Color{ 0, 180, 255, 200 };
                    const float mThick = activeCamera.zoom <= 0.5f ? 2.0f : 1.0f;
                    for (int mIdx : multiSelection_)
                    {
                        if (mIdx == selectedIndex) continue;
                        if (mIdx < 0 || mIdx >= (int)scene.entities.size()) continue;
                        Entity* me = scene.entities[mIdx].get();
                        if (!me) continue;
                        BeginMode2D(layerCamFor(me->depthLayer));
                        auto* mCol = me->getComponent<CircleCollider2D>();
                        if (mCol)  DrawCircleLinesV(me->getGlobalPosition(), mCol->radius + mThick, multiOutlineColor);
                        else
                        {
                            std::vector<Vector2> mv = me->getVertices();
                            if (!mv.empty()) {for (size_t k = 0; k < mv.size(); k++) {DrawLineEx(mv[k], mv[(k+1) % mv.size()], mThick, multiOutlineColor);}}
                            else {DrawRectangleLinesEx(me->getBounds(), mThick, multiOutlineColor);}
                        }
                        EndMode2D();
                    }
                }

                // --- Marquee / Box Selection Outline (world-space, no parallax —
                //     mouse-to-world conversion is anchored to activeCamera) ---
                if (isSceneTab && isSelectingBox)
                {
                    BeginMode2D(activeCamera);
                    Vector2 p1 = selectBoxStart;
                    Vector2 p2 = worldMouse;
                    float   x  = std::min(p1.x, p2.x);
                    float   y  = std::min(p1.y, p2.y);
                    float   w  = std::abs(p1.x - p2.x);
                    float   h  = std::abs(p1.y - p2.y);

                    DrawRectangleRec(::Rectangle{ x, y, w, h }, Color{ 0, 120, 255, 40 });
                    DrawRectangleLinesEx(::Rectangle{ x, y, w, h }, 2.0f, Color{ 0, 120, 255, 200 });
                    EndMode2D();
                }

                // --- Camera Gizmos (Scene tab only, world-space, layer-aware) ---
                if (isSceneTab)
                {
                    for (const auto& ent : scene.entities)
                    {
                        for (const auto& comp : ent->components)
                        {
                            auto* cam = dynamic_cast<CameraComponent*>(comp.get());
                            if (!cam) continue;

                            BeginMode2D(layerCamFor(ent->depthLayer));
                            Vector2 pos   = ent->getGlobalPosition();
                            float halfW   = (viewport.texture.width  * 0.5f) / cam->zoom;
                            float halfH   = (viewport.texture.height * 0.5f) / cam->zoom;
                            float angle   = cam->baseRotation * DEG2RAD;
                            float cosA    = cosf(angle);
                            float sinA    = sinf(angle);

                            auto rotPt = [&](float lx, float ly) -> Vector2 {return { pos.x + lx * cosA - ly * sinA, pos.y + lx * sinA + ly * cosA };};

                            Vector2 corners[4] = {
                                rotPt(-halfW, -halfH),
                                rotPt( halfW, -halfH),
                                rotPt( halfW,  halfH),
                                rotPt(-halfW,  halfH)
                            };

                            Color gizmoColor = cam->isPrimary ? Color{255, 220, 0, 220}: Color{180, 180, 255, 160};
                            for (int k = 0; k < 4; k++) DrawLineEx(corners[k], corners[(k + 1) % 4], 1.5f, gizmoColor);
                            // Corner lines from center to frustum corners
                            for (int k = 0; k < 4; k++) DrawLineEx(pos, corners[k], 1.0f, Color{gizmoColor.r, gizmoColor.g, gizmoColor.b, 80});
                            // Crosshair at camera position
                            float ch = 12.0f / editorCamera.zoom;
                            DrawLineEx({pos.x - ch, pos.y}, {pos.x + ch, pos.y}, 1.5f, gizmoColor);
                            DrawLineEx({pos.x, pos.y - ch}, {pos.x, pos.y + ch}, 1.5f, gizmoColor);

                            // Small camera body icon
                            float bw = 14.0f / editorCamera.zoom;
                            float bh =  9.0f / editorCamera.zoom;
                            DrawRectangleLinesEx(::Rectangle{pos.x - bw, pos.y - bh, bw * 2, bh * 2}, 1.5f, gizmoColor);
                            // Lens triangle on right side
                            float lx = pos.x + bw;
                            float lt = 5.0f / editorCamera.zoom;
                            DrawTriangleLines({lx, pos.y - lt}, {lx + lt * 1.4f, pos.y}, {lx, pos.y + lt}, gizmoColor);
                            EndMode2D();
                        }
                    }
                }

                // --- Transform Handle Gizmos (layer-aware) ---
                if (selectedIndex >= 0 && selectedIndex < (int)scene.entities.size()
                    && activeTool_ != TransformTool::Move)
                {
                    Entity* sel    = scene.entities[selectedIndex].get();
                    BeginMode2D(layerCamFor(sel->depthLayer));
                    Vector2 center = sel->getGlobalPosition();
                    float   rot    = sel->getGlobalRotation();
                    float   hw     = sel->scale.x / 2.0f;
                    float   hh     = sel->scale.y / 2.0f;
                    float   HR     = 7.0f / editorCamera.zoom;

                    auto toWorld = [&](float lx, float ly) -> Vector2
                    {
                        float rad   = rot * DEG2RAD;
                        float c     = cosf(rad), s = sinf(rad);
                        return {center.x + lx*c - ly*s, center.y + lx*s + ly*c};
                    };
                    auto drawHandle = [&](Vector2 pos, Color inner)
                    {
                        DrawCircleV(pos, HR, inner);
                        DrawCircleLinesV(pos, HR + 1.0f / editorCamera.zoom, Color{255,255,255,200});
                    };

                    bool showRect = (activeTool_ == TransformTool::Rect || activeTool_ == TransformTool::Universal)&& !sel->getVertices().empty();
                    bool showRot  = (activeTool_ == TransformTool::Rotate || activeTool_ == TransformTool::Universal);

                    if (showRect)
                    {
                        bool parented = sel->parentId != -1;
                        Color outlineCol = parented ? Color{80, 80, 80, 120}   : Color{100, 180, 255, 160};
                        Color handleCol  = parented ? Color{90, 90, 90, 160}   : Color{50, 140, 255, 255};
                        std::vector<Vector2> verts = sel->getVertices();
                        for (int k = 0; k < 4; k++) DrawLineEx(verts[k], verts[(k+1)%4], 1.0f / editorCamera.zoom, outlineCol);
                        Vector2 hpts[8] =
                        {
                            toWorld(-hw,-hh), toWorld(0,-hh), toWorld(+hw,-hh), toWorld(+hw,0),
                            toWorld(+hw,+hh), toWorld(0,+hh), toWorld(-hw,+hh), toWorld(-hw,0)
                        };
                        for (auto& hp : hpts) drawHandle(hp, handleCol);
                    }

                    if (showRot)
                    {
                        float gizmoR = fmaxf(hw, hh) + 28.0f / editorCamera.zoom;
                        DrawCircleLinesV(center, gizmoR, Color{255, 200, 60, 180});
                        Vector2 rotHPt = toWorld(0.0f, -gizmoR);
                        drawHandle(rotHPt, Color{255, 190, 30, 255});
                        DrawLineEx(center, rotHPt, 1.0f / editorCamera.zoom, Color{255,190,30,120});
                    }
                    EndMode2D();
                }
            }

            // --- Runtime screen-space UI — Play/Pause only, drawn over the world.
            //     Every component gets onGUI(); NativeScript routes it to the user OnGUI() hook,
            //     and the active dialogue box is drawn on top by the DialogueManager. ---
            if (state == GameState::Play || state == GameState::Pause)
            {
                for (auto& e : scene.entities)
                {
                    if (!e->activeInHierarchy()) continue;
                    for (auto& c : e->components)
                        if (c->enabled) c->onGUI(&scene);
                }
                DialogueManager::Get().DrawGUI(state == GameState::Play);
            }

            // --- Entity name labels (screen-space — after EndMode2D for constant pixel size) ---
            if (isSceneTab)
            {
                const int fontSize = 10;
                for (int i = 0; i < (int)scene.entities.size(); i++)
                {
                    Entity* e = scene.entities[i].get();
                    if (!e) continue;

                    ::Rectangle bounds = e->getBounds();
                    Vector2 worldPos   = { bounds.x + bounds.width * 0.5f, bounds.y + bounds.height };
                    Vector2 screenPos  = GetWorldToScreen2D(worldPos, activeCamera);
                    screenPos.y       += 4.0f;

                    float tw  = (float)MeasureText(e->name.c_str(), fontSize);
                    Color col = (selectedIndex == i) ? Color{ 0, 255, 255, 255 } : Color{ 255, 255, 255, 150 };
                    DrawText(e->name.c_str(), (int)(screenPos.x - tw * 0.5f), (int)screenPos.y, fontSize, col);
                }

                // Camera name labels (shown above the gizmo)
                for (const auto& ent : scene.entities)
                {
                    for (const auto& comp : ent->components)
                    {
                        if (!dynamic_cast<CameraComponent*>(comp.get())) continue;
                        ::Rectangle bounds = ent->getBounds();
                        Vector2 worldPos   = { bounds.x + bounds.width * 0.5f, bounds.y - 4.0f };
                        Vector2 screenPos  = GetWorldToScreen2D(worldPos, activeCamera);
                        screenPos.y       -= 14.0f;
                        const char* label  = ent->name.c_str();
                        float tw           = (float)MeasureText(label, fontSize);
                        DrawText(label, (int)(screenPos.x - tw * 0.5f), (int)screenPos.y, fontSize, Color{255, 220, 0, 220});
                        break;
                    }
                }
            }

            // --- Game tab: "No Camera" overlay ---
            if (viewportTab_ == 1 && !hasGameCamera)
            {
                const char* msg = "No Camera in scene";
                int fw = MeasureText(msg, 20);
                int fx = (viewport.texture.width  - fw) / 2;
                int fy = (viewport.texture.height - 20) / 2;
                DrawRectangle(fx - 12, fy - 8, fw + 24, 36, Color{ 0, 0, 0, 200 });
                DrawText(msg, fx, fy, 20, Color{ 200, 200, 200, 255 });
            }

        EndTextureMode();

        /** @brief Step 2: Render the Editor UI to the main window */
        BeginDrawing();
            ClearBackground(DARKGRAY);
            rlImGuiBegin();

            // --- Unsaved Changes Modal Popup ---
            if (showUnsavedChangesPopup) ImGui::OpenPopup("Save Changes?");
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
                    state                 = GameState::Editor;
                    undoStack.clear();
                    redoStack.clear();
                    selectedIndex         = -1;
                    isDirty               = false;
                    editorCamera.target   = scene.editorCameraTarget;
                    editorCamera.zoom     = scene.editorCameraZoom;
                    editorCamera.offset   = { 0, 0 };
                    editorCamera.rotation = 0.0f;
                    InputManager::Get().Load(pm.GetCurrentProjectPath() + "/input.json");
                    DialogueManager::Get().SetProjectPath(pm.GetCurrentProjectPath());
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
                    // Check if mouse is near the top edge of the bottom panel (only if not dragging any entity)
                    if (draggingEntity == nullptr && !isSelectingBox && ImGui::GetIO().MousePos.y > currentTopY - 5.0f && ImGui::GetIO().MousePos.y < currentTopY + 5.0f && ImGui::GetIO().MousePos.x < screenW)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                        if (ImGui::IsMouseDown(0)) isResizingBottom = true;
                    }

                    if (isResizingBottom)
                    {
                        if (ImGui::IsMouseDown(0))
                        {
                            if (draggingEntity == nullptr && !isSelectingBox && multiSelection_.size() <= 0)
                            {
                                bottomPanelHeight = screenH - ImGui::GetIO().MousePos.y;
                                if (bottomPanelHeight < 100.0f) bottomPanelHeight = 100.0f;                             /* minimum bottom panel height (300.0f)*/
                                if (bottomPanelHeight > bottomPanelMaxHeight) bottomPanelHeight = bottomPanelMaxHeight; /* maximum bottom panel height (500.0f)*/
                                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                            }
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

                    auto sceneNameOk = [](const char* s)
                    {
                        std::string n(s);
                        if (n.empty() || n.find_first_not_of(" \t") == std::string::npos) return false;
                        if (n.find('/') != std::string::npos || n.find('\\') != std::string::npos) return false;
                        if (n.find("..") != std::string::npos) return false;
                        return true;
                    };
                    bool snOk = sceneNameOk(newSceneName);
                    if (!snOk)ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION "  Invalid name.");

                    ImGui::Spacing();
                    if (!snOk) ImGui::BeginDisabled();
                    if (ImGui::Button("Create", ImVec2(126, 0)))
                    {
                        pm.SaveCurrentProject(scene);
                        isDirty = false;
                        if (pm.CreateNewScene(newSceneName, scene))
                        {
                            selectedIndex = -1;
                            undoStack.clear();
                            redoStack.clear();
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    if (!snOk) ImGui::EndDisabled();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(126, 0))) ImGui::CloseCurrentPopup();
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
                        if (sceneRenameBuffer[0] != '\0') pm.RenameScene(sceneRenameTarget, sceneRenameBuffer);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(126, 0))){ImGui::CloseCurrentPopup();}
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
                    if (ImGui::Button("Cancel", ImVec2(126, 0))) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }

                // Adjust heights of side panels to leave room for bottom panel
                float mainAreaH = screenH - menuBarH - bottomH;

                // --- Side Panel Resize Logic (Pre-Layout) ---
                {
                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    bool inPanelRows = mousePos.y > menuBarH && mousePos.y < menuBarH + mainAreaH;

                    // Hierarchy: drag its right edge
                    if (draggingEntity == nullptr && !isSelectingBox&& inPanelRows && mousePos.x > hierarchyWidth - 5.0f && mousePos.x < hierarchyWidth + 5.0f)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        if (ImGui::IsMouseClicked(0)) isResizingHierarchy = true;
                    }
                    if (isResizingHierarchy)
                    {
                        if(ImGui::IsMouseDown(0))
                        {
                            if (draggingEntity == nullptr && !isSelectingBox)
                            {
                                hierarchyWidth = mousePos.x;
                                if(hierarchyWidth >= hierarchyMaxWidth){hierarchyWidth = hierarchyMaxWidth;}
                                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                            }
                        }
                        else {isResizingHierarchy = false;}
                    }

                    // Inspector: drag its left edge
                    float inspectorEdgeX = screenW - inspectorWidth;
                    if (draggingEntity == nullptr && !isSelectingBox && inPanelRows && mousePos.x > inspectorEdgeX - 5.0f && mousePos.x < inspectorEdgeX + 5.0f)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        if (ImGui::IsMouseClicked(0)) isResizingInspector = true;
                    }
                    if (isResizingInspector)
                    {
                        if(ImGui::IsMouseDown(0))
                        {
                            if (draggingEntity == nullptr && !isSelectingBox)
                            {
                                inspectorWidth = screenW - mousePos.x;
                                if(inspectorWidth >= inspectorMaxWidth) { inspectorWidth = inspectorMaxWidth; }
                                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                            }
                        }
                        else { isResizingInspector = false; }
                    }

                    // Clamp so neither panel collapses and they don't overlap each other
                    const float minPanelW = 200.0f;
                    hierarchyWidth = std::clamp(hierarchyWidth, minPanelW, std::max(minPanelW, screenW - inspectorWidth));
                    inspectorWidth = std::clamp(inspectorWidth, minPanelW, std::max(minPanelW, screenW - hierarchyWidth));
                }

                // Viewport — full width and height, rendered first so panels can overlay on top
                ImGui::SetNextWindowPos(ImVec2(0, menuBarH));
                ImGui::SetNextWindowSize(ImVec2(screenW, screenH - menuBarH));
                ShowViewport();

                // Hierarchy — overlays the left side of the viewport
                ImGui::SetNextWindowPos(ImVec2(0, menuBarH));
                ImGui::SetNextWindowSize(ImVec2(hierarchyWidth, mainAreaH));
                ShowHierarchy();

                // Inspector — overlays the right side of the viewport
                ImGui::SetNextWindowPos(ImVec2(screenW - inspectorWidth, menuBarH));
                ImGui::SetNextWindowSize(ImVec2(inspectorWidth, mainAreaH));
                ShowInspector();

                // Bottom Panel (Content Browser & Console) — overlays viewport bottom
                if (showBottomPanel)
                {
                    ImGui::SetNextWindowPos(ImVec2(0, screenH - bottomH));
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
                            if (ImGui::BeginTabItem(ICON_FA_LAYER_GROUP "  Parallax"))
                            {
                                ShowParallax();
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

                if (showInputManager_) ShowInputManager();

                if (showProjectSettings)
                {
                    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.5f),ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

                    if (ImGui::Begin(ICON_FA_GEAR "  Project Settings", &showProjectSettings,ImGuiWindowFlags_NoCollapse))
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
                            if (ImGui::InputText("##ProjName", projNameBuf, sizeof(projNameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) { pm.SetProjectName(projNameBuf); }
                            ImGui::PopItemWidth();
                            ImGui::Spacing();
                            ImGui::Text("Project Path");
                            ImGui::PushItemWidth(-1);
                            {
                                char pathBuf[512] = {};
                                strncpy(pathBuf, pm.GetCurrentProjectPath().c_str(), sizeof(pathBuf) - 1);
                                ImGui::InputText("##ProjPath", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_ReadOnly);
                            }
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
                            if (ImGui::BeginCombo("##DefaultScene", defaultScene.empty() ? "(none)" : fs::path(defaultScene).stem().string().c_str()))
                            {
                                for (const auto& sf : sceneList)
                                {
                                    const std::string stem = fs::path(sf).stem().string();
                                    bool selected = (sf == defaultScene);
                                    if (ImGui::Selectable(stem.c_str(), selected)) pm.SetDefaultScene(sf);
                                    if (selected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            ImGui::PopItemWidth();
                            ImGui::Spacing();
                            ImGui::Text("World Size (px)");
                            ImGui::PushItemWidth(-1);

                            int wSize[2]          = { (int)scene.worldSize.x, (int)scene.worldSize.y };
                            bool worldSizeChanged = ImGui::DragInt2("##WorldSize", wSize, 1.0f, 64, 16384);
                            if (ImGui::IsItemActivated()) TakeSnapshot();
                            if (worldSizeChanged) { scene.worldSize.x = (float)wSize[0]; scene.worldSize.y = (float)wSize[1]; }
                            ImGui::PopItemWidth();
                            ImGui::Unindent(8.0f);
                        }

                        ImGui::Spacing();

                        // --- Tags ---
                        if (ImGui::CollapsingHeader("Tags", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Indent(8.0f);
                            ImGui::TextDisabled("Entity tags used by scripts and colliders.");
                            ImGui::Spacing();

                            const auto& currentTags = TagRegistry::Get().GetTags();
                            for (int t = 0; t < (int)currentTags.size(); ++t)
                            {
                                ImGui::PushID(t);
                                if (t == 0) { ImGui::TextDisabled("  %s  (built-in)", currentTags[t].c_str()); }
                                else
                                {
                                    ImGui::AlignTextToFramePadding();
                                    ImGui::TextUnformatted(currentTags[t].c_str());
                                    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 24.0f);
                                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
                                    if (ImGui::SmallButton(ICON_FA_TRASH "##del"))
                                    {
                                        ImGui::PopStyleColor();
                                        ImGui::PopID();
                                        std::vector<std::string> newTags = currentTags;
                                        newTags.erase(newTags.begin() + t);
                                        pm.SaveProjectTags(newTags);
                                        break;
                                    }
                                    ImGui::PopStyleColor();
                                }
                                ImGui::PopID();
                            }

                            ImGui::Spacing();
                            static char newTagBuf[64] = {};
                            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
                            ImGui::InputText("##NewTag", newTagBuf, sizeof(newTagBuf));
                            ImGui::SameLine();
                            bool tagNameOk = newTagBuf[0] != '\0';
                            if (!tagNameOk) ImGui::BeginDisabled();
                            if (ImGui::Button("Add Tag"))
                            {
                                std::string newTag(newTagBuf);
                                bool exists = false;
                                for (const auto& t : currentTags) if (t == newTag) { exists = true; break; }
                                if (!exists)
                                {
                                    std::vector<std::string> newTags = currentTags;
                                    newTags.push_back(newTag);
                                    pm.SaveProjectTags(newTags);
                                    newTagBuf[0] = '\0';
                                }
                            }
                            if (!tagNameOk) ImGui::EndDisabled();

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
    inline void Editor::ApplyTheme(std::string theme)
    {
        this->THEME_STYLE = theme;

        // Persist so the choice survives restarts
        {
            nlohmann::json prefs;
            std::ifstream inPf(PrefsPath());
            if (inPf.is_open()) { try { prefs = nlohmann::json::parse(inPf); } catch (...) {} }
            prefs["theme"] = theme;
            std::ofstream outPf(PrefsPath());
            if (outPf.is_open()) outPf << prefs.dump(4);
        }

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // --- Global Visual Styling ---
        style.WindowRounding    = 0.0f;          // Sharp windows for industrial look
        style.ChildRounding     = 0.0f;          // Sharp child panels
        style.FrameRounding     = 5.0f;          // Rounded buttons and widgets
        style.PopupRounding     = 5.0f;          // Rounded popups
        style.ScrollbarRounding = 12.0f;
        style.GrabRounding      = 5.0f;
        style.TabRounding       = 0.0f;          // Sharp tabs to match windows

        style.WindowBorderSize  = 1.5f;
        style.FrameBorderSize   = 1.5f;
        style.PopupBorderSize   = 1.5f;

        style.WindowPadding     = ImVec2(12.0f, 10.0f);
        style.FramePadding      = ImVec2(8.0f, 5.0f);
        style.ItemSpacing       = ImVec2(8.0f, 7.0f);
        style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
        style.GrabMinSize       = 15.0f;

        // Theme selection logic
        // Based on the provided THEME_STYLE string, the corresponding color palette
        // is applied. This allows dynamic switching between dark and light modes.
        theme == "dark" ? ApplyDarkTheme(colors) : ApplyLightTheme(colors);
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
                if (ImGui::MenuItem("Save", "Ctrl+S")) { pm.SaveCurrentProject(scene); isDirty = false;}
                ImGui::Separator();
                if (ImGui::MenuItem("Exit to Launcher"))
                {
                    if (isDirty) { showUnsavedChangesPopup = true; wantsToExitToLauncher = true; }
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
                    if (isDirty) { showUnsavedChangesPopup = true; wantsToExit = true; }
                    else { shouldExitImmediately = true; }
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
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoStack.empty() && state != GameState::Play))       Undo();
                if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, !redoStack.empty() && state != GameState::Play)) Redo();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Scripts"))
            {
                if (ImGui::MenuItem("Compile & Reload", nullptr, false, state == GameState::Editor))
                {
                    if (!pm.IsProjectOpen()) { consoleLogs.push_back({ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "[WARNING]", "No project open to compile scripts.", ICON_FA_EXCLAMATION}); }
                    else
                    {
                        std::string logOutput;
                        if (ScriptManager::Get().CompileScripts(pm.GetCurrentProjectPath(), logOutput))
                        {
                            // Live NativeScript instances hold vtable/code pointers into the
                            // currently-loaded dylib. dlclose would invalidate them, so snapshot
                            // the scene to JSON, destroy every owner (their destructors still
                            // dispatch into the loaded image), reload, then rehydrate fresh
                            // instances against the new createFunc.
                            nlohmann::json sceneState = scene.serialize();
                            int prevSelected = selectedIndex;

                            scene.entities.clear();
                            scene.snapshot.clear();
                            scene.startQueue.clear();
                            scene.destroyQueue.clear();
                            selectedIndex = -1;
                            multiSelection_.clear();

                            ScriptManager::Get().LoadLibrary(pm.GetCurrentProjectPath());

                            scene.nextEntityId = sceneState.value("nextEntityId", 1);
                            if (sceneState.contains("entities"))
                            {
                                for (const auto& ej : sceneState["entities"]) { auto e = factory.LoadEntity(ej); if (e) scene.entities.push_back(std::move(e)); }
                                scene.RebuildHierarchy();
                            }
                            if (prevSelected >= 0 && prevSelected < (int)scene.entities.size()) selectedIndex = prevSelected;
                            consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[COMPILER]", logOutput, ICON_FA_CHECK});
                        }
                        else { consoleLogs.push_back({ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "[COMPILER ERROR]", logOutput, ICON_FA_XMARK}); }
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
                const std::string currentSceneFile       = pm.GetCurrentSceneName() + ".scene";
                for (const auto& sceneFile : sceneList)
                {
                    const std::string displayName = fs::path(sceneFile).stem().string();
                    const bool isCurrent          = (sceneFile == currentSceneFile);

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
                            editorCamera.target = scene.editorCameraTarget;
                            editorCamera.zoom   = scene.editorCameraZoom;
                            editorCamera.offset = { 0, 0 };
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
                { sceneDeleteTarget = currentSceneFile; showDeleteSceneModal_ = true; }
                ImGui::PopStyleColor();
                ImGui::EndMenu();
            }
            // Handle global hotkeys
            if (state == GameState::Editor && CtrlDown() && IsKeyPressed(KEY_S))
            {
                pm.SaveCurrentProject(scene);
                if (pm.IsProjectOpen()) InputManager::Get().Save(pm.GetCurrentProjectPath() + "/input.json");
                isDirty = false;
            }
            if (CtrlDown() && !IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_Z)) Undo();
            if (CtrlDown() && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_Z))  Redo();

            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem("Circle"))          CreateEntityAt("Circle",    editorCamera.target);
                if (ImGui::MenuItem("Rectangle"))       CreateEntityAt("Rectangle", editorCamera.target);
                if (ImGui::MenuItem("Surface"))         CreateEntityAt("Surface",   editorCamera.target);
                if (ImGui::MenuItem("Image (Sprite)"))  CreateEntityAt("Sprite",    editorCamera.target);
                if (ImGui::MenuItem("Camera"))          CreateEntityAt("Camera",    editorCamera.target);
                ImGui::EndMenu();
            }

            // Input action mapping window
            if (ImGui::MenuItem(ICON_FA_GAMEPAD "  Input"))
                showInputManager_ = !showInputManager_;

            // Theme menu: allows runtime switching between available UI themes
            // This updates ImGui styling dynamically without restarting the application.
            if (ImGui::BeginMenu("Theme"))
            {
                if (ImGui::MenuItem("Dark theme"))  { ApplyTheme("dark"); }
                if (ImGui::MenuItem("Light theme")) { ApplyTheme("light"); }
                ImGui::EndMenu();
            }

            // --- Centered Play / Pause / Stop toolbar ---
            const float btnW    = 30.0f;
            const float spacing = 3.0f;
            const float totalW  = btnW * 3 + spacing * 2;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - totalW) * 0.5f);

            const bool inPlay    = (state == GameState::Play || state == GameState::Pause);
            const bool isPause   = (state == GameState::Pause);
            const bool wasEditor = (state == GameState::Editor);

            // ▶ Play — green highlight while running
            if (inPlay) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.62f, 0.20f, 1.0f));
            if (ImGui::Button(ICON_FA_PLAY, ImVec2(btnW, 0)) && state == GameState::Editor)
            {
                scene.Save();
                state = GameState::Play;
                StoryState::Get().Clear();
                StoryState::Get().Seed(scene.storyState);
                DialogueManager::Get().End();
                for (auto& e : scene.entities) for (auto& c : e->components) c->awake(&scene);
                for (auto& e : scene.entities) for (auto& c : e->components) c->start(&scene);
                Events::Publish(GameEvents::GameStartEvent{});
            }
            if (inPlay) ImGui::PopStyleColor();

            ImGui::SameLine(0, spacing);

            // ⏸ Pause — amber highlight while paused, greyed out in editor
            if (wasEditor) ImGui::BeginDisabled();
            if (isPause) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.52f, 0.04f, 1.0f));
            if (ImGui::Button(ICON_FA_PAUSE, ImVec2(btnW, 0)))
            {
                if      (state == GameState::Play)  state = GameState::Pause;
                else if (state == GameState::Pause) state = GameState::Play;
            }
            if (isPause) ImGui::PopStyleColor();
            if (wasEditor) ImGui::EndDisabled();

            ImGui::SameLine(0, spacing);

            // ⏹ Stop — red highlight while running, greyed out in editor
            if (wasEditor) ImGui::BeginDisabled();
            if (inPlay) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.12f, 0.12f, 1.0f));
            if (ImGui::Button(ICON_FA_STOP, ImVec2(btnW, 0)))
            {
                Events::Publish(GameEvents::GameStopEvent{});
                scene.Restore();
                state = GameState::Editor;
                selectedIndex = -1;
                multiSelection_.clear();
                StoryState::Get().Clear();
                DialogueManager::Get().End();
                EventBus::Get().Clear();
                StoryState::Get().SubscribeToEvents(); // re-seat: EventBus::Clear() wiped our handler
            }
            if (inPlay) ImGui::PopStyleColor();
            if (wasEditor) ImGui::EndDisabled();

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
            if (ImGui::MenuItem("Surface"))         { TakeSnapshot(); auto e = factory.CreatePlane(scene);     e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            if (ImGui::MenuItem("Image (Sprite)"))  { TakeSnapshot(); auto e = factory.CreateSprite(scene);    e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            if (ImGui::MenuItem("Camera"))          { TakeSnapshot(); auto e = factory.CreateCamera(scene);    e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            ImGui::EndPopup();
        }

        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5));

        std::vector<int> entitiesToDelete;

        std::unordered_map<Entity*, int> entityIndexMap;
        entityIndexMap.reserve(scene.entities.size());
        for (int i = 0; i < (int)scene.entities.size(); i++) entityIndexMap[scene.entities[i].get()] = i;

        // Lambda: recursively draw an entity as a tree node
        std::function<void(Entity*, int)> DrawEntityNode;
        DrawEntityNode = [&](Entity* entity, int index)
        {
            ImGui::PushID(entity->id);

            // --- Stealth Styling ---
            ImGuiStyle& style    = ImGui::GetStyle();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 cursorPos     = ImGui::GetCursorScreenPos();
            float fullWidth      = ImGui::GetContentRegionAvail().x;
            float nodeHeight     = 25.0f;

            // --- Entity Icons ---
            const char* icon = ICON_FA_CUBE;
            if (entity->getType() == "Circle")    icon = ICON_FA_CIRCLE;
            if (entity->getType() == "Rectangle") icon = ICON_FA_VECTOR_SQUARE;
            if (entity->getType() == "Plane")     icon = ICON_FA_LAYER_GROUP;
            if (entity->getType() == "Sprite")    icon = ICON_FA_IMAGE;
            for (const auto& c : entity->components) { if (dynamic_cast<CameraComponent*>(c.get())) { icon = ICON_FA_CAMERA; break; } }

            // 1. Draw Rounded Selection Background (If selected)
            bool inMultiSel_ = std::find(multiSelection_.begin(), multiSelection_.end(), index) != multiSelection_.end();
            if (selectedIndex == index)
            {
                ImU32 selCol = ImGui::GetColorU32(ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
                drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + fullWidth, cursorPos.y + nodeHeight), selCol, 5.0f);
                drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + 3, cursorPos.y + nodeHeight), ImColor(100, 100, 100, 255), 5.0f);
            }
            else if (inMultiSel_)
            {
                ImU32 multiSelCol = ImGui::GetColorU32(ImVec4(0.10f, 0.20f, 0.35f, 1.0f));
                drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + fullWidth, cursorPos.y + nodeHeight), multiSelCol, 4.0f);
                drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + 3, cursorPos.y + nodeHeight), ImColor(0, 140, 220, 255), 4.0f);
            }

            // 2. Node Styling
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));

            // Clear default header colors to use custom background
            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, selectedIndex == index ? ImVec4(0,0,0,0) : ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  selectedIndex == index ? ImVec4(0,0,0,0) : ImVec4(0.10f, 0.10f, 0.10f, 0.5f));

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
            {
                if (CtrlDown())
                {
                    auto it = std::find(multiSelection_.begin(), multiSelection_.end(), index);
                    if (it != multiSelection_.end()) multiSelection_.erase(it);
                    else { multiSelection_.push_back(index); selectedIndex = index; }
                }
                else
                {
                    selectedIndex = index;
                    multiSelection_.clear();
                }
            }

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
                                Vector2 gPos = dragged->getGlobalPosition();
                                dragged->setParent(entity); // fires onEnable/onDisable if hierarchy-active changed
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
                {
                    bool hm = multiSelection_.size() > 1;
                    if (ImGui::MenuItem("Cut", "Ctrl+X"))
                    {
                        CopySelected();
                        if (hm) { for (int i : multiSelection_) entitiesToDelete.push_back(i); multiSelection_.clear(); }
                        else entitiesToDelete.push_back(index);
                    }
                    if (ImGui::MenuItem("Copy", "Ctrl+C")) CopySelected();
                    bool canPasteH = !entityClipboard.is_null() || !multiClipboard_.empty();
                    if (ImGui::MenuItem("Paste", "Ctrl+V", false, canPasteH)) PasteAt(entity->position);
                    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) DuplicateSelected(index);
                }

                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_BOX "  Save as Prefab...") && pm.IsProjectOpen())
                {
                    prefabSourceIndex_ = index;
                    strncpy(prefabNameBuf_, entity->name.c_str(), sizeof(prefabNameBuf_) - 1);
                    showSavePrefabModal_ = true;
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Unparent", nullptr, false, entity->parent != nullptr))
                {
                    if (entity->parent)
                    {
                        TakeSnapshot();
                        Vector2 gPos = entity->getGlobalPosition();
                        entity->setParent(nullptr); // fires onEnable/onDisable if hierarchy-active changed
                        entity->position = gPos;
                    }
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Del"))
                {
                    if (multiSelection_.size() > 1) { for (int i : multiSelection_) entitiesToDelete.push_back(i); multiSelection_.clear(); }
                    else entitiesToDelete.push_back(index);
                }

                ImGui::EndPopup();
            }

            // Recurse into children
            if (wasPushed)
            {
                if (!entity->children.empty())
                {
                    for (Entity* child : entity->children)
                    {
                        auto it = entityIndexMap.find(child);
                        if (it != entityIndexMap.end()) DrawEntityNode(child, it->second);
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
            ImGui::PopID();
        };

        // --- Multi-scene Hierarchy ---
        const std::vector<std::string> allScenes    = pm.IsProjectOpen() ? pm.GetSceneList() : std::vector<std::string>{};
        const std::string              currentStem  = pm.GetCurrentSceneName();
        const std::string              currentFile  = currentStem + ".scene";

        // Active scene node — expanded by default, blue tint
        {
            ImGuiTreeNodeFlags sceneFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.82f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 5));
            bool sceneOpen = ImGui::TreeNodeEx("##ActiveScene", sceneFlags, "%s  %s", ICON_FA_FOLDER_OPEN, currentStem.empty() ? "Scene" : currentStem.c_str());
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            // Right-click on active scene node
            if (ImGui::BeginPopupContextItem("ActiveSceneCtx"))
            {
                ImGui::TextDisabled("%s", currentStem.empty() ? "Scene" : currentStem.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  Save Scene", "Ctrl+S"))
                {
                    pm.SaveCurrentProject(scene);
                    isDirty = false;
                }
                if (ImGui::MenuItem("Rename Scene..."))
                {
                    sceneRenameTarget = currentFile;
                    strncpy(sceneRenameBuffer, currentStem.c_str(), sizeof(sceneRenameBuffer) - 1);
                    showRenameSceneModal_ = true;
                }
                if (ImGui::MenuItem("New Scene...")) showNewSceneModal_ = true;
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                if (ImGui::MenuItem("Delete Scene...", nullptr, false, allScenes.size() > 1))
                {
                    sceneDeleteTarget = currentFile;
                    showDeleteSceneModal_ = true;
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            if (sceneOpen)
            {
                // Root entities of the active scene
                for (int i = 0; i < (int)scene.entities.size(); i++)
                {
                    if (scene.entities[i]->parent == nullptr) { DrawEntityNode(scene.entities[i].get(), i); }
                }

                // Drop onto scene node = unparent
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
                            dragged->setParent(nullptr);
                            dragged->position = gPos;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TreePop();
            }
        }

        // Other (unloaded) scene nodes — greyed out leaf nodes
        for (const auto& sceneFile : allScenes)
        {
            if (sceneFile == currentFile) continue;

            const std::string stemName = fs::path(sceneFile).stem().string();

            ImGuiTreeNodeFlags leafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 5));
            ImGui::TreeNodeEx(sceneFile.c_str(), leafFlags, "%s  %s", ICON_FA_FILE, stemName.c_str());
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            // Double-click to switch to this scene
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && state == GameState::Editor)
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

            // Right-click on inactive scene node
            if (ImGui::BeginPopupContextItem())
            {
                ImGui::TextDisabled("%s", stemName.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Load Scene") && state == GameState::Editor)
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
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                if (ImGui::MenuItem("Delete Scene...")) { sceneDeleteTarget     = sceneFile; showDeleteSceneModal_ = true; }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }
        }

        // Execute deletion safely outside the loop
        if (!entitiesToDelete.empty())
        {
            TakeSnapshot();
            DeleteEntitiesAt(entitiesToDelete);
        }

        // Save as Prefab modal
        if (showSavePrefabModal_) { ImGui::OpenPopup("SavePrefabModal##H"); showSavePrefabModal_ = false; }
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("SavePrefabModal##H", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("Prefab Name:");
            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputText("##PrefabName", prefabNameBuf_, sizeof(prefabNameBuf_));
            ImGui::Spacing();
            if (ImGui::Button("Save", ImVec2(126, 0)))
            {
                if (prefabNameBuf_[0] != '\0' && prefabSourceIndex_ >= 0 && prefabSourceIndex_ < (int)scene.entities.size())
                {
                    nlohmann::json j = scene.entities[prefabSourceIndex_]->serialize();
                    if (PrefabManager::Save(j, pm.GetCurrentProjectPath(), prefabNameBuf_))
                    {
                        consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO] ", std::string("Prefab saved: ") + prefabNameBuf_ + ".prefab", ICON_FA_BOX});
                    }
                    else consoleLogs.push_back({ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "[ERROR]", std::string("Failed to save prefab: ") + prefabNameBuf_, ICON_FA_TRIANGLE_EXCLAMATION});
                }
                prefabSourceIndex_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(126, 0))) { prefabSourceIndex_ = -1; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        // --- Keyboard Shortcuts ---
        if (!ImGui::GetIO().WantTextInput && state == GameState::Editor)
        {
            bool hasSel    = selectedIndex >= 0 && selectedIndex < (int)scene.entities.size();
            bool hasMulti  = multiSelection_.size() > 1;
            bool hasAny    = hasSel || hasMulti;

            // Cut
            if (hasAny && CtrlDown() && IsKeyPressed(KEY_X))
            {
                TakeSnapshot();
                CopySelected();
                if (hasMulti)
                {
                    DeleteEntitiesAt(multiSelection_);
                    multiSelection_.clear();
                }
                else DeleteEntity(*scene.entities[selectedIndex]);
            }

            // Copy
            if (hasAny && CtrlDown() && IsKeyPressed(KEY_C)) CopySelected();

            // Duplicate
            if (hasAny && CtrlDown() && IsKeyPressed(KEY_D)) DuplicateSelected(selectedIndex);

            // Paste
            bool canPasteKb = !entityClipboard.is_null() || !multiClipboard_.empty();
            if (canPasteKb && CtrlDown() && IsKeyPressed(KEY_V)) PasteAt(selectedIndex >= 0 ? scene.entities[selectedIndex]->position : editorCamera.target);

            // Delete
            if (hasAny && (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)))

            {
                TakeSnapshot();
                if (hasMulti)
                {
                    DeleteEntitiesAt(multiSelection_);
                    multiSelection_.clear();
                }
                else DeleteEntity(*scene.entities[selectedIndex]);
            }
        }

        // Right-click context menu for empty space in the Hierarchy
        if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::BeginMenu("2D Object"))
            {
                if (ImGui::MenuItem(ICON_FA_CIRCLE "  Circle"))           CreateEntityAt("Circle",    editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_VECTOR_SQUARE "  Rectangle")) CreateEntityAt("Rectangle", editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_LAYER_GROUP "  Surface"))     CreateEntityAt("Surface",   editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_IMAGE "  Image (Sprite)"))    CreateEntityAt("Sprite",    editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_CAMERA "  Camera"))           CreateEntityAt("Camera",    editorCamera.target);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            {
                bool canPasteEmpty = !entityClipboard.is_null() || !multiClipboard_.empty();
                if (ImGui::MenuItem("Paste", "Ctrl+V", false, canPasteEmpty)) PasteAt(editorCamera.target);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Deselect All", nullptr, false, selectedIndex >= 0)) selectedIndex = -1;
            ImGui::EndPopup();
        }
        ImGui::End();
    }

    inline void Editor::ShowViewport()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // --- Scene / Game tab bar — offset past the hierarchy panel so it isn't hidden behind it ---
        ImGui::SetCursorPosX(hierarchyWidth);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
        if (ImGui::BeginTabBar("##ViewTabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem(ICON_FA_PENCIL "  Scene"))
            {
                viewportTab_ = 0;
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(ICON_FA_GAMEPAD "  Game"))
            {
                viewportTab_ = 1;
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::PopStyleVar(); // FramePadding

        // --- Transform tool toolbar ---
        if (viewportTab_ == 0 && state == GameState::Editor)
        {
            ImGui::SetCursorPosX(hierarchyWidth + 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 3));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(3, 0));

            auto toolBtn = [&](const char* icon, TransformTool tool, const char* tip)
            {
                bool active = (activeTool_ == tool);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.76f, 1.0f));
                if (ImGui::Button(icon, ImVec2(26, 20))) activeTool_ = tool;
                if (active) ImGui::PopStyleColor();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tip);
                ImGui::SameLine();
            };

            toolBtn(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, TransformTool::Move,      "Move  [W]");
            toolBtn(ICON_FA_ROTATE,                    TransformTool::Rotate,    "Rotate  [E]");
            toolBtn(ICON_FA_EXPAND,                    TransformTool::Rect,      "Rect Transform  [R]");
            toolBtn(ICON_FA_WAND_MAGIC_SPARKLES,       TransformTool::Universal, "Universal  [Y]");

            // Separator
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            // Grid toggle
            bool gridWasOn = showGrid_;
            if (gridWasOn) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.76f, 1.0f));
            if (ImGui::Button(ICON_FA_TABLE_CELLS, ImVec2(26, 20))) showGrid_ = !showGrid_;
            if (gridWasOn) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Toggle Grid  [G]");
            ImGui::SameLine();

            // Snap toggle
            bool snapWasOn = snapEnabled_;
            if (snapWasOn) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.76f, 1.0f));
            if (ImGui::Button(ICON_FA_MAGNET, ImVec2(26, 20))) snapEnabled_ = !snapEnabled_;
            if (snapWasOn) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Toggle Snap  [S]");
            ImGui::SameLine();

            // Snap size input
            if (snapEnabled_)
            {
                ImGui::SetNextItemWidth(52.0f);
                ImGui::DragFloat("##SnapSz", &snapSize_, 1.0f, 1.0f, 512.0f, "%.0f");
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Snap size (world units)");
                ImGui::SameLine();
            }

            ImGui::PopStyleVar(2);
            ImGui::Separator();
        }

        ImVec2 viewportAvail = ImGui::GetContentRegionAvail();
        ImVec2 renderArea = ImVec2(std::max(viewportAvail.x, 1.0f), std::max(viewportAvail.y, 1.0f));

        ImGui::BeginChild("ViewportRender", renderArea, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Capture the viewport's position and size for mouse coordinate mapping
        viewportPos.x       = ImGui::GetCursorScreenPos().x;
        viewportPos.y       = ImGui::GetCursorScreenPos().y;
        viewportSize.x      = ImGui::GetContentRegionAvail().x;
        viewportSize.y      = ImGui::GetContentRegionAvail().y;
        float mouseX        = ImGui::GetIO().MousePos.x;
        bool mouseOverPanel = (mouseX < hierarchyWidth) || (mouseX > (float)GetScreenWidth() - inspectorWidth);
        viewportHovered     = ImGui::IsWindowHovered() && !mouseOverPanel && !isResizingHierarchy && !isResizingInspector && !isResizingBottom;

        rlImGuiImageRenderTextureFit(&viewport, false);

        // Prefab drop onto viewport
        if (state == GameState::Editor && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                std::string relPath((const char*)payload->Data, payload->DataSize - 1);
                fs::path droppedPath = fs::path(pm.GetCurrentProjectPath()) / relPath;
                if (droppedPath.extension() == ".prefab" && pm.IsProjectOpen())
                {
                    nlohmann::json j = PrefabManager::Load(droppedPath.string());
                    if (!j.is_null())
                    {
                        TakeSnapshot();
                        auto e = factory.LoadEntity(j);
                        if (e)
                        {
                            e->id       = scene.nextEntityId++;
                            e->position = worldMouse;
                            scene.entities.push_back(std::move(e));
                            selectedIndex = (int)scene.entities.size() - 1;
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (viewportHovered)
        {
            // Update worldMouse accurately based on current frame viewport data
            Camera2D activeCamera = GetActiveCamera();
            Vector2 screenMouse   = GetMousePosition();

            float scaleX = (viewportSize.x > 0) ? (float)viewport.texture.width  / viewportSize.x : 1.0f;
            float scaleY = (viewportSize.y > 0) ? (float)viewport.texture.height / viewportSize.y : 1.0f;

            Vector2 scaledMouse =
            {
                (screenMouse.x - viewportPos.x) * scaleX,
                (screenMouse.y - viewportPos.y) * scaleY
            };

            worldMouse = GetScreenToWorld2D(scaledMouse, activeCamera);

            // 1. Camera Panning (Middle Mouse)
            if (state == GameState::Editor && IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
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

            // Tool hotkeys
            if (state == GameState::Editor && !ImGui::GetIO().WantTextInput)
            {
                if (IsKeyPressed(KEY_W)) activeTool_ = TransformTool::Move;
                if (IsKeyPressed(KEY_E)) activeTool_ = TransformTool::Rotate;
                if (IsKeyPressed(KEY_R)) activeTool_ = TransformTool::Rect;
                if (IsKeyPressed(KEY_Y)) activeTool_ = TransformTool::Universal;
                if (IsKeyPressed(KEY_G)) showGrid_    = !showGrid_;
                if (IsKeyPressed(KEY_S) && !CtrlDown()) snapEnabled_ = !snapEnabled_;
            }

            // Sorted pick order (draw order = visual depth, used for click priority)
            auto makeSortedPick = [&]()
            {
                std::vector<int> order((int)scene.entities.size());
                std::iota(order.begin(), order.end(), 0);
                std::sort(order.begin(), order.end(), [&](int a, int b)
                {
                    return scene.entities[a]->computeSortKey() < scene.entities[b]->computeSortKey();
                });
                return order;
            };

            // 3. Left-Click — selection, handle start, body drag
            if (viewportTab_ == 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                draggingEntity = nullptr;
                activeHandle_  = HandleType::None;
                isSelectingBox = false;
                bool handleHit = false;

                // First, if an entity is already selected, try to hit its handles.
                // This ensures handles outside the entity's body (like rotation handle)
                // can be clicked without deselecting the entity.
                if (selectedIndex >= 0 && selectedIndex < (int)scene.entities.size() && state == GameState::Editor)
                {
                    Entity* sel    = scene.entities[selectedIndex].get();
                    Vector2 center = sel->getGlobalPosition();
                    float   rot    = sel->getGlobalRotation();
                    float   hw     = sel->scale.x / 2.0f;
                    float   hh     = sel->scale.y / 2.0f;
                    float   HR     = 10.0f / editorCamera.zoom;

                    auto toWorld = [&](float lx, float ly) -> Vector2
                    {
                        float rad  = rot * DEG2RAD;
                        float c    = cosf(rad);
                        float s    = sinf(rad);
                        return {center.x + lx*c - ly*s, center.y + lx*s + ly*c};
                    };

                    // Rect handles (Rect or Universal mode) — disabled for parented entities
                    if (!handleHit && (activeTool_ == TransformTool::Rect || activeTool_ == TransformTool::Universal) && !sel->getVertices().empty())
                    {
                        if (sel->parentId == -1) // scale handles only work on root entities
                        {
                            Vector2 hpts[8] =
                            {
                                toWorld(-hw,-hh), toWorld(0,-hh), toWorld(+hw,-hh), toWorld(+hw,0),
                                toWorld(+hw,+hh), toWorld(0,+hh), toWorld(-hw,+hh), toWorld(-hw,0)
                            };
                            HandleType htypes[8] =
                            {
                                HandleType::H_TL, HandleType::H_TM, HandleType::H_TR, HandleType::H_RM,
                                HandleType::H_BR, HandleType::H_BM, HandleType::H_BL, HandleType::H_LM
                            };
                            for (int k = 0; k < 8; k++)
                            {
                                if (Vector2Distance(worldMouse, hpts[k]) <= HR)
                                {
                                    TakeSnapshot();
                                    isDirty                   = true;
                                    activeHandle_             = htypes[k];
                                    handleDragStartMouse_     = worldMouse;
                                    handleDragStartGlobalPos_ = center;
                                    handleDragStartScale_     = sel->scale;
                                    handleDragStartRot_       = rot;
                                    handleHit                 = true;
                                    break;
                                }
                            }
                        }
                    }

                    // Rotation handle (Rotate or Universal mode)
                    if (!handleHit && (activeTool_ == TransformTool::Rotate || activeTool_ == TransformTool::Universal))
                    {
                        float gizmoR = fmaxf(hw, hh) + 28.0f / editorCamera.zoom;
                        float thresh = 12.0f / editorCamera.zoom;
                        if (fabsf(Vector2Distance(worldMouse, center) - gizmoR) <= thresh)
                        {
                            TakeSnapshot(); isDirty = true;
                            activeHandle_             = HandleType::H_Rotate;
                            handleDragStartMouse_     = worldMouse;
                            handleDragStartGlobalPos_ = center;
                            handleDragStartRot_       = sel->getGlobalRotation();
                            handleHit = true;
                        }
                    }
                }

                // If no handle was clicked, perform normal entity picking
                if (!handleHit)
                {
                    auto pickOrder = makeSortedPick();
                    int clickedIndex = -1;
                    for (int pi = (int)pickOrder.size() - 1; pi >= 0; pi--)
                    {
                        if (scene.entities[pickOrder[pi]]->Contains(worldMouse)) { clickedIndex = pickOrder[pi]; break; }
                    }

                    bool ctrlHeld = CtrlDown() || IsKeyDown(KEY_RIGHT_CONTROL);

                    if (clickedIndex == -1)
                    {
                        // Empty space: deselect all + begin box select
                        if (!ctrlHeld)
                        {
                            selectedIndex = -1;
                            multiSelection_.clear();
                        }
                        if (state == GameState::Editor) { isSelectingBox = true; selectBoxStart = worldMouse; }
                    }
                    else if (ctrlHeld && state == GameState::Editor)
                    {
                        // Ctrl+Click: toggle entity in multi-selection
                        auto it = std::find(multiSelection_.begin(), multiSelection_.end(), clickedIndex);
                        if (it != multiSelection_.end()) multiSelection_.erase(it);
                        else multiSelection_.push_back(clickedIndex);
                        selectedIndex = clickedIndex; // inspector shows last clicked
                    }
                    else if (clickedIndex != selectedIndex)
                    {
                        bool inMulti = std::find(multiSelection_.begin(), multiSelection_.end(), clickedIndex) != multiSelection_.end();
                        if (inMulti && multiSelection_.size() > 1 && state == GameState::Editor && activeTool_ == TransformTool::Move)
                        {
                            // Clicked a multi-selected entity that isn't the primary — start multi-drag
                            selectedIndex = clickedIndex;
                            TakeSnapshot();
                            multiDragStartMouse_ = worldMouse;
                            multiDragStartPos_.clear();
                            for (int idx : multiSelection_) multiDragStartPos_.push_back(scene.entities[idx]->position);
                        }
                        else
                        {
                            multiSelection_.clear();
                            selectedIndex = clickedIndex;
                            if (state == GameState::Editor && activeTool_ == TransformTool::Move)
                            {
                                TakeSnapshot();
                                draggingEntity = scene.entities[clickedIndex].get();
                                dragOffset     = {draggingEntity->position.x - worldMouse.x, draggingEntity->position.y - worldMouse.y};
                            }
                        }
                    }
                    else if (state == GameState::Editor)
                    {
                        // Clicked same entity — body drag (single) or begin multi-drag
                        if (multiSelection_.size() > 1)
                        {
                            // Start multi-drag
                            TakeSnapshot();
                            multiDragStartMouse_ = worldMouse;
                            multiDragStartPos_.clear();
                            for (int idx : multiSelection_) multiDragStartPos_.push_back(scene.entities[idx]->position);
                        }
                        else
                        {
                            Entity* sel = scene.entities[selectedIndex].get();
                            TakeSnapshot();
                            activeHandle_  = HandleType::Body;
                            draggingEntity = sel;
                            dragOffset     = {sel->position.x - worldMouse.x, sel->position.y - worldMouse.y};
                        }
                    }
                }
            }

            // 4. Context Menu (Right Click) — only in Scene tab
            if (viewportTab_ == 0 && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
            {
                contextEntityIndex = -1;
                auto pickOrder = makeSortedPick();
                for (int pi = (int)pickOrder.size() - 1; pi >= 0; pi--)
                {
                    int i = pickOrder[pi];
                    if (scene.entities[i]->Contains(worldMouse))
                    {
                        contextEntityIndex = i;
                        selectedIndex      = i;
                        break;
                    }
                }
            }

            // 5. Dragging / Handle update
            if (viewportTab_ == 0)
            {
                // 5a. Body move drag (single entity)
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggingEntity != nullptr)
                {
                    float rawX = worldMouse.x + dragOffset.x;
                    float rawY = worldMouse.y + dragOffset.y;
                    if (snapEnabled_ && snapSize_ > 0.0f)
                    {
                        rawX = roundf(rawX / snapSize_) * snapSize_;
                        rawY = roundf(rawY / snapSize_) * snapSize_;
                    }
                    draggingEntity->position.x = rawX;
                    draggingEntity->position.y = rawY;
                    isDirty = true;
                }

                // 5a2. Multi-entity drag
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !multiSelection_.empty()
                    && !multiDragStartPos_.empty() && draggingEntity == nullptr)
                {
                    Vector2 delta = {worldMouse.x - multiDragStartMouse_.x,
                                     worldMouse.y - multiDragStartMouse_.y};
                    for (int i = 0; i < (int)multiSelection_.size() && i < (int)multiDragStartPos_.size(); ++i)
                    {
                        int idx = multiSelection_[i];
                        if (idx < 0 || idx >= (int)scene.entities.size()) continue;
                        float rawX = multiDragStartPos_[i].x + delta.x;
                        float rawY = multiDragStartPos_[i].y + delta.y;
                        if (snapEnabled_ && snapSize_ > 0.0f)
                        {
                            rawX = roundf(rawX / snapSize_) * snapSize_;
                            rawY = roundf(rawY / snapSize_) * snapSize_;
                        }
                        scene.entities[idx]->position = {rawX, rawY};
                    }
                    isDirty = true;
                }
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) multiDragStartPos_.clear();

                // 5b. Handle drag (Rect / Rotate tools)
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && activeHandle_ != HandleType::None && activeHandle_ != HandleType::Body && selectedIndex >= 0 && selectedIndex < (int)scene.entities.size())
                {
                    Entity* sel = scene.entities[selectedIndex].get();
                    Vector2 P   = handleDragStartGlobalPos_;
                    float   rot = handleDragStartRot_ * DEG2RAD;
                    float   hw  = handleDragStartScale_.x / 2.0f;
                    float   hh  = handleDragStartScale_.y / 2.0f;

                    // Mouse position in entity-local space (relative to drag-start center)
                    float dx = worldMouse.x - P.x;
                    float dy = worldMouse.y - P.y;
                    float uc = cosf(-rot), us = sinf(-rot);
                    float mx = dx * uc - dy * us;  // local X
                    float my = dx * us + dy * uc;  // local Y

                    // Apply new scale and recompute center from local offset
                    auto applyResize = [&](float newW, float newH, float localCX, float localCY)
                    {
                        if (sel->parentId != -1) return;  // skip for parented entities
                        sel->scale.x    = std::max(newW, 2.0f);
                        sel->scale.y    = std::max(newH, 2.0f);
                        float rc        = cosf(handleDragStartRot_ * DEG2RAD);
                        float rs        = sinf(handleDragStartRot_ * DEG2RAD);
                        sel->position   = {P.x + localCX * rc - localCY * rs, P.y + localCX * rs + localCY * rc};
                        isDirty = true;
                    };

                    switch (activeHandle_)
                    {
                        // Corners (opposite corner stays fixed in world space)
                        case HandleType::H_TL: applyResize(fabsf(hw-mx), fabsf(hh-my), (hw+mx)/2.f, (hh+my)/2.f); break;
                        case HandleType::H_TR: applyResize(fabsf(mx+hw), fabsf(hh-my), (-hw+mx)/2.f,(hh+my)/2.f); break;
                        case HandleType::H_BR: applyResize(fabsf(mx+hw), fabsf(my+hh), (-hw+mx)/2.f,(-hh+my)/2.f); break;
                        case HandleType::H_BL: applyResize(fabsf(hw-mx), fabsf(my+hh), (hw+mx)/2.f, (-hh+my)/2.f); break;
                        // Edges (one axis locked)
                        case HandleType::H_TM: applyResize(hw*2.f, fabsf(hh-my), 0.f, (hh+my)/2.f); break;
                        case HandleType::H_BM: applyResize(hw*2.f, fabsf(my+hh), 0.f, (-hh+my)/2.f); break;
                        case HandleType::H_LM: applyResize(fabsf(hw-mx), hh*2.f, (hw+mx)/2.f, 0.f); break;
                        case HandleType::H_RM: applyResize(fabsf(mx+hw), hh*2.f, (-hw+mx)/2.f, 0.f); break;
                        // Rotation
                        case HandleType::H_Rotate:
                        {
                            float a0           = atan2f(handleDragStartMouse_.y - P.y, handleDragStartMouse_.x - P.x);
                            float a1           = atan2f(worldMouse.y - P.y, worldMouse.x - P.x);
                            float newGlobalRot = handleDragStartRot_ + (a1 - a0) * RAD2DEG;
                            float parentRot    = sel->parent ? sel->parent->getGlobalRotation() : 0.0f;
                            sel->rotation      = newGlobalRot - parentRot;
                            isDirty            = true;
                            break;
                        }
                        default: break;
                    }
                }

                // 5c. Release
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
                {
                    draggingEntity = nullptr;
                    activeHandle_  = HandleType::None;
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
                            ::Rectangle r = {x, y, w, h};
                            multiSelection_.clear();
                            for (int i = 0; i < (int)scene.entities.size(); i++) { if (CheckCollisionRecs(r, scene.entities[i]->getBounds())) multiSelection_.push_back(i); }
                            if (!multiSelection_.empty()) selectedIndex = multiSelection_.back();
                            else selectedIndex = -1;
                        }
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
                    ImGui::TextDisabled("Entity: %s", contextEntity->name.c_str());
                    ImGui::Separator();

                    {
                        bool hm = multiSelection_.size() > 1;
                        if (ImGui::MenuItem("Cut", "Ctrl+X"))
                        {
                            TakeSnapshot();
                            CopySelected();
                            if (hm)
                            {
                                DeleteEntitiesAt(multiSelection_);
                                multiSelection_.clear();
                            }
                            else DeleteEntity(*contextEntity);
                            contextEntityIndex = -1;
                        }
                        if (ImGui::MenuItem("Copy", "Ctrl+C")) CopySelected();
                        bool canPasteV = !entityClipboard.is_null() || !multiClipboard_.empty();
                        if (ImGui::MenuItem("Paste", "Ctrl+V", false, canPasteV)) PasteAt(Vector2Add(contextEntity->position, {16.0f, 16.0f}));
                        if (ImGui::MenuItem("Duplicate", "Ctrl+D"))               DuplicateSelected(contextEntityIndex);
                        ImGui::Separator();
                        if (ImGui::MenuItem("Delete", "Del"))
                        {
                            TakeSnapshot();
                            if (hm)
                            {
                                DeleteEntitiesAt(multiSelection_);
                                multiSelection_.clear();
                            }
                            else DeleteEntity(*contextEntity);
                        }
                    }
                }
                else
                {
                    if (ImGui::BeginMenu("2D Object"))
                    {
                        if (ImGui::MenuItem(ICON_FA_CIRCLE "  Circle"))           CreateEntityAt("Circle",    worldMouse);
                        if (ImGui::MenuItem(ICON_FA_VECTOR_SQUARE "  Rectangle")) CreateEntityAt("Rectangle", worldMouse);
                        if (ImGui::MenuItem(ICON_FA_LAYER_GROUP "  Surface"))     CreateEntityAt("Surface",   worldMouse);
                        if (ImGui::MenuItem(ICON_FA_IMAGE "  Image (Sprite)"))    CreateEntityAt("Sprite",    worldMouse);
                        if (ImGui::MenuItem(ICON_FA_CAMERA "  Camera"))   CreateEntityAt("Camera",    worldMouse);
                        ImGui::EndMenu();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Paste", "Ctrl+V", false, !entityClipboard.is_null() || !multiClipboard_.empty())) PasteAt(worldMouse);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Deselect All", nullptr, false, selectedIndex >= 0)) selectedIndex = -1;
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

        const bool inPlay   = (state == GameState::Play || state == GameState::Pause);
        const bool inEditor = (state == GameState::Editor);

        // LIVE mode banner
        if (inPlay)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.30f, 0.30f, 0.030f, 1.0f));
            ImGui::BeginChild("##livebanner", ImVec2(-1, 28),false);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.2f, 1.0f),"  " ICON_FA_CIRCLE_PLAY "  LIVE — edits apply now, discarded on Stop");
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (selectedIndex != -1 && selectedIndex < (int)scene.entities.size() && inEditor)
        {
            Entity* inspected = scene.entities[selectedIndex].get();
            inspected->pendingRemoveComponentIndex = -1;

            ImGui::PushID(inspected);
            inspected->inspect([this]() { TakeSnapshot(); });
            ImGui::PopID();

            // Block component removal during play to avoid simulation crashes
            if (inspected->pendingRemoveComponentIndex != -1 && !inPlay)
            {
                TakeSnapshot();
                inspected->removeComponent(inspected->pendingRemoveComponentIndex);
                inspected->pendingRemoveComponentIndex = -1;
            }
            else { inspected->pendingRemoveComponentIndex = -1; }

            ImGui::Separator();
            if (inPlay) ImGui::BeginDisabled();
            if(ImGui::Button("Add Component", ImVec2(-1, 0))) ImGui::OpenPopup("Component Popup");
            if (inPlay) ImGui::EndDisabled();
            if (inPlay && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) { ImGui::SetTooltip("Stop simulation to add components"); }

            if(ImGui::BeginPopup("Component Popup"))
            {
                ImGui::TextDisabled("Rendering");
                if(ImGui::MenuItem("Shape Renderer"))    { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<ShapeRendererComponent>(); }
                if(ImGui::MenuItem("Sprite Renderer"))   { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<SpriteRendererComponent>(); }
                if(ImGui::MenuItem("Text Renderer"))     { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<TextRendererComponent>(); }
                if(ImGui::MenuItem("Particle System"))   { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<ParticleSystemComponent>(); }
                if(ImGui::MenuItem("Tilemap"))           { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<TilemapComponent>(); }
                ImGui::Separator();
                ImGui::TextDisabled("Collision");
                if(ImGui::MenuItem("Box Collider 2D"))    { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<BoxCollider2D>(); }
                if(ImGui::MenuItem("Circle Collider 2D")) { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<CircleCollider2D>(); }
                ImGui::Separator();
                ImGui::TextDisabled("Audio");
                if(ImGui::MenuItem("Audio Source"))  { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<AudioSourceComponent>(); }
                ImGui::Separator();
                ImGui::TextDisabled("Physics & Logic");
                if(ImGui::MenuItem("Add Rigidbody")) { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<RigidbodyComponent>(); }
                if(ImGui::MenuItem("Add Bouncer"))   { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<BouncerComponent>(); }
                if(ImGui::MenuItem("Add Camera"))    { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<CameraComponent>(); }
                if(ImGui::MenuItem("Add Trigger"))   { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<TriggerComponent>(); }
                if(ImGui::MenuItem("Add Animator"))  { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<AnimatorComponent>(); }
                ImGui::Separator();
                ImGui::TextDisabled("Narrative");
                if(ImGui::MenuItem("Interactable"))      { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<InteractableComponent>(); }
                if(ImGui::MenuItem("Player Interactor")) { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<PlayerInteractorComponent>(); }

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
        std::function<void(Entity&)> doDelete = [&](Entity& ent)
        {
            // Recursively delete all children
            std::vector<Entity*> childrenCopy = ent.children;
            for (Entity* child : childrenCopy) { doDelete(*child); }

            auto it = std::find_if(scene.entities.begin(), scene.entities.end(),
            [&](const std::unique_ptr<Entity>& e) { return e.get() == &ent; });

            if (it != scene.entities.end())
            {
                // Notify components before the entity is destroyed (matches Scene::Update
                // and Scene::Restore so OnDestroy hooks and subscription teardown run).
                for (auto& comp : ent.components) comp->destroy(&scene);

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

    inline void Editor::DeleteEntitiesAt(const std::vector<int>& indices)
    {
        // Resolve to pointers up front: DeleteEntity cascades to children that may
        // sit at lower indices, so positions become stale after the first erase.
        std::vector<Entity*> targets;
        targets.reserve(indices.size());
        for (int i : indices)
            if (i >= 0 && i < (int)scene.entities.size())
                targets.push_back(scene.entities[i].get());

        for (Entity* p : targets)
        {
            // A selected child may already be gone as a descendant of a selected
            // parent. Check presence by pointer (no deref) before deleting.
            bool present = std::any_of(scene.entities.begin(), scene.entities.end(),
                [&](const std::unique_ptr<Entity>& e) { return e.get() == p; });
            if (present) DeleteEntity(*p);
        }
    }

    inline void Editor::TakeSnapshot()
    {
        if (state == GameState::Play) return; // Don't record undo states during gameplay

        undoStack.push_back(scene.serialize());
        if (undoStack.size() > MaxUndoSteps) { undoStack.erase(undoStack.begin()); }
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

        if (prevState.contains("editorCamera"))
        {
            editorCamera.target.x = prevState["editorCamera"][0].get<float>();
            editorCamera.target.y = prevState["editorCamera"][1].get<float>();
            editorCamera.zoom     = prevState["editorCamera"][2].get<float>();
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
        scene.storyState = prevState.contains("storyState") ? StoryValueMapFromJson(prevState["storyState"]) : std::map<std::string, StoryValue>{};
        scene.LoadParallaxFromJson(prevState);

        selectedIndex = -1;
        multiSelection_.clear();
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

        if (nextState.contains("editorCamera"))
        {
            editorCamera.target.x = nextState["editorCamera"][0].get<float>();
            editorCamera.target.y = nextState["editorCamera"][1].get<float>();
            editorCamera.zoom     = nextState["editorCamera"][2].get<float>();
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
        scene.storyState = nextState.contains("storyState") ? StoryValueMapFromJson(nextState["storyState"]): std::map<std::string, StoryValue>{};
        scene.LoadParallaxFromJson(nextState);
        selectedIndex = -1;
        multiSelection_.clear();
    }

    inline void Editor::ShowContentBrowser()
    {
        if (!pm.IsProjectOpen()) { ImGui::TextDisabled("No project open."); return; }

        static std::string selectedFolder;
        static std::string lastKnownProjectPath;
        static std::string cbCachedFolder;
        static std::vector<fs::directory_entry> cbCachedEntries;

        if (lastKnownProjectPath != pm.GetCurrentProjectPath())
        {
            lastKnownProjectPath = pm.GetCurrentProjectPath();
            selectedFolder = pm.GetCurrentProjectPath();
            cbCachedFolder.clear();
        }

        float availH = ImGui::GetContentRegionAvail().y;
        static float treeWidth = 250.0f;
        static float treeMinWidth = 200.0f;
        if(treeWidth <= treeMinWidth) treeWidth = treeMinWidth;
        // --- Left Side: Folder Tree ---
        ImGui::BeginChild("FolderTree", ImVec2(treeWidth, availH), true, ImGuiWindowFlags_HorizontalScrollbar);
        std::function<void(const fs::path&, bool)> DrawFolderTree;
        DrawFolderTree = [&](const fs::path& path, bool isRoot)
        {
            std::string name = path.filename().string();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (selectedFolder == path.string()) flags |= ImGuiTreeNodeFlags_Selected;
            if (isRoot) flags |= ImGuiTreeNodeFlags_DefaultOpen;

            bool hasSubdirs = false;
            if (fs::exists(path))
            {
                for (auto& entry : fs::directory_iterator(path)) { if (entry.is_directory()) { hasSubdirs = true; break; } }
            }
            if (!hasSubdirs) flags |= ImGuiTreeNodeFlags_Leaf;

            std::string label = std::string(ICON_FA_FOLDER "  ") + name;
            bool open = ImGui::TreeNodeEx(label.c_str(), flags);
            if (ImGui::IsItemClicked()) selectedFolder = path.string();

            if (open)
            {
                if (fs::exists(path)) { for (auto& entry : fs::directory_iterator(path)) { if (entry.is_directory()) DrawFolderTree(entry.path(), false); } }
                ImGui::TreePop();
            }
        };
        DrawFolderTree(pm.GetCurrentProjectPath(), true);
        ImGui::EndChild();

        // --- Resize Handle ---
        ImGui::SameLine();
        ImGui::Button("##treeSplitter", ImVec2(4.0f, availH));
        if (ImGui::IsItemActive()) treeWidth += ImGui::GetIO().MouseDelta.x;
        treeWidth = std::clamp(treeWidth, 100.0f, 400.0f);
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        ImGui::SameLine();

        // --- Right Side: Content View ---
        ImGui::BeginChild("ContentView", ImVec2(0, availH), false);

        // Navigation bar & Actions
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
        if (ImGui::Button(ICON_FA_PLUS "  Create Script")) ImGui::OpenPopup("Create A New Script");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ARROWS_ROTATE "  Refresh")) cbCachedFolder.clear();
        ImGui::PopStyleVar();

        ImGui::SameLine();
        if (selectedFolder != pm.GetCurrentProjectPath())
        {
            if (ImGui::Button(ICON_FA_ARROW_LEFT)) {
                fs::path p(selectedFolder);
                if (p.has_parent_path() && p.parent_path().string() >= pm.GetCurrentProjectPath()) selectedFolder = p.parent_path().string();
            }
            ImGui::SameLine();
        }
        ImGui::TextDisabled(ICON_FA_FOLDER_OPEN "  %s", fs::relative(selectedFolder, pm.GetCurrentProjectPath()).string().c_str());
        ImGui::Separator();

        // New Script Modal
        if (ImGui::BeginPopupModal("Create A New Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static char scriptName[64] = "NewScript";
            ImGui::InputText("Script Name", scriptName, 64);

            auto scriptNameOk = [](const char* s)
            {
                std::string n(s);
                if (n.empty() || n.find_first_not_of(" \t") == std::string::npos) return false;
                if (n.find('/') != std::string::npos || n.find('\\') != std::string::npos) return false;
                if (n.find("..") != std::string::npos) return false;
                // C++ class name: must start with letter/underscore, only alnum + underscore
                if (!std::isalpha((unsigned char)n[0]) && n[0] != '_') return false;
                for (char c : n) if (!std::isalnum((unsigned char)c) && c != '_') return false;
                return true;
            };
            bool snameOk = scriptNameOk(scriptName);
            if (!snameOk) ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION "  Must be a valid C++ identifier.");

            ImGui::Spacing();
            if (!snameOk) ImGui::BeginDisabled();
            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                std::string sName(scriptName);
                std::string projectPath = pm.GetCurrentProjectPath();
                std::string scriptDir = projectPath + "/scripts";

                // Ensure scripts directory exists
                if (!fs::exists(scriptDir)) fs::create_directories(scriptDir);

                std::string filePath = scriptDir + "/" + sName + ".cpp";
                if (!fs::exists(filePath))
                {
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
                    cbCachedFolder.clear();
                }
                ImGui::CloseCurrentPopup();
            }
            if (!snameOk) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        // --- Grid: Unity-style compact tiles ---
        float cellSize = 80.0f;
        float itemSpacing = 6.0f;
        float gridWidth = ImGui::GetContentRegionAvail().x;
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Rebuild directory listing only when the selected folder changes.
        if (selectedFolder != cbCachedFolder)
        {
            cbCachedFolder = selectedFolder;
            cbCachedEntries.clear();
            if (fs::exists(selectedFolder)) for (auto& e : fs::directory_iterator(selectedFolder)) cbCachedEntries.push_back(e);
        }

        {
            int itemIdx = 0;
            for (const auto& entry : cbCachedEntries)
            {
                auto path = entry.path();
                std::string name = path.filename().string();

                // Flow layout: place items side by side, wrap when needed
                float nextX = (cellSize + itemSpacing) * itemIdx;
                int perRow = (int)(gridWidth / (cellSize + itemSpacing));
                if (perRow < 1) perRow = 1;
                if (itemIdx % perRow != 0) ImGui::SameLine(0.0f, itemSpacing);
                ImGui::PushID(name.c_str());
                bool isDir = entry.is_directory();
                const char* icon = ICON_FA_FILE;
                ImVec4 iconColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                std::string ext = path.extension().string();

                if (isDir)                                { icon = ICON_FA_FOLDER;    iconColor = ImVec4(0.95f, 0.75f, 0.2f, 1.0f); }
                else if (ext == ".cpp" || ext == ".hpp")  { icon = ICON_FA_FILE_CODE; iconColor = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);  }
                else if (ext == ".scene")                 { icon = ICON_FA_MAP;       iconColor = ImVec4(0.4f, 0.8f, 0.4f, 1.0f);  }
                else if (ext == ".png" || ext == ".jpg")  { icon = ICON_FA_IMAGE;     iconColor = ImVec4(0.8f, 0.5f, 0.9f, 1.0f);  }
                else if (ext == ".prefab")                { icon = ICON_FA_BOX;       iconColor = ImVec4(1.0f, 0.75f, 0.3f, 1.0f);  }

                // --- Render tile with InvisibleButton (no BeginChild overhead) ---
                ImVec2 tileSize = ImVec2(cellSize, cellSize + 16.0f);
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 p1 = ImVec2(p0.x + tileSize.x, p0.y + tileSize.y);

                ImGui::InvisibleButton("##tile", tileSize);
                bool hovered = ImGui::IsItemHovered();
                bool clicked = ImGui::IsItemClicked();

                // --- Drag and Drop Source ---
                if (!isDir && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    std::string relPath = fs::relative(path, pm.GetCurrentProjectPath()).string();
                    ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", relPath.c_str(), relPath.size() + 1);
                    ImGui::Text("%s %s", icon, name.c_str());
                    ImGui::EndDragDropSource();
                }

                // Hover background
                if (hovered) drawList->AddRectFilled(p0, p1, ImColor(1.0f, 1.0f, 1.0f, 0.06f), 4.0f);

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
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && !isDir && ext == ".prefab" && pm.IsProjectOpen() && state == GameState::Editor)
                {
                    nlohmann::json j = PrefabManager::Load(path.string());
                    if (!j.is_null())
                    {
                        TakeSnapshot();
                        auto e = factory.LoadEntity(j);
                        if (e)
                        {
                            e->id       = scene.nextEntityId++;
                            e->position = editorCamera.target;
                            scene.entities.push_back(std::move(e));
                            selectedIndex = (int)scene.entities.size() - 1;
                        }
                    }
                }

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

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
    }

    inline void Editor::ShowStoryState()
    {
        if (!pm.IsProjectOpen()) { ImGui::TextDisabled("No project open."); return; }
        const bool playing = (state == GameState::Play);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        ImGui::TextDisabled("   " ICON_FA_FLAG "  %s", playing ? "Runtime Blackboard (live — discarded on Stop)" : "Authored Scene Flags (seeded into the blackboard on Play)");
        ImGui::PopStyleVar();
        ImGui::Separator();

        // Snapshot the entries so the backing store can be mutated while iterating.
        std::vector<std::pair<std::string, StoryValue>> entries;
        if (playing) {for (const auto& kv : StoryState::Get().Values()) entries.emplace_back(kv.first, kv.second);}
        else {for (const auto& kv : scene.storyState) entries.emplace_back(kv.first, kv.second);}

        const float addRowH = ImGui::GetFrameHeightWithSpacing() + 8.0f;
        ImGui::BeginChild("StoryStateScroll", ImVec2(0, -addRowH), false);

        if (entries.empty()) { ImGui::TextDisabled("(No story variables defined)"); }
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
                std::visit([&](auto&& arg)
                {
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

                if (playing) { if (changed) StoryState::Get().Set(key, value); }
                else { if (activated) TakeSnapshot(); if (changed)   scene.storyState[key] = value; }
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

    inline void Editor::ShowParallax()
    {
        if (!pm.IsProjectOpen()) { ImGui::TextDisabled("No project open."); return; }

        // --- Master toggles ---
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
        bool enabled = scene.parallaxEnabled;
        if (ImGui::Checkbox("Use parallax in this scene", &enabled))
        {
            TakeSnapshot();
            scene.parallaxEnabled = enabled;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(scene-level — persisted)");

        if (!scene.parallaxEnabled) ImGui::BeginDisabled();
        ImGui::Checkbox("Show in Scene tab (preview)", &parallaxPreviewSceneTab_);
        ImGui::SameLine();
        ImGui::TextDisabled("(editor-only — Play/Game always honors the scene toggle)");
        if (!scene.parallaxEnabled) ImGui::EndDisabled();
        // Preview shifts layers visually, but picking/drag still resolve through the
        // unshifted editor camera — so clicking a non-zero layer won't line up. Warn.
        if (scene.parallaxEnabled && parallaxPreviewSceneTab_)
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.20f, 1.0f),
                ICON_FA_TRIANGLE_EXCLAMATION " Preview is view-only: selection & drag still use world coords, so non-zero layers won't align with the cursor.");
        ImGui::PopStyleVar();

        ImGui::Separator();

        if (!scene.parallaxEnabled)
        {
            ImGui::TextDisabled("Enable parallax above to configure per-layer scroll rates.");
            return;
        }

        // --- Anchor ---
        // Layers coincide with their authored positions only when the camera is at
        // this point; set it to the camera's starting view so backgrounds don't jump
        // the moment Play begins.
        ImGui::TextDisabled("Anchor (camera 'rest' point — layers align here)");
        float anchor[2] = { scene.parallaxAnchor.x, scene.parallaxAnchor.y };
        ImGui::PushItemWidth(180.0f);
        const bool anchorChanged   = ImGui::DragFloat2("##plxAnchor", anchor, 1.0f);
        const bool anchorActivated = ImGui::IsItemActivated();
        ImGui::PopItemWidth();
        if (anchorActivated) TakeSnapshot();
        if (anchorChanged) { scene.parallaxAnchor = { anchor[0], anchor[1] }; }
        ImGui::SameLine();
        if (ImGui::Button("Use primary camera"))
        {
            // Anchor to the primary camera entity's authored position — its view at
            // scene start, which is exactly where backgrounds should line up.
            for (const auto& e : scene.entities)
            {
                bool isPrimaryCam = false;
                for (const auto& c : e->components)
                    if (auto* cam = dynamic_cast<CameraComponent*>(c.get())) { isPrimaryCam = cam->isPrimary; if (isPrimaryCam) break; }
                if (isPrimaryCam) { TakeSnapshot(); scene.parallaxAnchor = e->getGlobalPosition(); break; }
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the anchor to the primary camera's position");

        ImGui::Separator();

        // --- Compile the layer list ---
        // Active layers (used by some entity in the scene) + every override key,
        // so layers the user customized but isn't using yet still appear.
        std::set<int> layers;
        for (const auto& e : scene.entities) layers.insert(e->depthLayer);
        for (const auto& [k, v] : scene.parallaxByLayer) layers.insert(k);
        layers.insert(0); // always show the reference row

        ImGui::TextDisabled("Backgrounds (negative depth) scroll slower; foregrounds (positive) scroll faster. Layer 0 is the reference and always 1.0.");
        ImGui::Spacing();

        const float addRowH = ImGui::GetFrameHeightWithSpacing() + 8.0f;
        ImGui::BeginChild("ParallaxScroll", ImVec2(0, -addRowH), false);

        if (ImGui::BeginTable("ParallaxTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Layer",  ImGuiTableColumnFlags_WidthFixed,   72.0f);
            ImGui::TableSetupColumn("Factor", ImGuiTableColumnFlags_WidthStretch, 0.55f);
            ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed,   90.0f);
            ImGui::TableSetupColumn("##rm",   ImGuiTableColumnFlags_WidthFixed,   28.0f);
            ImGui::TableHeadersRow();

            for (int layer : layers)
            {
                ImGui::TableNextRow();
                ImGui::PushID(layer);

                const bool isLockedRef = (layer == 0);
                const bool hasOverride = !isLockedRef && scene.HasParallaxOverride(layer);
                const float currentVal = scene.ParallaxFactor(layer);

                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%d", layer);

                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                float editVal = currentVal;
                if (isLockedRef) ImGui::BeginDisabled();
                // Snapshot-before-mutate (matches the Story State panel) so undo
                // restores the pre-drag value even on a click-and-drag same frame.
                const bool factorChanged   = ImGui::DragFloat("##plxFactor", &editVal, 0.01f, 0.0f, 5.0f, "%.2f");
                const bool factorActivated = ImGui::IsItemActivated();
                if (isLockedRef) ImGui::EndDisabled();
                ImGui::PopItemWidth();
                if (factorActivated) TakeSnapshot();
                if (factorChanged)   scene.SetParallaxFactor(layer, editVal);

                ImGui::TableSetColumnIndex(2);
                if (isLockedRef)        ImGui::TextDisabled("reference");
                else if (hasOverride)   ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.20f, 1.0f), "override");
                else                    ImGui::TextDisabled("default");

                ImGui::TableSetColumnIndex(3);
                if (hasOverride)
                {
                    if (ImGui::SmallButton(ICON_FA_ROTATE_LEFT "##reset"))
                    {
                        TakeSnapshot();
                        scene.ResetParallaxFactor(layer);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset to formula default");
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::EndChild();

        // --- Add unused layer row ---
        ImGui::Separator();
        ImGui::TextDisabled("Add:");
        ImGui::SameLine();
        ImGui::PushItemWidth(80.0f);
        // Changing the layer prefills the factor with that layer's formula default —
        // this is the "default on layer creation" behavior; the user can then tweak it.
        if (ImGui::InputInt("layer##plxnew", &newParallaxLayer_, 1, 1))
        {
            newParallaxFactor_ = Scene::DefaultParallaxFactor(newParallaxLayer_);
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::TextDisabled("formula %.2f", Scene::DefaultParallaxFactor(newParallaxLayer_));
        ImGui::SameLine();
        ImGui::PushItemWidth(80.0f);
        ImGui::DragFloat("factor##plxnew", &newParallaxFactor_, 0.01f, 0.0f, 5.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Add##plxnew"))
        {
            if (newParallaxLayer_ != 0)
            {
                TakeSnapshot();
                scene.SetParallaxFactor(newParallaxLayer_, newParallaxFactor_);
            }
        }
    }

    inline void Editor::ShowInputManager()
    {
        ImGui::SetNextWindowSize(ImVec2(480, 420), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(200, 200), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin(ICON_FA_GAMEPAD "  Input Action Map", &showInputManager_, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::End();
            return;
        }

        ImGui::TextDisabled("Define named actions that scripts query via InputManager::Get().IsPressed(\"Jump\")");
        ImGui::Spacing();

        // --- Key capture overlay ---
        if (capturingKey_)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.08f, 0.05f, 1.0f));
            ImGui::BeginChild("##capture", ImVec2(-1, 48), true);
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), ICON_FA_KEYBOARD "  Press any key or mouse button for action \"%s\"  (Esc = cancel)", capturingAction_.c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();

            // Detect pressed key
            int pressed = GetKeyPressed();
            if (pressed == KEY_ESCAPE) { capturingKey_ = false; }
            else if (pressed > 0)
            {
                InputManager::Get().SetAction(capturingAction_, pressed);
                capturingKey_ = false;
                if (pm.IsProjectOpen()) InputManager::Get().Save(pm.GetCurrentProjectPath() + "/input.json");
            }
            else
            {
                // Check mouse buttons
                for (int mb = 0; mb < 5; mb++)
                {
                    if (IsMouseButtonPressed((MouseButton)mb))
                    {
                        InputManager::Get().SetMouseAction(capturingAction_, mb);
                        capturingKey_ = false;
                        if (pm.IsProjectOpen()) InputManager::Get().Save(pm.GetCurrentProjectPath() + "/input.json");
                        break;
                    }
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
        }

        // --- Action table ---
        auto& actions = InputManager::Get().GetActions();

        // Collect sorted list for stable display order
        std::vector<std::string> actionNames;
        actionNames.reserve(actions.size());
        for (auto& [name, _] : actions) actionNames.push_back(name);
        std::sort(actionNames.begin(), actionNames.end());
        std::string toRemove;

        if (!actionNames.empty() && ImGui::BeginTable("##ActTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,ImVec2(0, -60.0f)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Action",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("##ops",   ImGuiTableColumnFlags_WidthFixed,  84.0f);
            ImGui::TableHeadersRow();

            for (const auto& name : actionNames)
            {
                auto it = actions.find(name);
                if (it == actions.end()) continue;
                const ActionBinding& b = it->second;

                ImGui::TableNextRow();
                ImGui::PushID(name.c_str());

                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(name.c_str());

                ImGui::TableSetColumnIndex(1);
                std::string bindStr = InputManager::BindingName(b);
                bool isCapturing = (capturingKey_ && capturingAction_ == name);
                if (isCapturing) {ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "...");}
                else {ImGui::TextUnformatted(bindStr.c_str());}

                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton(isCapturing ? "Cancel" : "Rebind"))
                {
                    if (isCapturing) capturingKey_ = false;
                    else { capturingKey_      = true; capturingAction_   = name; }
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
                if (ImGui::SmallButton(ICON_FA_TRASH)) toRemove = name;
                ImGui::PopStyleColor();
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
        else if (actionNames.empty())
        {
            ImGui::TextDisabled("(no actions defined)");
            ImGui::Spacing();
        }

        if (!toRemove.empty())
        {
            InputManager::Get().RemoveAction(toRemove);
            if (pm.IsProjectOpen()) InputManager::Get().Save(pm.GetCurrentProjectPath() + "/input.json");
        }

        // --- Add new action row ---
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushItemWidth(180.0f);
        ImGui::InputText("##NewActionName", newActionNameBuf_, sizeof(newActionNameBuf_));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        bool canAdd = newActionNameBuf_[0] != '\0' && !InputManager::Get().HasAction(newActionNameBuf_);
        if (!canAdd) ImGui::BeginDisabled();
        if (ImGui::Button("Add Action"))
        {
            InputManager::Get().SetAction(newActionNameBuf_, 0);

            capturingKey_        = true;
            capturingAction_     = newActionNameBuf_;
            newActionNameBuf_[0] = '\0';
            if (pm.IsProjectOpen()) InputManager::Get().Save(pm.GetCurrentProjectPath() + "/input.json");
        }
        if (!canAdd) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FLOPPY_DISK "  Save")) { if (pm.IsProjectOpen()) InputManager::Get().Save(pm.GetCurrentProjectPath() + "/input.json"); }

        ImGui::End();
    }

    inline void Editor::CopySelected()
    {
        if (multiSelection_.size() > 1)
        {
            multiClipboard_.clear();
            for (int i : multiSelection_) multiClipboard_.push_back(scene.entities[i]->serialize());
            entityClipboard = {};
        }
        else if (selectedIndex >= 0 && selectedIndex < (int)scene.entities.size())
        {
            entityClipboard = scene.entities[selectedIndex]->serialize();
            multiClipboard_.clear();
        }
    }

    inline void Editor::PasteAt(Vector2 pos)
    {
        TakeSnapshot();
        if (!multiClipboard_.empty())
        {
            for (auto& j : multiClipboard_)
            {
                auto pasted = factory.LoadEntity(j);
                if (!pasted) continue;
                pasted->id       = scene.nextEntityId++;
                pasted->parentId = -1;
                pasted->position = Vector2Add(pasted->position, {16.0f, 16.0f});
                scene.entities.push_back(std::move(pasted));
            }
            selectedIndex = (int)scene.entities.size() - 1;
        }
        else if (!entityClipboard.is_null())
        {
            auto pasted = factory.LoadEntity(entityClipboard);
            if (!pasted) return;
            pasted->id       = scene.nextEntityId++;
            pasted->parentId = -1;
            pasted->position = pos;
            scene.entities.push_back(std::move(pasted));
            selectedIndex = (int)scene.entities.size() - 1;
        }
    }

    inline void Editor::DuplicateSelected(int index)
    {
        TakeSnapshot();
        if (multiSelection_.size() > 1)
        {
            std::vector<nlohmann::json> todupJ;
            for (int i : multiSelection_) todupJ.push_back(scene.entities[i]->serialize());
            for (auto& j : todupJ)
            {
                auto dup = factory.LoadEntity(j);
                if (dup) { dup->id = scene.nextEntityId++; dup->name += " (Copy)"; dup->position = Vector2Add(dup->position, {16.0f, 16.0f}); scene.entities.push_back(std::move(dup)); }
            }
        }
        else if (index >= 0 && index < (int)scene.entities.size())
        {
            Entity* ent = scene.entities[index].get();
            auto dup = factory.LoadEntity(ent->serialize());
            if (dup)
            {
                dup->id       = scene.nextEntityId++;
                dup->name     = ent->name + " (Copy)";
                dup->position = Vector2Add(dup->position, {16.0f, 16.0f});
                scene.entities.push_back(std::move(dup));
                scene.entities.back()->setParent(ent->parent);
                selectedIndex = (int)scene.entities.size() - 1;
            }
        }
    }
}
