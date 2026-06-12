#pragma once
#include "raylib.h"
#include "scene/Scene.hpp"
#include "Screen.hpp"
#include "DialogueManager.hpp"
#include "QuestManager.hpp"
#include "ItemManager.hpp"
#include "CutsceneManager.hpp"
#include "MenuManager.hpp"

namespace Indium
{
    /**
     * @brief The in-game screen-space UI pass, shared by the editor viewport
     * (Editor::DrawRuntimeUI) and the standalone player so the game looks and
     * behaves identically in both hosts.
     *
     * Layers, in order: cutscene letterbox, per-component onGUI (e.g. the
     * PlayerInteractor prompt and script OnGUI HUDs), the dialogue box, the quest
     * log, the inventory HUD, the cutscene skip prompt, the Esc routing, and the
     * front-end menu on top. While a menu page is open the component onGUI pass is
     * skipped entirely (scripts hit-test Screen directly, so they can't be gated by
     * a parameter) and the gameplay UI gets no input.
     *
     * Must be called inside the host's render-target scope (the editor's viewport
     * texture / the player's window-sized texture), after the world + light
     * composite.
     *
     * @param texW / texH     Render-target pixel size — the space the UI lays out in.
     * @param uiMouse         Mouse position mapped into that same pixel space.
     * @param screenAccept    Whether game UI may take input this frame (the editor
     *                        passes Play && viewport-hovered; the player passes
     *                        window-focused). Pause keeps drawing with this false.
     */
    namespace RuntimeUI
    {
        inline void Draw(Scene& scene, int texW, int texH, Vector2 uiMouse, bool screenAccept)
        {
            const bool menuBlocked = MenuManager::Get().BlocksGameplay();
            const bool gameAccept  = screenAccept && !menuBlocked;
            Screen::Get().Set(texW, texH, uiMouse,
                              screenAccept && IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                              screenAccept && IsMouseButtonDown(MOUSE_BUTTON_LEFT));

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
            // prompt), then the dialogue box, the quest log, and the inventory HUD.
            // Script OnGUI hooks hit-test against Screen directly (no accept parameter
            // to gate them), so while a menu blocks gameplay the whole pass is skipped —
            // otherwise a HUD button would still fire through the menu's dim layer.
            if (!menuBlocked)
            {
                for (auto& e : scene.entities)
                    if (e->activeInHierarchy())
                        for (auto& c : e->components)
                            if (c->enabled) c->onGUI(&scene);
            }
            DialogueManager::Get().DrawGUI(gameAccept);
            QuestManager::Get().DrawLogGUI(gameAccept);
            ItemManager::Get().DrawInventoryGUI(gameAccept);

            // Skip prompt (kept in the top band so the dialogue box can't hide it),
            // hidden while a menu is up — Esc belongs to the menu then.
            if (!menuBlocked && CutsceneManager::Get().IsActive())
            {
                const char* msg = "Press [Esc] to skip";
                const int   fs  = 16;
                const int   tw  = MeasureText(msg, fs);
                DrawText(msg, texW - tw - 18, (int)(texH * 0.12f) + 8, fs, Color{ 210, 210, 220, 200 });
            }

            // Esc, layered innermost-first: an open menu consumes it (cancel rebind /
            // back out of settings / resume), then an active cutscene skips, then the
            // pause menu opens. MenuManager's rebind capture never binds Esc, so this
            // is the single authority for what Escape does during Play.
            if (screenAccept && IsKeyPressed(KEY_ESCAPE))
            {
                if      (MenuManager::Get().BlocksGameplay()) MenuManager::Get().OnEscape();
                else if (CutsceneManager::Get().IsActive())   CutsceneManager::Get().Skip();
                else                                          MenuManager::Get().OpenPause();
            }

            // The menu draws last so it sits above every other runtime UI layer.
            MenuManager::Get().DrawGUI(screenAccept);
        }
    }
}
