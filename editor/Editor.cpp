#include "Editor.hpp"
#include "rlgl.h"   // immediate-mode vertices for spotlight cones + culling control for shadows
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
        viewport      = LoadRenderTexture(1, 1);
        lightMap_     = LoadRenderTexture(1, 1);
        lightScratch_ = LoadRenderTexture(1, 1);

        // Post-processing shader pipeline (scratch RTs + compiled effect shaders).
        postFx_.Init();

        // Bake the radial light splat once (white core fading to transparent edge). Each
        // Light2DComponent draws this additively, tinted and scaled, into the light map.
        {
            Image grad     = GenImageGradientRadial(256, 256, 0.0f, WHITE, BLANK);
            lightGradient_ = LoadTextureFromImage(grad);
            UnloadImage(grad);
            SetTextureFilter(lightGradient_, TEXTURE_FILTER_BILINEAR);
        }

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
        UnloadRenderTexture(lightMap_);
        UnloadRenderTexture(lightScratch_);
        UnloadTexture(lightGradient_);
    }

    void Editor::RaylibTraceCallback(int level, const char* text, va_list args)
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

    void Editor::Update(float dt)
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

        if (state == GameState::Play)
        {
            // Inject current viewport pixel size so CameraComponent bounds clamp is viewport-aware
            for (auto& e : scene.entities)
            {
                for (auto& c : e->components) {if (auto* cam = dynamic_cast<CameraComponent*>(c.get())) cam->viewportPx_ = {viewportSize.x, viewportSize.y};}
            }

            scene.Update(dt);

            // Tick the cutscene player AFTER the scene so a running cutscene has the final
            // say on the entities it drives this frame. It advances on the raw (unscaled) dt
            // so a cutscene that freezes gameplay (Time::scale=0) still plays.
            CutsceneManager::Get().Update(dt, &scene);

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

    void Editor::RenderLightMap(const Camera2D& cam)
    {
        using LT = Light2DComponent::LightType;

        // --- Gather solid (non-trigger) collider polygons once; reused as shadow occluders. ---
        std::vector<std::pair<Entity*, std::vector<Vector2>>> occluders;
        for (const auto& e : scene.entities)
        {
            if (!e->activeInHierarchy()) continue;
            auto* col = e->getComponent<Collider2D>();
            if (!col || col->isTrigger) continue;

            std::vector<Vector2> poly = col->getVertices();
            if (poly.empty() && col->isCircleShape())
            {
                // Approximate a circle collider with a polygon so it can cast shadows too.
                Vector2 cc = Vector2Add(e->getGlobalPosition(), col->offset);
                float   r  = col->getCircleRadius();
                const int N = 16;
                poly.reserve(N);
                for (int i = 0; i < N; ++i)
                {
                    float ang = (float)i / N * 2.0f * PI;
                    poly.push_back({ cc.x + cosf(ang) * r, cc.y + sinf(ang) * r });
                }
            }
            if (poly.size() >= 3) occluders.emplace_back(e.get(), std::move(poly));
        }

        const float gw = (float)lightGradient_.width;
        const float gh = (float)lightGradient_.height;

        // --- Accumulation buffer: ambient floor, then flat Directional lights. ---
        BeginTextureMode(lightMap_);
            ClearBackground(Color{ scene.ambientLight.r, scene.ambientLight.g, scene.ambientLight.b, 255 });

            BeginBlendMode(BLEND_ADDITIVE);
            for (const auto& e : scene.entities)
            {
                if (!e->activeInHierarchy()) continue;
                for (const auto& c : e->components)
                {
                    if (!c->enabled) continue;
                    auto* light = dynamic_cast<Light2DComponent*>(c.get());
                    if (!light || light->type != LT::Directional) continue;
                    float eff = light->EffectiveIntensity();
                    if (eff <= 0.0f) continue;
                    auto a = (unsigned char)Clamp(eff * 255.0f, 0.0f, 255.0f);
                    DrawRectangle(0, 0, lightMap_.texture.width, lightMap_.texture.height,
                                  Color{ light->color.r, light->color.g, light->color.b, a });
                }
            }
            EndBlendMode();
        EndTextureMode();

        // --- Point / Spot lights: render each (minus its shadows) to scratch, then add in. ---
        for (const auto& e : scene.entities)
        {
            if (!e->activeInHierarchy()) continue;
            for (const auto& c : e->components)
            {
                if (!c->enabled) continue;
                auto* light = dynamic_cast<Light2DComponent*>(c.get());
                if (!light || light->type == LT::Directional) continue;

                float eff = light->EffectiveIntensity();
                if (eff <= 0.0f || light->radius <= 0.0f) continue;

                Vector2 p    = e->getGlobalPosition();
                auto    a    = (unsigned char)Clamp(eff * 255.0f, 0.0f, 255.0f);
                Color   tint = { light->color.r, light->color.g, light->color.b, a };
                float   d    = light->radius * 2.0f;

                // 1) Draw this light's shape into the scratch buffer (over transparent black).
                BeginTextureMode(lightScratch_);
                    ClearBackground(BLANK);
                    BeginMode2D(cam);

                        if (light->type == LT::Point)
                        {
                            DrawTexturePro(lightGradient_,
                                ::Rectangle{ 0.0f, 0.0f, gw, gh },
                                ::Rectangle{ p.x, p.y, d, d },
                                Vector2{ d * 0.5f, d * 0.5f }, 0.0f, tint);
                        }
                        else // Spot: a cone fan, bright apex fading to nothing at the rim.
                        {
                            float baseA = e->getGlobalRotation() * DEG2RAD;
                            float halfA = light->coneAngle * 0.5f * DEG2RAD;
                            float range = light->radius;
                            const int SEG = 32;
                            rlSetTexture(rlGetTextureIdDefault()); // untextured white verts
                            rlBegin(RL_TRIANGLES);
                            for (int s = 0; s < SEG; ++s)
                            {
                                float a0 = baseA - halfA + (2.0f * halfA) * ((float)s       / SEG);
                                float a1 = baseA - halfA + (2.0f * halfA) * ((float)(s + 1) / SEG);
                                rlColor4ub(tint.r, tint.g, tint.b, a);
                                rlVertex2f(p.x, p.y);
                                rlColor4ub(tint.r, tint.g, tint.b, 0);
                                rlVertex2f(p.x + cosf(a0) * range, p.y + sinf(a0) * range);
                                rlColor4ub(tint.r, tint.g, tint.b, 0);
                                rlVertex2f(p.x + cosf(a1) * range, p.y + sinf(a1) * range);
                            }
                            rlEnd();
                        }

                        // 2) Carve shadows: extrude each occluder edge away from the light and
                        //    paint the resulting quad solid black, removing this light behind it.
                        if (light->castShadows)
                        {
                            rlDisableBackfaceCulling(); // shadow quad winding varies per edge
                            rlSetTexture(rlGetTextureIdDefault()); // draw untextured triangles

                            float reach = light->radius * 2.5f;
                            float softness = light->shadowSoftness;

                            for (auto& oc : occluders)
                            {
                                if (oc.first == e.get()) continue; // a light never shadows itself
                                const auto& poly = oc.second;
                                size_t n = poly.size();
                                for (size_t i = 0; i < n; ++i)
                                {
                                    Vector2 va = poly[i];
                                    Vector2 vb = poly[(i + 1) % n];

                                    Vector2 dirA = Vector2Subtract(va, p);
                                    Vector2 dirB = Vector2Subtract(vb, p);
                                    float distA = Vector2Length(dirA);
                                    float distB = Vector2Length(dirB);

                                    if (distA <= 0.001f || distB <= 0.001f) continue;

                                    Vector2 ndirA = Vector2Scale(dirA, 1.0f / distA);
                                    Vector2 ndirB = Vector2Scale(dirB, 1.0f / distB);

                                    // Generate perpendiculars pointing outwards from the edge AB
                                    Vector2 perpA = { -ndirA.y, ndirA.x };
                                    Vector2 toB = Vector2Subtract(vb, va);
                                    if (Vector2DotProduct(perpA, toB) > 0.0f) perpA = Vector2Scale(perpA, -1.0f);

                                    Vector2 perpB = { -ndirB.y, ndirB.x };
                                    Vector2 toA = Vector2Subtract(va, vb);
                                    if (Vector2DotProduct(perpB, toA) > 0.0f) perpB = Vector2Scale(perpB, -1.0f);

                                    // Soft shadow math:
                                    // Outer penumbra ray diverges away from the edge.
                                    // Inner umbra ray converges towards the inside of the edge.
                                    float divA = softness / distA;
                                    float divB = softness / distB;

                                    Vector2 rOuterA = Vector2Normalize(Vector2Add(ndirA, Vector2Scale(perpA, divA)));
                                    Vector2 rInnerA = Vector2Normalize(Vector2Subtract(ndirA, Vector2Scale(perpA, divA)));

                                    Vector2 rOuterB = Vector2Normalize(Vector2Add(ndirB, Vector2Scale(perpB, divB)));
                                    Vector2 rInnerB = Vector2Normalize(Vector2Subtract(ndirB, Vector2Scale(perpB, divB)));

                                    Vector2 vaP_inner = Vector2Add(va, Vector2Scale(rInnerA, reach));
                                    Vector2 vbP_inner = Vector2Add(vb, Vector2Scale(rInnerB, reach));
                                    Vector2 vaP_outer = Vector2Add(va, Vector2Scale(rOuterA, reach));
                                    Vector2 vbP_outer = Vector2Add(vb, Vector2Scale(rOuterB, reach));

                                    rlBegin(RL_TRIANGLES);
                                        // 1. Solid Umbra core (BLACK)
                                        rlColor4ub(0, 0, 0, 255); rlVertex2f(va.x, va.y);
                                        rlColor4ub(0, 0, 0, 255); rlVertex2f(vb.x, vb.y);
                                        rlColor4ub(0, 0, 0, 255); rlVertex2f(vbP_inner.x, vbP_inner.y);

                                        rlColor4ub(0, 0, 0, 255); rlVertex2f(va.x, va.y);
                                        rlColor4ub(0, 0, 0, 255); rlVertex2f(vbP_inner.x, vbP_inner.y);
                                        rlColor4ub(0, 0, 0, 255); rlVertex2f(vaP_inner.x, vaP_inner.y);

                                        // 2. Left Penumbra triangle (va -> vaP_inner -> vaP_outer)
                                        // Fades from BLACK to BLANK (transparent)
                                        rlColor4ub(0, 0, 0, 255); rlVertex2f(va.x, va.y);
                                        rlColor4ub(0, 0, 0, 255); rlVertex2f(vaP_inner.x, vaP_inner.y);
                                        rlColor4ub(0, 0, 0, 0);   rlVertex2f(vaP_outer.x, vaP_outer.y);

                                        // 3. Right Penumbra triangle (vb -> vbP_outer -> vbP_inner)
                                        // Fades from BLACK to BLANK (transparent)
                                        rlColor4ub(0, 0, 0, 255); rlVertex2f(vb.x, vb.y);
                                        rlColor4ub(0, 0, 0, 0);   rlVertex2f(vbP_outer.x, vbP_outer.y);
                                        rlColor4ub(0, 0, 0, 255); rlVertex2f(vbP_inner.x, vbP_inner.y);
                                    rlEnd();
                                }
                            }
                        }

                    EndMode2D();
                EndTextureMode();

                // 3) Add the shadowed light into the accumulation buffer with a straight RGB
                //    add (BLEND_ADD_COLORS). The scratch is a render texture, so flip the
                //    source height (-h) to undo raylib's bottom-up framebuffer orientation.
                BeginTextureMode(lightMap_);
                    BeginBlendMode(BLEND_ADD_COLORS);
                    DrawTexturePro(lightScratch_.texture,
                        ::Rectangle{ 0.0f, 0.0f, (float)lightScratch_.texture.width, -(float)lightScratch_.texture.height },
                        ::Rectangle{ 0.0f, 0.0f, (float)lightMap_.texture.width,  (float)lightMap_.texture.height },
                        Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
                    EndBlendMode();
                EndTextureMode();
            }
        }
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
            UnloadRenderTexture(lightMap_);
            UnloadRenderTexture(lightScratch_);
            viewport      = LoadRenderTexture(targetW, targetH);
            lightMap_     = LoadRenderTexture(targetW, targetH);
            lightScratch_ = LoadRenderTexture(targetW, targetH);
            postFx_.Resize(targetW, targetH);
        }

        /** @brief Step 0: Build the light map (its own texture-mode scope, so it must run
         *  BEFORE BeginTextureMode(viewport) — raylib can't nest render targets).
         *  Lighting activates when the scene's master toggle is on OR the scene contains at
         *  least one enabled Light2D — so dropping a light in "just works" without hunting
         *  for the Project Settings switch. */
        bool sceneHasLight = false;
        for (const auto& e : scene.entities)
        {
            if (!e->activeInHierarchy()) continue;
            for (const auto& c : e->components)
            {
                if (c->enabled && dynamic_cast<Light2DComponent*>(c.get())) { sceneHasLight = true; break; }
            }
            if (sceneHasLight) break;
        }
        const bool lightingActive = scene.lightingEnabled || sceneHasLight;
        if (lightingActive) RenderLightMap(GetActiveCamera());

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
                // The light map is a render texture, so its source rect height is flipped (-h)
                // to undo raylib's bottom-up framebuffer orientation.
                if (lightingActive)
                {
                    BeginBlendMode(BLEND_MULTIPLIED);
                    DrawTexturePro(
                        lightMap_.texture,
                        ::Rectangle{ 0.0f, 0.0f, (float)lightMap_.texture.width, -(float)lightMap_.texture.height },
                        ::Rectangle{ 0.0f, 0.0f, (float)viewport.texture.width,  (float)viewport.texture.height },
                        Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
                    EndBlendMode();
                }

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
                if (selectedIndex >= 0 && selectedIndex < (int)scene.entities.size()
                    && activeTool_ != TransformTool::Move)
                {
                    Entity* sel    = scene.entities[selectedIndex].get();
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
            // Drawn into the viewport texture in pixel space (origin top-left) after the
            // world. This is the single screen-space pass for the running game: it hosts
            // the cutscene letterbox/skip overlay plus the engine's other screen-space UI
            // (interaction prompts via onGUI, the dialogue box, the quest log), none of
            // which otherwise has a draw site.
            if (state == GameState::Play || state == GameState::Pause)
            {
                const int texW = viewport.texture.width;
                const int texH = viewport.texture.height;

                // Mouse mapped into viewport-texture pixels — the space the UI lays out in.
                Vector2 sm  = GetImGuiSpaceMousePosition();
                float   msx = (viewportSize.x > 0) ? (float)texW / viewportSize.x : 1.0f;
                float   msy = (viewportSize.y > 0) ? (float)texH / viewportSize.y : 1.0f;
                Vector2 vpMouse = { (sm.x - viewportPos.x) * msx, (sm.y - viewportPos.y) * msy };

                // Only accept game input while actually playing and the viewport is hovered,
                // so dialogue/skip don't fire while interacting with editor panels.
                const bool accept = (state == GameState::Play) && viewportHovered;
                Screen::Get().Set(texW, texH, vpMouse,
                                  accept && IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                  accept && IsMouseButtonDown(MOUSE_BUTTON_LEFT));

                // Cinematic letterbox behind the UI while a cutscene plays (eases in),
                // unless this cutscene opted out (Cutscene::letterbox).
                if (CutsceneManager::Get().IsActive() && CutsceneManager::Get().Current().letterbox)
                {
                    float f = CutsceneManager::Get().Time() / 0.35f;
                    if (f > 1.0f) f = 1.0f; else if (f < 0.0f) f = 0.0f;
                    const int barH = (int)(texH * 0.12f * f);
                    if (barH > 0)
                    {
                        DrawRectangle(0, 0,            texW, barH, BLACK);
                        DrawRectangle(0, texH - barH,  texW, barH, BLACK);
                    }
                }

                // Engine screen-space UI: per-component onGUI (e.g. PlayerInteractor's
                // prompt), then the dialogue box, then the quest log.
                for (auto& e : scene.entities)
                    if (e->activeInHierarchy())
                        for (auto& c : e->components)
                            if (c->enabled) c->onGUI(&scene);
                DialogueManager::Get().DrawGUI(accept);
                QuestManager::Get().DrawLogGUI(accept);

                // Skip prompt (kept in the top band so the dialogue box can't hide it) + input.
                if (CutsceneManager::Get().IsActive())
                {
                    const char* msg = "Press [Esc] to skip";
                    const int   fs  = 16;
                    const int   tw  = MeasureText(msg, fs);
                    DrawText(msg, texW - tw - 18, (int)(texH * 0.12f) + 8, fs, Color{ 210, 210, 220, 200 });
                    if (accept && IsKeyPressed(KEY_ESCAPE)) CutsceneManager::Get().Skip();
                }
            }

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
