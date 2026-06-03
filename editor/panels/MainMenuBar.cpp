#include "../Editor.hpp"
#include "../../core/Logger.hpp"

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
                        Logger::Event("EDITOR", "Compile & Reload requested for project '%s'", pm.GetCurrentProjectPath().c_str());
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
                if (ImGui::MenuItem("Tilemap"))         CreateEntityAt("Tilemap",   editorCamera.target);
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
                Logger::Event("EDITOR", "Play started (%d entities)", (int)scene.entities.size());
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
                Logger::Event("EDITOR", "Play stopped");
                Events::Publish(GameEvents::GameStopEvent{});
                scene.Restore();
                state = GameState::Editor;
                selectedIndex = -1;
                multiSelection_.clear();
                StoryState::Get().Clear();
                EventBus::Get().Clear();
                QuestManager::Get().Reset();
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
