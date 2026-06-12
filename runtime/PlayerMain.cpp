/**********************************************************************************************
*
*   Indium Player — the standalone game runtime.
*
*   Boots a project directly into Play with no editor, no ImGui and no script
*   compiler: it loads the prebuilt script library, the project's default scene,
*   and runs the same world update + lighting + post-fx + screen-space UI passes
*   the editor viewport runs (shared via core/RuntimeUI.hpp / core/LightMapRenderer.hpp),
*   full-window.
*
*   Configuration comes from a game.json next to the executable (written by the
*   editor's File > Export Game), or a project path can be passed directly on the
*   command line for quick editor-less test runs:
*
*       IndiumPlayer ~/IndiumProjects/MyGame
*
*   game.json shape (all fields optional):
*       { "title": "My Game", "width": 1280, "height": 720,
*         "targetFps": 60, "project": "data", "showTitle": true }
*
*   Copyright (c) 2026
*
**********************************************************************************************/

#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../include/nlohmann/json.hpp"
#include "../core/scene/Scene.hpp"
#include "../core/ProjectManager.hpp"
#include "../core/InputManager.hpp"
#include "../core/StoryState.hpp"
#include "../core/QuestManager.hpp"
#include "../core/ItemManager.hpp"
#include "../core/DialogueManager.hpp"
#include "../core/CutsceneManager.hpp"
#include "../core/SaveManager.hpp"
#include "../core/GameSettings.hpp"
#include "../core/MenuManager.hpp"
#include "../core/Screen.hpp"
#include "../core/RuntimeUI.hpp"
#include "../core/LightMapRenderer.hpp"
#include "../core/PrefabManager.hpp"
#include "../2D/entity/EntityFactory.hpp"
#include "../2D/component/CameraComponent.hpp"
#include "../2D/component/PostProcessComponent.hpp"
#include "../editor/PostProcessManager.hpp"   // raylib-only (no ImGui), shared with the editor

extern "C" void InstallCrashHandler();        // core/CrashHandler.cpp

namespace
{
    using Indium::Scene;

    /** @brief Player window/boot settings, read from game.json next to the exe. */
    struct GameConfig
    {
        std::string title     = "Indium Game";
        int         width     = 1280;
        int         height    = 720;
        int         targetFps = 60;
        std::string project   = "data";   // project dir, relative to the exe
        bool        showTitle = true;     // open the title menu at boot

        static GameConfig Load(const std::string& path)
        {
            GameConfig c;
            std::ifstream f(path);
            if (!f.is_open()) return c;
            try
            {
                nlohmann::json j = nlohmann::json::parse(f);
                c.title     = j.value("title",     c.title);
                c.width     = j.value("width",     c.width);
                c.height    = j.value("height",    c.height);
                c.targetFps = j.value("targetFps", c.targetFps);
                c.project   = j.value("project",   c.project);
                c.showTitle = j.value("showTitle", c.showTitle);
            }
            catch (const std::exception& e)
            {
                TraceLog(LOG_WARNING, "PLAYER: malformed game.json (%s), using defaults", e.what());
            }
            return c;
        }
    };

    /** @brief The primary CameraComponent's camera, centered in the window — the
     *  player-side mirror of Editor::GetGameCamera. Falls back to the scene's saved
     *  editor view so a camera-less scene still frames what the author last saw. */
    Camera2D GameCamera(const Scene& scene, int w, int h)
    {
        Camera2D cam  = { 0 };
        cam.target    = scene.editorCameraTarget;
        cam.offset    = { (float)w * 0.5f, (float)h * 0.5f };
        cam.zoom      = (scene.editorCameraZoom > 0.0f) ? scene.editorCameraZoom : 1.0f;
        cam.rotation  = 0.0f;

        for (const auto& e : scene.entities)
        {
            for (const auto& c : e->components)
            {
                const auto* camComp = dynamic_cast<const Indium::CameraComponent*>(c.get());
                if (!camComp || !camComp->isPrimary) continue;
                cam.target   = e->getGlobalPosition();
                cam.offset   = { (float)w * 0.5f + camComp->GetShakeOffset().x,
                                 (float)h * 0.5f + camComp->GetShakeOffset().y };
                cam.zoom     = camComp->zoom;
                cam.rotation = camComp->GetEffectiveRotation() + camComp->GetShakeAngle();
                return cam;
            }
        }
        return cam;
    }

    /** @brief Player-side mirror of Editor::DrainPendingSceneLoad: runs a script /
     *  save-restore scene switch, autosaving the outgoing scene first on gameplay
     *  switches (never when the switch is itself a save-restore load). */
    void DrainPendingSceneLoad(Indium::ProjectManager& pm, Scene& scene)
    {
        if (scene._pendingSceneLoad.empty()) return;

        if (Indium::SaveManager::AutosaveEnabled() && Indium::SaveManager::AutosaveOnSceneSwitch()
            && !scene._hasPendingRestore)
            Indium::SaveManager::Save(scene, Indium::SaveManager::AutosaveSlot());

        std::string target = scene._pendingSceneLoad;
        scene._pendingSceneLoad.clear();
        pm.SwitchScene(scene, target);
    }

    /** @brief Blocking "could not start" screen so a broken install reports itself
     *  instead of flashing a window shut. */
    void FatalScreen(const std::string& message)
    {
        while (!WindowShouldClose())
        {
            BeginDrawing();
                ClearBackground(Color{ 16, 16, 20, 255 });
                const char* head = "Failed to start game";
                int hw = MeasureText(head, 28);
                DrawText(head, (GetScreenWidth() - hw) / 2, GetScreenHeight() / 2 - 40, 28, Color{ 230, 90, 90, 255 });
                int mw = MeasureText(message.c_str(), 16);
                DrawText(message.c_str(), (GetScreenWidth() - mw) / 2, GetScreenHeight() / 2 + 8, 16, RAYWHITE);
            EndDrawing();
        }
    }
}

int main(int argc, char** argv)
{
    InstallCrashHandler();   // capture unhandled crashes to crash.log

    namespace fs = std::filesystem;
    const std::string appDir = []
    {
        // GetApplicationDirectory needs no window; capture before InitWindow for clarity.
        const char* d = GetApplicationDirectory();
        return std::string(d ? d : "./");
    }();

    GameConfig cfg = GameConfig::Load(appDir + "game.json");

    // Resolve the project: CLI argument (dev runs) beats game.json (exported layout).
    std::string projectPath;
    if (argc > 1) projectPath = argv[1];
    else
    {
        fs::path p(cfg.project);
        projectPath = p.is_absolute() ? p.string() : (fs::path(appDir) / p).string();
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(cfg.width, cfg.height, cfg.title.c_str());
    InitAudioDevice();
    SetExitKey(KEY_NULL);            // Esc belongs to the pause-menu layer, not the window
    SetTargetFPS(cfg.targetFps);

    using namespace Indium;

    ProjectManager pm;
    EntityFactory  factory;

    // Scene lives on the heap-of-main, destroyed before the static ScriptManager
    // unloads the script library — NativeScript destructors must run while their
    // code is still mapped.
    Scene scene;

    if (!fs::exists(fs::path(projectPath) / "project.indp"))
    {
        FatalScreen("No project found at: " + projectPath);
        CloseAudioDevice(); CloseWindow();
        return 1;
    }

    // Loads tags + the prebuilt script library + the default scene, and points
    // ScriptManager / AssetManager at the project root.
    if (!pm.LoadProject(projectPath, scene))
    {
        FatalScreen("Project failed to load: " + projectPath);
        CloseAudioDevice(); CloseWindow();
        return 1;
    }

    // Script-spawned prefabs (NativeScript::Instantiate("name")) — same hook the
    // editor installs in Editor::Init.
    NativeScript::s_prefabLoader = [&pm, &factory](const std::string& name, Scene* sc) -> Entity*
    {
        if (!pm.IsProjectOpen()) return nullptr;
        std::string path = pm.GetCurrentProjectPath() + "/prefabs/" + name + ".prefab";
        nlohmann::json j = PrefabManager::Load(path);
        if (j.is_null()) return nullptr;
        auto e = factory.LoadEntity(j);
        if (!e) return nullptr;
        e->id = sc->nextEntityId++;
        return sc->Instantiate(std::move(e));
    };

    InputManager::Get().Load(pm.GetCurrentProjectPath() + "/input.json");

    // --- Play start: the same singleton wiring the editor performs on ▶ Play
    // (editor/panels/MainMenuBar.cpp) — keep the two in sync. ---
    StoryState::Get().Clear();
    StoryState::Get().Seed(scene.storyState);
    StoryState::Get().SubscribeToEvents();
    QuestManager::Get().SetProjectPath(pm.GetCurrentProjectPath());
    QuestManager::Get().LoadAll();
    QuestManager::Get().SubscribeToEvents();
    CutsceneManager::Get().SetProjectPath(pm.GetCurrentProjectPath());
    ItemManager::Get().SetProjectPath(pm.GetCurrentProjectPath());
    ItemManager::Get().LoadAll();
    DialogueManager::Get().SetProjectPath(pm.GetCurrentProjectPath());
    SaveManager::SetProjectPath(pm.GetCurrentProjectPath());
    SaveManager::ResetAutosaveConfig();
    SaveManager::SubscribeToEvents();
    MenuManager::Get().Reset();
    MenuManager::Get().SetTitle(pm.GetCurrentProjectName());
    MenuManager::Get().SetStandalone(true);   // adds Quit to the title screen
    GameSettings::Get().SetProjectPath(pm.GetCurrentProjectPath());
    GameSettings::Get().Load();
    Screen::Get().SetDebugGizmos(false);      // collider/trigger gizmos are editor-only

    // Title-first boot (after SaveManager has its path: OpenTitle reads the slots
    // for Continue / Load Game). Scripts may also re-open it from OnStart.
    if (cfg.showTitle) MenuManager::Get().OpenTitle();

    // Snapshot raw component pointers BEFORE awake()/start(): a script's OnStart may
    // AddComponent<>() and reallocate e->components mid-iteration (same trap the
    // editor documents at its Play handler). Heap Components never move.
    {
        std::vector<Component*> startComps;
        for (auto& e : scene.entities) for (auto& c : e->components) startComps.push_back(c.get());
        for (auto* c : startComps) c->awake(&scene);
        for (auto* c : startComps) c->start(&scene);
    }
    Events::Publish(GameEvents::GameStartEvent{});

    // --- Render targets: full-window world texture + lighting + post fx ---
    RenderTexture2D    worldRT = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    LightMapRenderer   lighting;
    PostProcessManager postFx;
    lighting.Init();
    postFx.Init();

    while (!WindowShouldClose() && !MenuManager::Get().QuitRequested())
    {
        const float dt = GetFrameTime();
        const int   sw = GetScreenWidth();
        const int   sh = GetScreenHeight();

        // ---------- Update (mirrors the Play branch of Editor::Update) ----------
        const bool menuBlocked = MenuManager::Get().BlocksGameplay();
        Screen::Get().SetTicking(!menuBlocked);

        // Drain deferred save/load work queued by the runtime UI last frame. This
        // runs even while a menu blocks gameplay — the save/load menu is by
        // definition open when one of these is pending.
        if (SaveManager::ConsumeAutosaveRequest())
            SaveManager::Save(scene, SaveManager::AutosaveSlot());

        const MenuManager::MenuAction act = MenuManager::Get().TakePendingAction();
        if (act.type == MenuManager::MenuAction::Type::Save)
        {
            SaveManager::Save(scene, act.slot);
            MenuManager::Get().RefreshSlots();
        }
        else if (act.type == MenuManager::MenuAction::Type::Load)
        {
            if (SaveManager::Load(scene, act.slot))     // queues restore + _pendingSceneLoad
            {
                MenuManager::Get().Reset();
                DrainPendingSceneLoad(pm, scene);       // switch now — never a frame late
            }
        }

        if (!menuBlocked)
        {
            // Camera bounds clamp is viewport-aware; here the viewport is the window.
            for (auto& e : scene.entities)
                for (auto& c : e->components)
                    if (auto* cam = dynamic_cast<CameraComponent*>(c.get()))
                        cam->viewportPx_ = { (float)sw, (float)sh };

            scene.Update(dt);

            // Cutscenes tick AFTER the scene (final say on driven entities) and on the
            // raw dt so a gameplay-freezing cutscene (Time::scale = 0) still plays.
            CutsceneManager::Get().Update(dt, &scene);

            DrainPendingSceneLoad(pm, scene);
        }

        // ---------- Render (mirrors the viewport pass of Editor::Run) ----------
        if (sw > 0 && sh > 0 && (sw != worldRT.texture.width || sh != worldRT.texture.height))
        {
            UnloadRenderTexture(worldRT);
            worldRT = LoadRenderTexture(sw, sh);
            lighting.Resize(sw, sh);
            postFx.Resize(sw, sh);
        }

        const Camera2D cam            = GameCamera(scene, sw, sh);
        const bool     lightingActive = LightMapRenderer::SceneWantsLighting(scene);
        if (lightingActive) lighting.Render(scene, cam);   // own RT scope — before worldRT's

        BeginTextureMode(worldRT);
            ClearBackground(BLACK);
            scene.Draw(cam);
            if (lightingActive) lighting.Composite(worldRT.texture.width, worldRT.texture.height);

            // The shared in-game UI pass: OnGUI hooks, dialogue, quest log, inventory,
            // Esc routing and the front-end menu — identical to the editor viewport.
            RuntimeUI::Draw(scene, worldRT.texture.width, worldRT.texture.height,
                            GetMousePosition(), IsWindowFocused());
        EndTextureMode();

        // Post-processing chain (PostProcessComponents), applied over world + UI —
        // same order as the editor, so exports look like the viewport did.
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
            if (!activeFx.empty()) postFx.Apply(worldRT, activeFx);
        }

        BeginDrawing();
            ClearBackground(BLACK);
            // Render texture → window, height flipped to undo raylib's bottom-up FBO.
            DrawTexturePro(worldRT.texture,
                ::Rectangle{ 0.0f, 0.0f, (float)worldRT.texture.width, -(float)worldRT.texture.height },
                ::Rectangle{ 0.0f, 0.0f, (float)sw, (float)sh },
                Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
        EndDrawing();
    }

    // ---------- Shutdown ----------
    Events::Publish(GameEvents::GameStopEvent{});
    if (GameSettings::Get().IsDirty()) GameSettings::Get().Save();

    // Entities (and their NativeScripts) go before the asset caches and the audio
    // device — the same teardown order Editor::Shutdown documents.
    scene.entities.clear();
    scene.snapshot.clear();
    EventBus::Get().Clear();

    lighting.Shutdown();
    postFx.Shutdown();
    UnloadRenderTexture(worldRT);
    AssetManager::Get().Clear();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
