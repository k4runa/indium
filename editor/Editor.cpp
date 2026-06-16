#include "Editor.hpp"
#include "../core/LightingEnvironment.hpp"
#include "panels/BusyOverlay.hpp"

namespace Indium
{
    std::string Editor::PrefsPath()
    {
        const char* home = std::getenv("HOME");
        if (!home) home = std::getenv("USERPROFILE"); // Windows fallback
        return std::string(home ? home : ".") + "/.indium_prefs.json";
    }

    void Editor::CreateEntityAt(const std::string& type, Vector2 pos)
    {
        TakeSnapshot();
        std::unique_ptr<Entity> e;
        if      (type == "Circle")          e = factory.CreateCircle(scene);
        else if (type == "Rectangle")       e = factory.CreateRectangle(scene);
        else if (type == "Surface")         e = factory.CreatePlane(scene);
        else if (type == "Sprite")          e = factory.CreateSprite(scene);
        else if (type == "Camera")          e = factory.CreateCamera(scene);
        else if (type == "Empty")           e = factory.CreateEmpty(scene);
        else if (type == "Text")            e = factory.CreateText(scene);
        else if (type == "Light")           e = factory.CreateLight(scene);
        else if (type == "ParticleSystem")  e = factory.CreateParticleSystem(scene);
        else if (type == "Tilemap")         e = factory.CreateTilemap(scene);
        else if (type == "TriggerZone")     e = factory.CreateTriggerZone(scene);
        else if (type == "AudioSource")     e = factory.CreateAudioSource(scene);
        else if (type == "SpawnPoint")      e = factory.CreateSpawnPoint(scene);
        else if (type == "Checkpoint")      e = factory.CreateCheckpoint(scene);
        if (e) { e->position = pos; scene.entities.push_back(std::move(e)); selectedIndex = (int)scene.entities.size() - 1; isDirty = true; }
    }

    void Editor::PushToast(const std::string& text, ImVec4 color, float life)
    {
        toasts_.push_back({ text, color, GetTime(), life });
        if (toasts_.size() > 6) toasts_.erase(toasts_.begin());
    }

    void Editor::DrawToasts()
    {
        if (toasts_.empty()) return;

        // Drop expired toasts first.
        const double now = GetTime();
        toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
            [now](const Toast& t){ return (now - t.born) >= t.life; }), toasts_.end());
        if (toasts_.empty()) return;

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImDrawList*    dl = ImGui::GetForegroundDrawList();
        ImFont*        font = ImGui::GetFont();
        const float    fontSize = ImGui::GetFontSize();
        const float    pad = 12.0f, margin = 16.0f, gap = 8.0f;

        float y = vp->Pos.y + 48.0f;   // below the menu bar
        for (const auto& t : toasts_)
        {
            float age   = (float)(now - t.born);
            float fade   = 1.0f;
            const float fadeDur = 0.4f;
            if (age < fadeDur)              fade = age / fadeDur;             // fade in
            else if (age > t.life - fadeDur) fade = (t.life - age) / fadeDur; // fade out
            fade = (fade < 0) ? 0 : (fade > 1 ? 1 : fade);

            ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, t.text.c_str());
            float  w  = ts.x + pad * 2.0f;
            float  h  = ts.y + pad * 1.2f;
            float  x  = vp->Pos.x + vp->Size.x - w - margin;

            ImU32 bg     = ImGui::ColorConvertFloat4ToU32(ImVec4(0.10f, 0.10f, 0.11f, 0.95f * fade));
            ImU32 border  = ImGui::ColorConvertFloat4ToU32(ImVec4(t.color.x, t.color.y, t.color.z, 0.9f * fade));
            ImU32 textCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.92f, 0.92f, 0.92f, fade));

            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), bg, 6.0f);
            dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), border, 6.0f, 0, 1.5f);
            // accent stripe on the left
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 4.0f, y + h), border, 6.0f);
            dl->AddText(font, fontSize, ImVec2(x + pad, y + h * 0.5f - ts.y * 0.5f), textCol, t.text.c_str());

            y += h + gap;
        }
    }

    bool Editor::ShouldClose()
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

    void Editor::Init(const Config& config)
    {
        this->config = config;

        // Initialize with a dummy size; Run() will dynamically resize to fit the UI layout.
        viewport = LoadRenderTexture(1, 1);

        // 2D lighting pass (light map buffers + baked radial gradient).
        lighting_.Init();

        // Post-processing shader pipeline (scratch RTs + compiled effect shaders).
        postFx_.Init();

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

    void Editor::Shutdown()
    {
        SetTraceLogCallback(nullptr);
        s_consoleLogs = nullptr;
        AssetManager::Get().Clear();
        postFx_.Shutdown();
        UnloadRenderTexture(viewport);
        lighting_.Shutdown();
    }

    void Editor::RaylibTraceCallback(int level, const char* text, va_list args)
    {
        char buf[1024];
        vsnprintf(buf, sizeof(buf), text, args);

        // Chain into the session log file — SetTraceLogCallback replaced Logger's
        // own callback with this one, so without this call the durable log stops
        // capturing TraceLog output the moment the editor initializes.
        Logger::Mirror(level, buf);

        if (!s_consoleLogs) return;

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

        // Stage instead of pushing into consoleLogs directly: this callback fires
        // from worker threads too (script compile, async project create), and the UI
        // thread iterates consoleLogs while drawing the console panel.
        std::lock_guard<std::mutex> lock(s_pendingLogsMutex);
        s_pendingLogs.push_back({color, label, buf, icon});
    }

    void Editor::DrainPendingLogs()
    {
        std::lock_guard<std::mutex> lock(s_pendingLogsMutex);
        if (s_pendingLogs.empty()) return;
        consoleLogs.insert(consoleLogs.end(),
                           std::make_move_iterator(s_pendingLogs.begin()),
                           std::make_move_iterator(s_pendingLogs.end()));
        s_pendingLogs.clear();
        if (consoleLogs.size() > 2000) consoleLogs.erase(consoleLogs.begin(), consoleLogs.begin() + 500);
    }

    void Editor::Update(float dt)
    {
        // Move TraceLog messages staged by worker threads into the console (main
        // thread only — the console panel iterates consoleLogs while drawing).
        DrainPendingLogs();

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

        // Expose "actively ticking" to engine-layer code (inspector test affordances
        // etc.) — true only in Play, where scene.Update() runs below; not Pause/Edit,
        // and not while a menu (pause/title/settings) is up: menus freeze the world.
        const bool menuBlocked = MenuManager::Get().BlocksGameplay();
        Screen::Get().SetTicking(state == GameState::Play && !menuBlocked);

        if (state == GameState::Play)
        {
            // Drain deferred save/load work queued by the runtime UI last frame. This
            // runs even while a menu blocks gameplay — the save/load menu is by
            // definition open when one of these is pending.
            if (SaveManager::ConsumeAutosaveRequest())   // condition edge / RequestAutosave()
                SaveManager::Save(scene, SaveManager::AutosaveSlot());

            const MenuManager::MenuAction act = MenuManager::Get().TakePendingAction();
            if (act.type == MenuManager::MenuAction::Type::Save)
            {
                SaveManager::Save(scene, act.slot);
                MenuManager::Get().RefreshSlots();       // page stays open; show the new timestamp
            }
            else if (act.type == MenuManager::MenuAction::Type::Load)
            {
                if (SaveManager::Load(scene, act.slot))  // queues restore + _pendingSceneLoad
                {
                    MenuManager::Get().Reset();          // close the menu
                    DrainPendingSceneLoad();             // switch now — the restore must not wait
                                                         // a frame behind a live scene.Update
                }
            }
        }

        if (state == GameState::Play && !menuBlocked)
        {
            // Inject current viewport pixel size so CameraComponent bounds clamp is viewport-aware
            for (auto& e : scene.entities)
            {
                for (auto& c : e->components) {if (auto* cam = dynamic_cast<CameraComponent*>(c.get())) cam->viewportPx_ = {viewportSize.x, viewportSize.y};}
            }

            // Selection is index-based; entities destroyed during Update shift the
            // vector, silently moving the inspector onto a neighbor. Pin the selection
            // to entity ids across the update and remap (dropping ids that died).
            const bool hasSelection = selectedIndex >= 0 || !multiSelection_.empty();
            int selectedId = -1;
            std::vector<int> multiSelIds;
            if (hasSelection)
            {
                auto idAt = [&](int idx) { return (idx >= 0 && idx < (int)scene.entities.size()) ? scene.entities[idx]->id : -1; };
                selectedId = idAt(selectedIndex);
                multiSelIds.reserve(multiSelection_.size());
                for (int idx : multiSelection_) { int id = idAt(idx); if (id != -1) multiSelIds.push_back(id); }
            }

            scene.Update(dt);

            if (hasSelection)
            {
                auto indexOf = [&](int id) -> int
                {
                    if (id == -1) return -1;
                    for (int i = 0; i < (int)scene.entities.size(); ++i) if (scene.entities[i]->id == id) return i;
                    return -1;
                };
                selectedIndex = indexOf(selectedId);
                multiSelection_.clear();
                for (int id : multiSelIds) { int i = indexOf(id); if (i != -1) multiSelection_.push_back(i); }
            }

            // Tick the cutscene player AFTER the scene so a running cutscene has the final
            // say on the entities it drives this frame. It advances on the raw (unscaled) dt
            // so a cutscene that freezes gameplay (Time::scale=0) still plays.
            CutsceneManager::Get().Update(dt, &scene);

            // Drain script-requested scene transitions (NativeScript::LoadScene → scene._pendingSceneLoad).
            DrainPendingSceneLoad();
        }
        // Pause: no scene update, rendering continues

        // Regression guard for the runtime screen-space UI pass (Editor::DrawRuntimeUI): that
        // pass populates Screen every Play frame. A few frames into Play with Screen still
        // unpopulated means the pass was dropped again — warn once, loudly, with the fix.
        if (state == GameState::Play)
        {
            if (++playUiCheckFrames_ > 3 && Screen::Width() == 0 && !warnedUiPassMissing_)
            {
                warnedUiPassMissing_ = true;
                TraceLog(LOG_WARNING, "RUNTIME UI: the screen-space UI pass isn't running during Play "
                         "(Screen unpopulated) — Editor::DrawRuntimeUI() must be called inside the viewport "
                         "render. Dialogue, interact prompts, OnGUI HUDs and the quest log won't draw.");
            }
        }
        else playUiCheckFrames_ = 0;

        // Keep editor camera synced to scene so SaveCurrentProject always captures the latest view.
        if (state != GameState::Play && state != GameState::Pause)
        {
            scene.editorCameraTarget = editorCamera.target;
            scene.editorCameraZoom   = editorCamera.zoom;
        }

        // Cutscene editor preview: drive bound entities to the timeline pose without
        // permanently mutating the authored scene. The Cutscene panel sets
        // csPreviewKeepAlive_ every frame it runs; if it stops running (tab hidden) or we
        // leave Editor state, restore the snapshot so a preview pose is never baked in.
        if (csPreviewActive_ && (state != GameState::Editor || !csPreviewKeepAlive_))
        {
            csExitPreview();
        }
        else if (csPreviewActive_)
        {
            if (csPlaying_)
            {
                csPlayhead_ += dt;
                if (csPlayhead_ >= csDoc_.duration)
                {
                    if (csDoc_.loop && csDoc_.duration > 0.0f)
                        while (csPlayhead_ >= csDoc_.duration) csPlayhead_ -= csDoc_.duration;
                    else { csPlayhead_ = csDoc_.duration; csPlaying_ = false; }
                }
            }
            csSamplePreview();
        }
        csPreviewKeepAlive_ = false;

        // Finish an in-flight async script compile (reload happens on this thread).
        PollScriptCompile();
        // Run any deferred blocking op now that the overlay has had frames to show.
        RunDeferredBlockingOp();
    }

    // ── Async script compilation ───────────────────────────────────────────────
    // Compile & Reload used to run g++ inline, freezing the editor for seconds with
    // no feedback. Now StartScriptCompile() launches the compile on a worker thread
    // and flips scriptCompileRunning_; DrawScriptCompileModal() shows a spinner each
    // frame; PollScriptCompile() detects completion and does the (main-thread-only)
    // library reload + scene rehydrate.

    void Editor::DrainPendingSceneLoad()
    {
        if (scene._pendingSceneLoad.empty()) return;

        // Autosave the OUTGOING scene on every gameplay scene switch — but never when
        // the switch is itself a save-restore load (the pending restore means we're
        // about to discard this scene's state for the saved one; autosaving here would
        // clobber the autosave slot with pre-load data). Both call sites are Play-gated
        // and Play start never goes through here, so no GameState check is needed.
        if (SaveManager::AutosaveEnabled() && SaveManager::AutosaveOnSceneSwitch()
            && !scene._hasPendingRestore)
            SaveManager::Save(scene, SaveManager::AutosaveSlot());

        std::string target = scene._pendingSceneLoad;
        scene._pendingSceneLoad.clear();
        if (pm.SwitchScene(scene, target))
        {
            selectedIndex = -1;
            undoStack.clear();
            redoStack.clear();
        }
    }

    void Editor::StartScriptCompile()
    {
        if (scriptCompileRunning_) return;        // guard against double-start
        scriptCompileRunning_ = true;
        scriptCompileLog_.clear();

        const std::string projectPath = pm.GetCurrentProjectPath();
        Logger::Event("EDITOR", "Compile & Reload requested for project '%s'", projectPath.c_str());

        // Only the compile runs off-thread: it's a child process + file I/O and
        // touches no ImGui/raylib/scene state. The result string is captured into
        // the member so the main thread can read it once the future is ready.
        scriptCompileFuture_ = std::async(std::launch::async,
            [this, projectPath]() -> bool
            {
                return ScriptManager::Get().CompileScripts(projectPath, scriptCompileLog_);
            });
    }

    void Editor::PollScriptCompile()
    {
        // A reload deferred from Play/Pause runs as soon as we're back in Editor.
        if (scriptReloadPending_ && state == GameState::Editor)
        {
            scriptReloadPending_ = false;
            ReloadScriptsNow();
        }

        if (!scriptCompileRunning_) return;
        if (!scriptCompileFuture_.valid()) { scriptCompileRunning_ = false; return; }

        // Don't block: bail until the worker has actually finished.
        if (scriptCompileFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;

        const bool ok = scriptCompileFuture_.get();
        scriptCompileRunning_ = false;

        if (!ok)
        {
            consoleLogs.push_back({ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "[COMPILER ERROR]", scriptCompileLog_, ICON_FA_XMARK});
            PushToast("Script compile failed — see Console", ImVec4(0.9f, 0.3f, 0.3f, 1.0f), 5.0f);
            return;
        }

        // Never reload mid-Play: the rebuild clears scene.snapshot (Stop could no
        // longer restore the editor scene) and dlclose's the image that live script
        // instances still execute. The user may have pressed Play after starting
        // the compile — hold the reload until Stop returns us to Editor state.
        if (state != GameState::Editor)
        {
            scriptReloadPending_ = true;
            PushToast("Scripts compiled — reload after Stop", ImVec4(0.9f, 0.6f, 0.2f, 1.0f));
            return;
        }

        ReloadScriptsNow();
    }

    void Editor::ReloadScriptsNow()
    {
        // Reload must happen on the main thread: it swaps the loaded library and
        // rebuilds the scene. Live NativeScript instances point into the old image,
        // so snapshot to JSON first, tear them down, reload, then rehydrate fresh
        // instances against the new createFunc.
        nlohmann::json sceneState = scene.serialize();
        int prevSelected = selectedIndex;

        scene.entities.clear();
        scene.snapshot.clear();
        scene.startQueue.clear();
        scene.destroyQueue.clear();
        selectedIndex = -1;
        multiSelection_.clear();

        // Drop every event handler BEFORE the dylib swap. Handler std::functions
        // may hold code/captures living in the old image; destroying them after
        // dlclose (e.g. on the next Publish's deferred purge) would execute
        // unmapped code. Stop's teardown does the same — this path must too.
        EventBus::Get().Clear();

        ScriptManager::Get().LoadLibrary(pm.GetCurrentProjectPath());

        scene.nextEntityId = sceneState.value("nextEntityId", 1);
        if (sceneState.contains("entities"))
        {
            for (const auto& ej : sceneState["entities"]) { auto e = factory.LoadEntity(ej); if (e) scene.entities.push_back(std::move(e)); }
            scene.RebuildHierarchy();
        }
        if (prevSelected >= 0 && prevSelected < (int)scene.entities.size()) selectedIndex = prevSelected;
        consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[COMPILER]", scriptCompileLog_, ICON_FA_CHECK});
        PushToast("Scripts compiled & reloaded", ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
    }

    void Editor::DrawScriptCompileModal()
    {
        // Reuse the shared busy overlay so every blocking operation looks the same.
        // Drawn on the foreground draw list, so no popup bookkeeping is needed —
        // we simply draw it while the compile is running OR a deferred op is pending.
        if (scriptCompileRunning_)
            BusyOverlay::Draw("Compiling scripts", "Running the C++ compiler. This can take a few seconds.");
        else if (pendingBlockingOp_)
            BusyOverlay::Draw(pendingBlockingTitle_.c_str(),
                              pendingBlockingSubtitle_.empty() ? nullptr : pendingBlockingSubtitle_.c_str());
    }

    bool Editor::BusyOverlayActive() const
    {
        return scriptCompileRunning_ || (bool)pendingBlockingOp_;
    }

    void Editor::RequestBlockingOp(const std::string& title, const std::string& subtitle,
                                   std::function<void()> op)
    {
        // Stash the work; it runs a couple of frames later (see RunDeferredBlockingOp)
        // so the busy overlay is already on screen before the main thread blocks.
        pendingBlockingOp_       = std::move(op);
        pendingBlockingTitle_    = title;
        pendingBlockingSubtitle_ = subtitle;
        pendingBlockingDelay_    = 2;   // let 2 frames present the overlay first
    }

    void Editor::RunDeferredBlockingOp()
    {
        if (!pendingBlockingOp_) return;
        // Wait a couple of frames so the overlay is visibly drawn before we block.
        if (pendingBlockingDelay_ > 0) { --pendingBlockingDelay_; return; }

        auto op = std::move(pendingBlockingOp_);
        pendingBlockingOp_ = nullptr;     // clear before running (op may request another)
        op();
    }

    // ===================================================================================
    //  RUNTIME SCREEN-SPACE UI PASS  —  DO NOT DROP THIS IN EDITOR/VIEWPORT REFACTORS.
    //
    //  This is the in-game UI layer: NativeScript OnGUI() hooks, interact prompts, the
    //  quest-log overlay, and the dialogue box. It MUST run inside BeginTextureMode(viewport),
    //  after the world + light composite, while Playing or Paused. It has already been
    //  silently dropped TWICE by viewport/docking refactors (everything just stops drawing
    //  during Play). Keeping it as one clearly-named call — plus the Screen-unpopulated guard
    //  in Editor::Update — is what stops that happening unnoticed a third time.
    // ===================================================================================
    void Editor::DrawRuntimeUI()
    {
        // Draws only while the game is running; Pause still draws but stops accepting input.
        // MUST be called inside BeginTextureMode(viewport) (see the banner above) — the sole
        // call site is in Run()'s viewport pass, and it owns the render-target scope.
        if (state != GameState::Play && state != GameState::Pause) return;

        const int texW = viewport.texture.width;
        const int texH = viewport.texture.height;

        // Mouse mapped into viewport-texture pixels — the space the UI lays out in.
        Vector2 sm  = GetImGuiSpaceMousePosition();
        float   msx = (viewportSize.x > 0) ? (float)texW / viewportSize.x : 1.0f;
        float   msy = (viewportSize.y > 0) ? (float)texH / viewportSize.y : 1.0f;
        Vector2 vpMouse = { (sm.x - viewportPos.x) * msx, (sm.y - viewportPos.y) * msy };

        // Only accept game input while actually playing and the viewport is hovered,
        // so dialogue/skip don't fire while interacting with editor panels. Pause
        // keeps drawing with input off. Layer order, the menu gating, Esc routing
        // and the Screen update all live in the shared pass (core/RuntimeUI.hpp),
        // so the standalone player renders the exact same UI.
        const bool screenAccept = (state == GameState::Play) && viewportHovered;
        RuntimeUI::Draw(scene, texW, texH, vpMouse, screenAccept);
    }

    // Per-depthLayer parallax configuration, rendered inside the Project Settings window.
    // The per-layer factor drives both the scroll rate AND the lighting Z (see
    // LightingEnvironment::LayerZ), so this panel is also where you tune cross-layer
    // lighting depth.
    void Editor::ShowParallax()
    {
        if (!ImGui::CollapsingHeader("2.5D Parallax", ImGuiTreeNodeFlags_DefaultOpen)) return;
        ImGui::Indent(8.0f);

        ImGui::TextDisabled("Layers scroll at different rates to fake depth. The per-layer\n"
                            "factor also sets how far each layer sits in Z for lighting\n"
                            "(that part works even with scrolling off).");
        ImGui::Spacing();

        bool en = scene.parallaxEnabled;
        if (ImGui::Checkbox("Enable parallax scrolling", &en))
        {
            TakeSnapshot();
            scene.parallaxEnabled = en;
            isDirty = true;
        }

        ImGui::Spacing();
        ImGui::Text("Anchor (world point where every layer aligns)");
        ImGui::PushItemWidth(-1);
        float anchor[2] = { scene.parallaxAnchor.x, scene.parallaxAnchor.y };
        if (ImGui::DragFloat2("##ParallaxAnchor", anchor, 1.0f))
        {
            scene.parallaxAnchor = { anchor[0], anchor[1] };
            isDirty = true;
        }
        if (ImGui::IsItemActivated()) TakeSnapshot();
        ImGui::PopItemWidth();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Per-layer factor");
        ImGui::TextDisabled("> 1 = foreground (faster scroll, nearer the viewer)\n"
                            "< 1 = background (slower scroll, deeper in)\n"
                            "1.0 = locked to the camera. Layer 0 is always 1.0.");
        ImGui::Spacing();

        // Every layer in use, plus any that already carry an override, plus 0.
        std::set<int> layers;
        for (const auto& e : scene.entities) layers.insert(e->depthLayer);
        for (const auto& kv : scene.parallaxByLayer) layers.insert(kv.first);
        layers.insert(0);

        if (ImGui::BeginTable("##ParallaxLayers", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("Factor");
            ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthFixed, 55.0f);
            ImGui::TableSetupColumn("##act", ImGuiTableColumnFlags_WidthFixed, 55.0f);
            ImGui::TableHeadersRow();

            for (int layer : layers)
            {
                ImGui::PushID(layer);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", layer);

                ImGui::TableSetColumnIndex(1);
                float factor = scene.HasParallaxOverride(layer)
                                   ? scene.parallaxByLayer.at(layer)
                                   : Scene::DefaultParallaxFactor(layer);
                ImGui::PushItemWidth(-1);
                if (layer == 0)
                {
                    ImGui::BeginDisabled();
                    ImGui::DragFloat("##f", &factor, 0.01f);
                    ImGui::EndDisabled();
                }
                else
                {
                    if (ImGui::DragFloat("##f", &factor, 0.01f, 0.0f, 10.0f, "%.2f"))
                    {
                        scene.SetParallaxFactor(layer, factor);
                        isDirty = true;
                    }
                    if (ImGui::IsItemActivated()) TakeSnapshot();
                }
                ImGui::PopItemWidth();

                ImGui::TableSetColumnIndex(2);
                ImGui::TextDisabled("%.0f", LightingEnvironment::LayerZ(scene, layer));

                ImGui::TableSetColumnIndex(3);
                if (layer != 0 && scene.HasParallaxOverride(layer))
                {
                    if (ImGui::SmallButton("Reset"))
                    {
                        TakeSnapshot();
                        scene.ResetParallaxFactor(layer);
                        isDirty = true;
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        // Add or override an arbitrary layer (e.g. one with no entities yet).
        ImGui::Spacing();
        ImGui::Text("Set layer");
        ImGui::SameLine();
        ImGui::PushItemWidth(70.0f);
        ImGui::DragInt("##newLayer", &newParallaxLayer_, 1);
        ImGui::SameLine();
        ImGui::DragFloat("##newFactor", &newParallaxFactor_, 0.01f, 0.0f, 10.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Set##addParallax") && newParallaxLayer_ != 0)
        {
            TakeSnapshot();
            scene.SetParallaxFactor(newParallaxLayer_, newParallaxFactor_);
            isDirty = true;
        }

        ImGui::Unindent(8.0f);
    }

    void Editor::Run()
     {
        /**
         * @brief Dynamic Viewport Resizing
         *
         * If the ImGui viewport panel has been resized by th e user, we recreate
         * the RenderTexture to match the new dimensions. T his ensures that the
         * internal resolution always matches the visual output.
         */
        // Compare integer dimensions, not the raw float. ImGui's content region can report a
        // fractional size (e.g. 1280.4); testing it against the truncated texture width made
        // this true every frame, so the render textures were destroyed and recreated on every
        // single frame. Re-creating FBOs per frame flickers on some drivers (notably Mesa) —
        // invisible on the dark scene before, but obvious once a bright light was composited.
        int targetW = (int)viewportSize.x;
        int targetH = (int)viewportSize.y;
        if (targetW > 0 && targetH > 0 && (targetW != viewport.texture.width || targetH != viewport.texture.height))
        {
            UnloadRenderTexture(viewport);
            viewport = LoadRenderTexture(targetW, targetH);
            lighting_.Resize(targetW, targetH);
            postFx_.Resize(targetW, targetH);
        }

        /** @brief Snapshot the scene's lights for the per-pixel passes (3D meshes +
         *  normal-mapped sprites). Must run before the mesh pre-pass and scene.Draw below
         *  so both shade from this frame's lights. */
        LightingEnvironment::Get().Gather(scene);

        /** @brief Step 0: Build the light map (its own texture-mode scope, so it must run
         *  BEFORE BeginTextureMode(viewport) — raylib can't nest render targets). */
        const bool lightingActive = LightMapRenderer::SceneWantsLighting(scene);
        if (lightingActive) lighting_.Render(scene, GetActiveCamera());

        /** @brief Step 0b: Render any MeshRenderer's 3D model into its off-screen target,
         *  which (like the light map) needs its own BeginTextureMode scope and so must run
         *  BEFORE BeginTextureMode(viewport). In Play / the standalone player scene.Update()
         *  drives this via MeshRendererComponent::update(); in Edit mode Update() doesn't run,
         *  so the preview is kept live here. RenderIfNeeded() is dirty-gated, so a static prop
         *  costs nothing per frame. */
        for (const auto& e : scene.entities)
            for (const auto& c : e->components)
                if (auto* mesh = dynamic_cast<MeshRendererComponent*>(c.get()); mesh && mesh->enabled)
                    mesh->RenderIfNeeded();

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
                // scene.Draw() owns its own BeginMode2D/EndMode2D scope. raylib's
                // EndMode2D resets the modelview to identity (it does NOT restore the
                // previous camera), so once Draw() returns the camera transform is gone.
                // The editor overlays below therefore need their OWN BeginMode2D scope —
                // otherwise they render in screen space and ignore camera pan/zoom,
                // making outlines/gizmos detach from their entities.
                scene.Draw(activeCamera);

                // --- Lighting composite: multiply the lit world by the accumulated light map.
                // Done before the editor overlays below so gizmos/outlines/labels stay full-bright.
                if (lightingActive)
                    lighting_.Composite(viewport.texture.width, viewport.texture.height);

                BeginMode2D(activeCamera);

                // --- Selection Outline (world-space — stays in BeginMode2D) ---
                if (isSceneTab)
                {
                    if (selectedIndex >= 0 && selectedIndex < (int)scene.entities.size())
                    {
                        Entity* sel = scene.entities[selectedIndex].get();
                        if (sel)
                        {
                            const Color outlineColor = Color{ 0, 255, 255, 255 };
                            // Divide by zoom so the outline is always ~2 screen pixels wide,
                            // independent of zoom level. Without this, 2 world-unit thickness
                            // grows to many screen pixels when zoomed in, making the outline
                            // appear to separate from the entity.
                            const float thickness = 2.0f / editorCamera.zoom;
                            auto* cCol = sel->getComponent<CircleCollider2D>();

                            if (cCol) { DrawCircleLinesV(sel->getGlobalPosition(), cCol->getCircleRadius(), outlineColor); }
                            else
                            {
                                std::vector<Vector2> verts = sel->getVertices();
                                if (!verts.empty()) { for (size_t i = 0; i < verts.size(); i++) DrawLineEx(verts[i], verts[(i + 1) % verts.size()], thickness, outlineColor); }
                                else { DrawRectangleLinesEx(sel->getBounds(), thickness, outlineColor); }
                            }
                        }
                    }

                    // Multi-selection outlines (blue-cyan, distinct from primary)
                    const Color multiOutlineColor = Color{ 0, 180, 255, 200 };
                    const float mThick = 1.5f / editorCamera.zoom;
                    for (int mIdx : multiSelection_)
                    {
                        if (mIdx == selectedIndex) continue;
                        if (mIdx < 0 || mIdx >= (int)scene.entities.size()) continue;
                        Entity* me = scene.entities[mIdx].get();
                        if (!me) continue;
                        auto* mCol = me->getComponent<CircleCollider2D>();
                        if (mCol)  DrawCircleLinesV(me->getGlobalPosition(), mCol->getCircleRadius(), multiOutlineColor);
                        else
                        {
                            std::vector<Vector2> mv = me->getVertices();
                            if (!mv.empty()) { for (size_t k = 0; k < mv.size(); k++) { DrawLineEx(mv[k], mv[(k+1) % mv.size()], mThick, multiOutlineColor); } }
                            else { DrawRectangleLinesEx(me->getBounds(), mThick, multiOutlineColor); }
                        }
                    }
                }

                // --- Marquee / Box Selection Outline (world-space) ---
                if (isSceneTab && isSelectingBox)
                {
                    Vector2 p1 = selectBoxStart;
                    Vector2 p2 = worldMouse;
                    float   x  = std::min(p1.x, p2.x);
                    float   y  = std::min(p1.y, p2.y);
                    float   w  = std::abs(p1.x - p2.x);
                    float   h  = std::abs(p1.y - p2.y);

                    DrawRectangleRec(::Rectangle{ x, y, w, h }, Color{ 0, 120, 255, 40 });
                    DrawRectangleLinesEx(::Rectangle{ x, y, w, h }, 1.5f / editorCamera.zoom, Color{ 0, 120, 255, 200 });
                }

                // --- Camera Gizmos (Scene tab only, world-space) ---
                if (isSceneTab)
                {
                    for (const auto& ent : scene.entities)
                    {
                        for (const auto& comp : ent->components)
                        {
                            auto* cam = dynamic_cast<CameraComponent*>(comp.get());
                            if (!cam) continue;

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
                        }
                    }
                }

                // --- Transform Handle Gizmos ---
                // isSceneTab is false in Play/Pause, so these editor handles vanish the
                // moment you hit Play — same as every other overlay above.
                if (isSceneTab
                    && selectedIndex >= 0 && selectedIndex < (int)scene.entities.size()
                    && activeTool_ != TransformTool::Move)
                {
                    Entity* sel    = scene.entities[selectedIndex].get();
                    Vector2 center = sel->getGlobalPosition();
                    float   rot    = sel->getGlobalRotation();
                    float   hw     = sel->scale.x / 2.0f;
                    float   hh     = sel->scale.y / 2.0f;
                    float   HR     = 7.0f / editorCamera.zoom;

                    // Visual half-extents — must match the hit-test in ViewportPanel:
                    // a circle's size is its collider radius (× scale), not entity scale.
                    auto* selCircle = sel->getComponent<CircleCollider2D>();
                    if (selCircle) { hw = hh = selCircle->getCircleRadius(); }

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

                    // Handles draw for every entity type (sprites, circles, text, ...) —
                    // extents come from the visual size above, vertices are only used
                    // for the optional OBB outline below.
                    bool showRect = (activeTool_ == TransformTool::Rect || activeTool_ == TransformTool::Universal);
                    bool showRot  = (activeTool_ == TransformTool::Rotate || activeTool_ == TransformTool::Universal);

                    if (showRect)
                    {
                        bool parented = sel->parentId != -1;
                        Color outlineCol = parented ? Color{80, 80, 80, 120}   : Color{100, 180, 255, 160};
                        Color handleCol  = parented ? Color{90, 90, 90, 160}   : Color{50, 140, 255, 255};
                        // Bounding box through the handle points, so the 8 dots always
                        // read as connected (mid-edge dots sit on the lines). Drawn from
                        // the same extents as the handles — getVertices() can disagree
                        // with them (collider offsets) and circles have no vertices.
                        Vector2 q[4] = { toWorld(-hw,-hh), toWorld(+hw,-hh), toWorld(+hw,+hh), toWorld(-hw,+hh) };
                        for (int k = 0; k < 4; k++) DrawLineEx(q[k], q[(k+1)%4], 1.0f / editorCamera.zoom, outlineCol);
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
                }

                EndMode2D();
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

            // --- In-game screen-space UI (Play/Pause) ---
            // Single source of truth is Editor::DrawRuntimeUI() (see its banner): one
            // clearly-named call so an editor/viewport refactor can't silently drop the
            // runtime UI a third time. MUST stay inside this BeginTextureMode(viewport) scope.
            DrawRuntimeUI();

        EndTextureMode();

        /** @brief Step 1.5: Post-processing — chain every enabled PostProcessComponent's
         *  shader over the rendered viewport. Applied in editor and play so effects are
         *  WYSIWYG. Must run AFTER EndTextureMode(viewport) and BEFORE the texture is drawn
         *  to the ImGui viewport panel. */
        {
            std::vector<PostProcessComponent*> activeFx;
            for (const auto& e : scene.entities)
            {
                if (!e->activeInHierarchy()) continue;
                for (const auto& c : e->components)
                {
                    if (!c->enabled) continue;
                    if (auto* pp = dynamic_cast<PostProcessComponent*>(c.get())) activeFx.push_back(pp);
                }
            }
            if (!activeFx.empty()) postFx_.Apply(viewport, activeFx);
        }

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

                    // If scripts failed to auto-compile on open, the user would
                    // otherwise only notice via "my script has no properties".
                    // Surface it loudly in the console instead.
                    if (ScriptManager::Get().lastAutoCompileFailed)
                    {
                        consoleLogs.push_back({ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "[SCRIPTS]",
                            "Scripts failed to compile on open — components show no properties until fixed. "
                            "Use Scripts > Compile & Reload after resolving:\n" + ScriptManager::Get().lastAutoCompileLog,
                            ICON_FA_TRIANGLE_EXCLAMATION});
                    }
                }
            }
            else
            {
                float menuBarH = ImGui::GetFrameHeight();
                float screenW = (float)GetScreenWidth();
                float screenH = (float)GetScreenHeight();

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

                // --- DockSpace host: fills the work area below the menu bar. Every editor
                // panel docks into this; the user can rearrange / tab / float / resize them
                // freely and the layout persists in imgui.ini between sessions. ---
                ImGuiViewport* dockVp = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(dockVp->WorkPos);
                ImGui::SetNextWindowSize(dockVp->WorkSize);
                ImGui::SetNextWindowViewport(dockVp->ID);
                ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                             ImGuiWindowFlags_NoResize  | ImGuiWindowFlags_NoMove |
                                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                             ImGuiWindowFlags_NoDocking;
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::Begin("##DockHost", nullptr, hostFlags);
                ImGui::PopStyleVar(3);

                ImGuiID dockId = ImGui::GetID("MainDockSpace");
                // Build the default layout once. Skipped if imgui.ini already restored a
                // layout (the node will already exist), so user customizations persist.
                // Window > Reset Layout forces a rebuild back to the default arrangement.
                if (ImGui::DockBuilderGetNode(dockId) == nullptr || resetDockLayout_)
                {
                    resetDockLayout_ = false;
                    ImGui::DockBuilderRemoveNode(dockId);
                    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
                    ImGui::DockBuilderSetNodeSize(dockId, dockVp->WorkSize);

                    ImGuiID center = dockId;
                    ImGuiID left   = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left,  0.18f, nullptr, &center);
                    ImGuiID right  = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.22f, nullptr, &center);
                    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down,  0.28f, nullptr, &center);

                    ImGui::DockBuilderDockWindow("Hierarchy", left);
                    ImGui::DockBuilderDockWindow("Inspector", right);
                    ImGui::DockBuilderDockWindow("Viewport",  center);
                    ImGui::DockBuilderDockWindow(ICON_FA_FOLDER_OPEN "  Content Browser", bottom);
                    ImGui::DockBuilderDockWindow(ICON_FA_TERMINAL "  Console",            bottom);
                    ImGui::DockBuilderDockWindow(ICON_FA_FLAG "  Story State",            bottom);
                    ImGui::DockBuilderDockWindow(ICON_FA_FLAG "  Quests",                 bottom);
                    ImGui::DockBuilderDockWindow(ICON_FA_COMMENT "  Dialogue",            bottom);
                    ImGui::DockBuilderFinish(dockId);
                }
                ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_None);
                ImGui::End(); // ##DockHost

                // Panels — each a dockable window (no manual positioning anymore).
                ShowViewport();
                ShowHierarchy();
                ShowInspector();

                // Bottom-area panels — each is its own dockable window. The default
                // layout tabs them together at the bottom; users can split them apart.
                // The Window > Bottom Panel toggle shows/hides them as a group.
                if (showBottomPanel)
                {
                    if (ImGui::Begin(ICON_FA_FOLDER_OPEN "  Content Browser")) ShowContentBrowser();
                    ImGui::End();
                    if (ImGui::Begin(ICON_FA_TERMINAL "  Console"))            ShowConsole();
                    ImGui::End();
                    if (ImGui::Begin(ICON_FA_FLAG "  Story State"))            ShowStoryState();
                    ImGui::End();
                    if (ImGui::Begin(ICON_FA_FLAG "  Quests"))                 ShowQuests();
                    ImGui::End();
                    if (ImGui::Begin(ICON_FA_BOXES_STACKED "  Items"))         ShowItems();
                    ImGui::End();
                    if (ImGui::Begin(ICON_FA_COMMENT "  Dialogue"))            ShowDialogue();
                    ImGui::End();
                    if (ImGui::Begin(ICON_FA_FILM "  Cutscenes"))              ShowCutscenes();
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

                            ImGui::Spacing();
                            ImGui::Separator();
                            ImGui::Spacing();

                            // --- 2D Lighting ---
                            ImGui::Text("2D Lighting");
                            ImGui::TextDisabled("Turns on automatically when a scene has a Light 2D\ncomponent. Tick below to force it on with no lights\n(pure ambient darkness).");
                            bool lit = scene.lightingEnabled;
                            if (ImGui::Checkbox("Force lighting on", &lit))
                            {
                                TakeSnapshot();
                                scene.lightingEnabled = lit;
                                isDirty = true;
                            }

                            ImGui::Spacing();
                            ImGui::Text("Ambient Light");
                            ImGui::TextDisabled("The brightness where no light reaches. Darken for\nnight; white = no darkening (lights won't show).");
                            float amb[3] = { scene.ambientLight.r / 255.f, scene.ambientLight.g / 255.f, scene.ambientLight.b / 255.f };
                            ImGui::PushItemWidth(-1);
                            if (ImGui::ColorEdit3("##AmbientLight", amb))
                            {
                                scene.ambientLight.r = (unsigned char)(amb[0] * 255);
                                scene.ambientLight.g = (unsigned char)(amb[1] * 255);
                                scene.ambientLight.b = (unsigned char)(amb[2] * 255);
                                isDirty = true;
                            }
                            if (ImGui::IsItemActivated()) TakeSnapshot();
                            ImGui::PopItemWidth();

                            ImGui::Unindent(8.0f);
                        }

                        ImGui::Spacing();

                        // --- 2.5D Parallax (per-layer scroll rate + lighting depth) ---
                        ShowParallax();

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

                        // --- Audio Mixer ---
                        if (ImGui::CollapsingHeader("Audio Mixer", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::Indent(8.0f);
                            ImGui::TextDisabled("Master + per-bus volumes. AudioSources route\nthrough a bus (set on each source).");
                            ImGui::Spacing();

                            auto& mixer = AudioMixer::Get();
                            ImGui::Text("Master");
                            ImGui::PushItemWidth(-1);
                            ImGui::SliderFloat("##MixMaster", &mixer.master, 0.0f, 1.0f, "%.2f");
                            ImGui::PopItemWidth();
                            ImGui::Spacing();
                            for (auto& [name, vol] : mixer.buses)
                            {
                                ImGui::Text("%s", name.c_str());
                                ImGui::PushID(name.c_str());
                                ImGui::PushItemWidth(-1);
                                ImGui::SliderFloat("##bus", &vol, 0.0f, 1.0f, "%.2f");
                                ImGui::PopItemWidth();
                                ImGui::PopID();
                            }
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

            // Script-compile spinner — drawn regardless of editor/play state so it
            // stays visible for the whole async compile.
            DrawScriptCompileModal();

            // Transient toast notifications (top-right, auto-fade).
            DrawToasts();

            rlImGuiEnd();
        EndDrawing();
    }
}
