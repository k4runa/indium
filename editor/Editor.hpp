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
#include "../2D/component/Light2DComponent.hpp"
#include "../2D/component/InteractableComponent.hpp"
#include "../2D/component/PlayerInteractorComponent.hpp"
#include "../2D/component/AudioListenerComponent.hpp"
#include "../2D/component/Joint2D.hpp"
#include "../2D/component/SortingGroup.hpp"
#include "../2D/component/PathFollowerComponent.hpp"
#include "../2D/component/FlipComponent.hpp"
#include "../2D/component/TimerComponent.hpp"
#include "../2D/component/LineRendererComponent.hpp"
#include "../2D/component/AreaEffect2DComponent.hpp"
#include "../2D/component/NavigationAgent2DComponent.hpp"
#include "../2D/component/PostProcessComponent.hpp"
#include "../2D/component/TrailRendererComponent.hpp"
#include "../2D/component/SpawnPointComponent.hpp"
#include "../2D/component/CheckpointComponent.hpp"
#include "../2D/component/PhysicsMaterial2DComponent.hpp"
#include "../2D/component/NavigationRegion2DComponent.hpp"
#include "../2D/component/DecalComponent.hpp"
#include "../2D/component/SpriteSheetComponent.hpp"
#include "PostProcessManager.hpp"
#include "../core/scene/Scene.hpp"
#include "../core/StoryState.hpp"
#include "../core/PrefabManager.hpp"
#include "../2D/entity/EntityFactory.hpp"
#include <vector>
#include <deque>
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
#include <future>
#include <chrono>
#include <functional>
#include <cctype>

#include "Launcher.hpp"
#include "../core/ProjectManager.hpp"
#include "../core/ScriptManager.hpp"
#include "../core/InputManager.hpp"
#include "../core/Screen.hpp"
#include "../core/DialogueManager.hpp"
#include "../core/QuestManager.hpp"
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

        /** @brief Project-relative scene path captured at Play start and restored on Stop.
         *  A script-driven LoadScene during Play repoints ProjectManager's current scene;
         *  without this, Stop restores the original snapshot but Save would then write it
         *  over the switched-to scene file. */
        std::string         prePlayScenePath_;

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

        /** @brief Off-screen light accumulation buffer, kept the same size as `viewport`.
         *  Built by RenderLightMap() when Scene::lightingEnabled, then composited over the
         *  rendered world with BLEND_MULTIPLIED. */
        RenderTexture2D     lightMap_;

        /** @brief Per-light scratch buffer (same size as lightMap_). Each Point/Spot light is
         *  drawn here first so its shadows can be subtracted from that light alone, then the
         *  result is added into lightMap_. */
        RenderTexture2D     lightScratch_;

        /** @brief A radial white→transparent gradient, generated once in Init(). Each light
         *  draws this additively, tinted by its color/intensity, to splat a soft pool of light. */
        Texture2D           lightGradient_ = { 0 };

        /** @brief Screen-space post-processing (shader effects) applied to the viewport
         *  after the scene + lighting pass. Driven by PostProcessComponents in the scene. */
        PostProcessManager  postFx_;

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

        /** @brief Regression guard for the runtime screen-space UI pass (Editor::DrawRuntimeUI):
         *  a Play-frame counter + one-shot flag so we can warn once if that pass ever stops
         *  populating Screen — i.e. a viewport/docking refactor dropped it again (it has
         *  happened twice). See DrawRuntimeUI's banner. */
        int                 playUiCheckFrames_   = 0;
        bool                warnedUiPassMissing_ = false;

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
        std::deque<nlohmann::json> undoStack;
        std::deque<nlohmann::json> redoStack;
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

        /** @brief Add Component popup search filter. */
        char                componentSearchBuf_[64] = {};

        /** @brief Hierarchy / Content Browser search filters. */
        char                hierarchySearchBuf_[64]  = {};
        char                contentSearchBuf_[128]   = {};

        /** @brief In-place rename state for the Hierarchy (F2 / double-click). */
        int                 renamingEntityId_   = -1;
        char                entityRenameBuf_[128] = {};
        bool                renameFocusPending_ = false;

        /** @brief Rotation snap (degrees) applied when snap is enabled. */
        float               rotSnapDegrees_ = 15.0f;

        /** @brief Set by Window > Reset Layout to rebuild the default dock layout. */
        bool                resetDockLayout_ = false;

        // --- Transient toast notifications (top-right, auto-fade) ---
        struct Toast
        {
            std::string text;
            ImVec4      color;
            double      born;      // GetTime() when created
            float       life;      // seconds before fade-out completes
        };
        std::vector<Toast>  toasts_;
        void PushToast(const std::string& text, ImVec4 color = ImVec4(0.4f, 0.8f, 0.4f, 1.0f), float life = 3.0f);
        void DrawToasts();

        /** @brief Scene management modal state. */
        std::string         sceneRenameTarget;
        std::string         sceneDeleteTarget;
        char                sceneRenameBuffer[64] = {};
        bool                showNewSceneModal_    = false;
        bool                showRenameSceneModal_ = false;
        bool                showDeleteSceneModal_ = false;

        /** @brief Bottom panel height and visibility. */
        float               bottomPanelHeight     = 350.0f;    // 350 by default
        float               bottomPanelMaxHeight  = 500.0f;
        bool                showBottomPanel       = true;
        bool                isResizingBottom      = false;

        /** @brief Side panel widths and resize state. */
        float               hierarchyWidth        = 350.0f;
        float               hierarchyMaxWidth     = 600.0f;
        float               inspectorWidth        = 400.0f;
        float               inspectorMaxWidth     = 600.0f;
        bool                isResizingHierarchy   = false;
        bool                isResizingInspector   = false;

        // --- Dirty / Save changes state ---
        bool                isDirty                 = false;
        bool                showUnsavedChangesPopup = false;
        bool                wantsToExit             = false;
        bool                wantsToExitToLauncher   = false;
        bool                shouldExitImmediately   = false;

        // --- Async script compilation (Compile & Reload) ---
        // The compile (running g++) takes seconds; doing it inline froze the whole
        // editor with no feedback. We run it on a worker thread and show a modal
        // spinner. The thread ONLY compiles (process + file I/O — safe off-thread);
        // the library reload + scene rehydrate happen back on the main thread in
        // Update(), since they touch ImGui/raylib/scene state.
        std::future<bool>   scriptCompileFuture_;
        std::string         scriptCompileLog_;
        bool                scriptCompileRunning_   = false;
        void StartScriptCompile();      // kick off the worker + open the modal
        void PollScriptCompile();       // called each frame; finishes the reload
        void DrawScriptCompileModal();  // the "Compiling…" spinner popup

        // --- Deferred blocking operations ---
        // Synchronous work (project load, scene switch, save) is requested from
        // inside an ImGui frame, so we can't draw an overlay AND run the work in
        // the same frame. Instead we defer: RequestBlockingOp() stashes a closure
        // + label; the next frame draws the busy overlay; the frame after that runs
        // the closure (overlay already on screen) and clears it. RunDeferredBlockingOp()
        // is called once per frame from Update().
        std::function<void()> pendingBlockingOp_;
        std::string           pendingBlockingTitle_;
        std::string           pendingBlockingSubtitle_;
        int                   pendingBlockingDelay_ = 0;   // frames to show overlay before running
        void RequestBlockingOp(const std::string& title, const std::string& subtitle, std::function<void()> op);
        void RunDeferredBlockingOp();   // called each frame from Update()
        bool BusyOverlayActive() const; // true while compile OR a deferred op is pending

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

        // --- Dialogue editor panel (see editor/panels/DialoguePanel.cpp) ---
        // Working copy of the open dialogue/<dlgFile_>.json, held as an ordered vector
        // so authoring order is stable and node ids stay editable until Save (runtime
        // keys them into a map). Serialized back to disk on Save.
        std::vector<DialogueNode> dlgNodes_;
        std::string               dlgStart_;             // id of the start node
        std::string               dlgFile_;              // open dialogue name (file stem), "" = none
        char                      dlgNewNameBuf_[64] = {};
        bool                      dlgDirty_  = false;    // unsaved edits in the working copy
        bool                      dlgLoaded_ = false;    // a file (or new doc) is open
        // Deferred destructive action awaiting an unsaved-changes decision:
        // 0 = none, 1 = load dlgPendingArg_, 2 = new doc named dlgPendingArg_.
        int                       dlgPendingAction_ = 0;
        std::string               dlgPendingArg_;

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

        /** @brief Draws the in-game screen-space UI — script OnGUI() hooks, interact prompts,
         *  the quest-log overlay and the dialogue box — into the viewport texture during
         *  Play/Pause. MUST be called inside BeginTextureMode(viewport). Do not drop in refactors. */
        void DrawRuntimeUI();

        /** @brief Renders the property editor for the selected entity. */
        void ShowInspector();

        /** @brief Renders the project file system. */
        void ShowContentBrowser();

        /** @brief Renders engine and script logs. */
        void ShowConsole();

        /** @brief Renders the story flags/variables blackboard panel. */
        void ShowStoryState();

        /** @brief Renders the quest authoring / live-debug panel. */
        void ShowQuests();

        /** @brief Renders the dialogue authoring panel (edits dialogue/<name>.json). */
        void ShowDialogue();

        /** @brief Renders the per-depthLayer parallax configuration panel. */
        void ShowParallax();

        /** @brief Accumulates every active Light2DComponent into lightMap_ (cleared to the
         *  scene's ambient color). Call before BeginTextureMode(viewport); the result is
         *  then composited over the world with BLEND_MULTIPLIED. */
        void RenderLightMap(const Camera2D& cam);

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
        void ApplyHistoryState(std::deque<nlohmann::json>& from, std::deque<nlohmann::json>& to);

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

        void CreateEntityAt(const std::string& type, Vector2 pos);
        static std::string PrefsPath();
     };
}
