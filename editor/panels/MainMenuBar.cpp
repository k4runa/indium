#include "../Editor.hpp"
#include "../../core/Logger.hpp"
#include "../../core/SaveManager.hpp"
#include "../../core/Exporter.hpp"

namespace Indium
{
    void Editor::ShowMainMenuBar()
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
                if (ImGui::MenuItem("Save", "Ctrl+S")) { pm.SaveCurrentProject(scene); isDirty = false; PushToast("Scene saved"); }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_FILE_EXPORT "  Export Game...", nullptr, false,
                                    state == GameState::Editor && pm.IsProjectOpen()))
                {
                    RequestBlockingOp("Exporting game", pm.GetCurrentProjectName(), [this]()
                    {
                        // Export what's on screen: persist the scene first.
                        pm.SaveCurrentProject(scene);
                        isDirty = false;

                        // The player runtime is built next to the editor executable.
#if defined(_WIN32)
                        const std::string playerBin = std::string(GetApplicationDirectory()) + "IndiumPlayer.exe";
#else
                        const std::string playerBin = std::string(GetApplicationDirectory()) + "IndiumPlayer";
#endif
                        Exporter::Result res = Exporter::ExportGame(
                            pm.GetCurrentProjectPath(), pm.GetCurrentProjectName(),
                            playerBin, pm.GetCurrentProjectPath() + "/Export");

                        if (res.ok)
                        {
                            PushToast("Game exported: " + res.outputDir, ImVec4(0.4f, 0.8f, 0.4f, 1.0f), 6.0f);
                            consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[EXPORT]",
                                "Game exported to " + res.outputDir, ICON_FA_FILE_EXPORT});
                        }
                        else
                        {
                            PushToast("Export failed — see Console", ImVec4(0.9f, 0.3f, 0.3f, 1.0f), 6.0f);
                            consoleLogs.push_back({ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "[EXPORT]",
                                res.error, ICON_FA_XMARK});
                        }
                    });
                }
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
                if (ImGui::MenuItem(ICON_FA_TABLE_CELLS_LARGE "  Reset Layout")) resetDockLayout_ = true;
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
                // Disable while a compile is already running so we never start two.
                const bool canCompile = (state == GameState::Editor) && !scriptCompileRunning_;
                if (ImGui::MenuItem("Compile & Reload", nullptr, false, canCompile))
                {
                    if (!pm.IsProjectOpen()) { consoleLogs.push_back({ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "[WARNING]", "No project open to compile scripts.", ICON_FA_EXCLAMATION}); }
                    else StartScriptCompile();   // runs on a worker thread; modal shows progress
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
                        // Switching a scene can recompile/reload scripts — defer it
                        // behind the busy overlay so the editor doesn't just freeze.
                        std::string target = sceneFile;
                        RequestBlockingOp("Loading scene", fs::path(target).stem().string(),
                            [this, target]()
                            {
                                pm.SaveCurrentProject(scene);
                                isDirty = false;
                                if (pm.SwitchScene(target, scene))
                                {
                                    selectedIndex = -1;
                                    undoStack.clear();
                                    redoStack.clear();
                                    editorCamera.target = scene.editorCameraTarget;
                                    editorCamera.zoom   = scene.editorCameraZoom;
                                    editorCamera.offset = { 0, 0 };
                                }
                            });
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
                PushToast("Scene saved");
            }
            if (CtrlDown() && !IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_Z)) Undo();
            if (CtrlDown() && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_Z))  Redo();

            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem(ICON_FA_CUBE "  Empty"))              CreateEntityAt("Empty",          editorCamera.target);
                ImGui::Separator();
                if (ImGui::BeginMenu(ICON_FA_VECTOR_SQUARE "  2D Object"))
                {
                    if (ImGui::MenuItem(ICON_FA_CIRCLE "  Circle"))           CreateEntityAt("Circle",    editorCamera.target);
                    if (ImGui::MenuItem(ICON_FA_VECTOR_SQUARE "  Rectangle")) CreateEntityAt("Rectangle", editorCamera.target);
                    if (ImGui::MenuItem(ICON_FA_LAYER_GROUP "  Surface"))     CreateEntityAt("Surface",   editorCamera.target);
                    if (ImGui::MenuItem(ICON_FA_IMAGE "  Image (Sprite)"))    CreateEntityAt("Sprite",    editorCamera.target);
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem(ICON_FA_FONT "  Text"))               CreateEntityAt("Text",           editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_LIGHTBULB "  Light 2D"))      CreateEntityAt("Light",          editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_CAMERA "  Camera"))           CreateEntityAt("Camera",         editorCamera.target);
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_STAR "  Particle System"))    CreateEntityAt("ParticleSystem", editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_TABLE_CELLS "  Tilemap"))     CreateEntityAt("Tilemap",        editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_VOLUME_HIGH "  Audio Source")) CreateEntityAt("AudioSource",   editorCamera.target);
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_VECTOR_SQUARE "  Trigger Zone")) CreateEntityAt("TriggerZone", editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_LOCATION_DOT "  Spawn Point"))   CreateEntityAt("SpawnPoint",  editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_FLAG "  Checkpoint"))            CreateEntityAt("Checkpoint",  editorCamera.target);
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
                Logger::Event("EDITOR", "Play started (%d entities)", (int)scene.entities.size());
                csExitPreview();   // restore any cutscene scrub preview before snapshotting the scene
                scene.Save();
                state = GameState::Play;
                StoryState::Get().Clear();
                StoryState::Get().Seed(scene.storyState);
                // Re-arm subscriptions wiped by the previous Stop's EventBus::Clear(), so
                // NarrativeEvents still record flags and quests advance on Stop->Play cycles.
                StoryState::Get().SubscribeToEvents();
                QuestManager::Get().SetProjectPath(pm.GetCurrentProjectPath());
                QuestManager::Get().LoadAll();
                QuestManager::Get().SubscribeToEvents();
                // Point the cutscene player at the project so a script / Interactable can
                // Play("name") a cutscene by id; cutscenes load lazily on first Play.
                CutsceneManager::Get().SetProjectPath(pm.GetCurrentProjectPath());
                // Load item definitions so Give/Take and the inventory HUD work in Play.
                // Counts live in StoryState (seeded above), so no other persistence is needed.
                ItemManager::Get().SetProjectPath(pm.GetCurrentProjectPath());
                ItemManager::Get().LoadAll();
                // Save slots: point SaveManager at the project, restore default autosave
                // behavior (scripts re-customize in OnStart below), and re-arm the
                // StoryStateChangedEvent subscription that drives autosave conditions.
                SaveManager::SetProjectPath(pm.GetCurrentProjectPath());
                SaveManager::ResetAutosaveConfig();
                SaveManager::SubscribeToEvents();
                // Front-end menus: clean slate each Play (no stale pause page or armed
                // rebind from the last run), titled after the project; then layer the
                // player's saved settings (audio volumes + key rebinds) over the
                // authored defaults — GameSettings::Load never adds or clears actions.
                MenuManager::Get().Reset();
                MenuManager::Get().SetTitle(pm.GetCurrentProjectName());
                GameSettings::Get().SetProjectPath(pm.GetCurrentProjectPath());
                GameSettings::Get().Load();

                // Snapshot raw component pointers BEFORE calling awake()/start(). A script's
                // OnStart() may AddComponent<>() (e.g. PlayerMovement adds a Rigidbody), which
                // push_backs into e->components and can REALLOCATE the vector we'd otherwise be
                // range-iterating — dangling the iterator and crashing. The Component objects
                // themselves are heap-allocated and never move, so cached raw pointers stay
                // valid across a reallocation. Newly-added components are start()'d by
                // AddComponent itself, so they don't need to be in this snapshot.
                std::vector<Component*> startComps;
                for (auto& e : scene.entities) for (auto& c : e->components) startComps.push_back(c.get());
                for (auto* c : startComps) c->awake(&scene);
                for (auto* c : startComps) c->start(&scene);
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
                Logger::Event("EDITOR", "Play stopped");
                Events::Publish(GameEvents::GameStopEvent{});
                scene.Restore();
                state = GameState::Editor;
                selectedIndex = -1;
                multiSelection_.clear();
                StoryState::Get().Clear();
                EventBus::Get().Clear();
                QuestManager::Get().Reset();
                // Clear runtime viewport state so the DrawRuntimeUI regression guard (Editor::Update)
                // re-arms: Screen::Width() reads 0 again until the next Play repopulates it.
                Screen::Get().Set(0, 0, { 0, 0 }, false, false);
                CutsceneManager::Get().End();   // stop any running cutscene + restore Time::scale
                DialogueManager::Get().End();   // drop any dialogue active at Stop so it can't bleed into the next Play
                // Persist menu-made settings changes even when the user stops Play while
                // still inside the settings page (Back is what normally saves). Dirty-gated
                // so script-driven runtime volume changes are never captured as preferences.
                if (GameSettings::Get().IsDirty()) GameSettings::Get().Save();
                MenuManager::Get().Reset();
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
}
