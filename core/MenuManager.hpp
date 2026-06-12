#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include "raylib.h"
#include "Screen.hpp"
#include "GUI.hpp"
#include "AudioMixer.hpp"
#include "InputManager.hpp"
#include "GameSettings.hpp"
#include "SaveManager.hpp"

namespace Indium
{
    /**
     * @brief In-game front-end: title screen, pause menu, a settings page exposing
     * the two player-facing backends (AudioMixer volumes + InputManager rebinds),
     * and Save/Load Game slot pages over SaveManager (manual slots + the autosave
     * slot, two-step overwrite/delete confirms, "Continue" loading the newest save).
     * Save/load actions are deferred via TakePendingAction — the editor drains them
     * with Scene access each frame. Scripts can tailor the offering in OnStart:
     *   MenuManager::Get().SetAllowManualSave(false);  // checkpoint-only saves
     *   MenuManager::Get().SetManualSlotCount(6);
     *
     * Header-only singleton shared across the script dylib boundary, like
     * DialogueManager / QuestManager / ItemManager. The editor draws it LAST in the
     * runtime screen-space UI pass (Editor::DrawRuntimeUI) so it sits above the
     * dialogue box / quest log / HUD, and freezes the world while a page is open
     * (see BlocksGameplay, gating scene.Update in Editor::Update).
     *
     * From a script:
     *   MenuManager::Get().OpenTitle();   // e.g. in OnStart() for a title-first game
     *   MenuManager::Get().OpenPause();   // Esc during Play does this automatically
     *
     * Esc routing lives in the editor (one authority, see Editor::DrawRuntimeUI):
     * an open menu consumes Esc via OnEscape() — cancel rebind > back out of
     * settings > resume — before cutscene-skip or open-pause are considered.
     */
    class MenuManager
    {
    public:
        // "Page", not "Screen": Screen is the viewport-metrics singleton.
        enum class Page { None, Title, Pause, Settings, SaveGame, LoadGame };

        /**
         * @brief A save/load the player asked for from a menu. Deferred: the menu has
         * no Scene access, so the editor drains this each frame (TakePendingAction)
         * and performs the SaveManager call — the same shape as Scene::_pendingSceneLoad.
         */
        struct MenuAction
        {
            enum class Type { None, Save, Load };
            Type type = Type::None;
            int  slot = 0;
        };

        MenuManager(const MenuManager&)            = delete;
        MenuManager& operator=(const MenuManager&) = delete;
        MenuManager(MenuManager&&)                 = delete;
        MenuManager& operator=(MenuManager&&)      = delete;

        static MenuManager& Get() { static MenuManager inst; return inst; }

        // --- State transitions (pure; exercised directly by tests) ---------------

        void OpenTitle()    { page_ = Page::Title;  rebinding_.clear(); RefreshSlots(); }
        void OpenPause()    { page_ = Page::Pause;  rebinding_.clear(); }
        /** @brief Opens settings, remembering the page to return to on Back/Esc
         *  (Title or Pause; None if opened from a script with nothing up). */
        void OpenSettings() { if (page_ != Page::Settings) returnTo_ = page_; page_ = Page::Settings; rebinding_.clear(); }
        /** @brief Opens the manual-save slot page (from Pause). Same returnTo_
         *  convention as Settings — slot pages only open from Title/Pause, never
         *  from Settings or each other, so the single field can't conflict. */
        void OpenSaveGame() { if (page_ != Page::SaveGame) returnTo_ = page_; page_ = Page::SaveGame; rebinding_.clear(); confirmSlot_ = -1; RefreshSlots(); }
        void OpenLoadGame() { if (page_ != Page::LoadGame) returnTo_ = page_; page_ = Page::LoadGame; rebinding_.clear(); confirmSlot_ = -1; RefreshSlots(); }
        void Resume()       { page_ = Page::None;   rebinding_.clear(); }
        void Close()        { Resume(); }
        /** @brief Hard reset for Play start / Stop — no stale page, armed rebind,
         *  pending save/load, or per-game menu customization. */
        void Reset()
        {
            page_ = Page::None; returnTo_ = Page::None; rebinding_.clear();
            pending_ = {}; confirmSlot_ = -1; confirmIsDelete_ = false; slots_.clear();
            allowManualSave_ = true; allowLoad_ = true; manualSlots_ = 3;
        }

        // --- Save/load plumbing (drained by the editor each frame) ---------------

        void RequestSave(int slot) { pending_ = { MenuAction::Type::Save, slot }; }
        void RequestLoad(int slot) { pending_ = { MenuAction::Type::Load, slot }; }
        /** @brief Returns the queued save/load (if any) and clears it. */
        [[nodiscard]] MenuAction TakePendingAction() { MenuAction a = pending_; pending_ = {}; return a; }

        /** @brief Re-reads slot metadata from disk. Called on page open and by the
         *  editor after it executes a save, so timestamps shown are current. */
        void RefreshSlots() { slots_ = SaveManager::ListSlots(); }

        // --- Per-game menu customization (script-facing; Reset() restores defaults,
        //     which runs at Play start, so configure these in OnStart) ---------------

        /** @brief false = no "Save Game" in the pause menu (checkpoint/autosave-only games). */
        void SetAllowManualSave(bool on) { allowManualSave_ = on; }
        [[nodiscard]] bool AllowManualSave() const { return allowManualSave_; }
        /** @brief false = no "Load Game"/"Continue" anywhere (e.g. roguelikes). */
        void SetAllowLoad(bool on) { allowLoad_ = on; }
        [[nodiscard]] bool AllowLoad() const { return allowLoad_; }
        /** @brief Number of manual slots on the Save/Load pages (clamped 1..8). Default 3. */
        void SetManualSlotCount(int n) { manualSlots_ = std::clamp(n, 1, 8); }
        [[nodiscard]] int ManualSlotCount() const { return manualSlots_; }

        [[nodiscard]] bool IsOpen()  const { return page_ != Page::None; }
        [[nodiscard]] Page Current() const { return page_; }

        /** @brief True while any page is up: the editor freezes scene.Update /
         *  cutscenes and withholds input from the gameplay UI underneath. */
        [[nodiscard]] bool BlocksGameplay() const { return page_ != Page::None; }

        /**
         * @brief Layered Esc, innermost first: an armed rebind is cancelled (Esc can
         * never be captured as a binding), then Settings backs out to its opener
         * (saving), a slot page disarms its overwrite/delete confirm or backs out,
         * then Pause resumes. The title screen consumes Esc but stays put.
         * Returns whether the press was consumed; false = nothing open, the editor
         * opens the pause menu instead.
         */
        bool OnEscape()
        {
            switch (page_)
            {
                case Page::Settings:
                    if (!rebinding_.empty()) { rebinding_.clear(); return true; }
                    CloseSettings();
                    return true;
                case Page::SaveGame:
                case Page::LoadGame:
                    if (confirmSlot_ >= 0) { confirmSlot_ = -1; return true; }
                    CloseSlotsPage();
                    return true;
                case Page::Pause: Resume(); return true;
                case Page::Title: return true;
                case Page::None:  return false;
            }
            return false;
        }

        // --- Rebinding (public so the Esc semantics are testable without a window) ---

        void StartRebind(const std::string& action) { rebinding_ = action; }
        void CancelRebind()                         { rebinding_.clear(); }
        [[nodiscard]] const std::string& RebindingAction() const { return rebinding_; }

        /** @brief Heading on the title screen. The editor sets the project name at Play. */
        void SetTitle(const std::string& t) { title_ = t.empty() ? "Indium" : t; }

        // --- Drawing (runtime UI pass only) ---------------------------------------

        /**
         * @brief Draws the current page over a dim layer, centered via Screen metrics.
         * `mouseLive` mirrors the editor's screenAccept (Play + viewport hovered);
         * Screen's pressed/down are already gated by it, so widgets self-disable —
         * it additionally gates button activation and rebind key capture here.
         */
        void DrawGUI(bool mouseLive)
        {
            if (page_ == Page::None) return;
            const float W = (float)Screen::Width();
            const float H = (float)Screen::Height();
            if (W <= 0.0f || H <= 0.0f) return;

            DrawRectangle(0, 0, (int)W, (int)H, Color{ 0, 0, 0, 150 });

            switch (page_)
            {
                case Page::Title:    DrawTitlePage(W, H, mouseLive);          break;
                case Page::Pause:    DrawPausePage(W, H, mouseLive);          break;
                case Page::Settings: DrawSettingsPage(W, H, mouseLive);       break;
                case Page::SaveGame: DrawSlotsPage(W, H, mouseLive, true);    break;
                case Page::LoadGame: DrawSlotsPage(W, H, mouseLive, false);   break;
                default: break;
            }
        }

    private:
        MenuManager()  = default;
        ~MenuManager() = default;

        /** @brief Back/Esc out of settings: persist changes (if any) and return to the opener. */
        void CloseSettings()
        {
            rebinding_.clear();
            if (GameSettings::Get().IsDirty()) GameSettings::Get().Save();
            page_ = returnTo_;
        }

        /** @brief Back/Esc out of a slot page: drop any armed confirm, return to the opener. */
        void CloseSlotsPage()
        {
            confirmSlot_     = -1;
            confirmIsDelete_ = false;
            page_            = returnTo_;
        }

        static constexpr Color kHeading = { 230, 210, 140, 255 };
        static constexpr Color kDim     = { 170, 170, 185, 255 };
        static constexpr Color kAccent  = { 140, 170, 230, 255 };

        void DrawTitlePage(float W, float H, bool live)
        {
            const int   ts = 48;
            const float tw = (float)MeasureText(title_.c_str(), ts);
            GUI::Label(title_.c_str(), (W - tw) * 0.5f, H * 0.26f, ts, RAYWHITE);

            // Continue / Load Game appear only when loading is allowed AND at least
            // one save exists — a fresh game's title screen looks exactly as before.
            const bool  showLoad = allowLoad_ && !slots_.empty();
            const float bw = 220.0f, bh = 42.0f, gap = 12.0f;
            const float x  = (W - bw) * 0.5f;
            float       y  = showLoad ? H * 0.40f : H * 0.48f;
            if (showLoad)
            {
                if (GUI::Button(::Rectangle{ x, y, bw, bh }, "Continue") && live)
                    RequestLoad(SaveManager::MostRecentSlot());
                y += bh + gap;
                if (GUI::Button(::Rectangle{ x, y, bw, bh }, "Load Game") && live) OpenLoadGame();
                y += bh + gap;
            }
            if (GUI::Button(::Rectangle{ x, y, bw, bh }, "Start") && live)    Close();
            y += bh + gap;
            if (GUI::Button(::Rectangle{ x, y, bw, bh }, "Settings") && live) OpenSettings();
            // No Quit in-editor: the window belongs to the editor. Standalone export adds it.
        }

        void DrawPausePage(float W, float H, bool live)
        {
            const int   hs = 36;
            const float hw = (float)MeasureText("Paused", hs);
            GUI::Label("Paused", (W - hw) * 0.5f, H * 0.28f, hs, RAYWHITE);

            const float bw = 220.0f, bh = 42.0f, gap = 12.0f;
            const float x  = (W - bw) * 0.5f;
            const int   extra = (allowManualSave_ ? 1 : 0) + (allowLoad_ ? 1 : 0);
            float       y  = H * 0.42f - (float)extra * (bh + gap) * 0.5f;
            if (GUI::Button(::Rectangle{ x, y, bw, bh }, "Resume") && live)    Resume();
            y += bh + gap;
            if (allowManualSave_)
            {
                if (GUI::Button(::Rectangle{ x, y, bw, bh }, "Save Game") && live) OpenSaveGame();
                y += bh + gap;
            }
            if (allowLoad_)
            {
                if (GUI::Button(::Rectangle{ x, y, bw, bh }, "Load Game") && live) OpenLoadGame();
                y += bh + gap;
            }
            if (GUI::Button(::Rectangle{ x, y, bw, bh }, "Settings") && live)  OpenSettings();
            y += bh + gap;
            if (GUI::Button(::Rectangle{ x, y, bw, bh }, "Main Menu") && live) OpenTitle();
        }

        void DrawSettingsPage(float W, float H, bool live)
        {
            // While a rebind is armed, menu clicks are swallowed so a stray click can't
            // press buttons (or re-arm another row) mid-capture; Esc cancels via OnEscape.
            const bool clicksLive = live && rebinding_.empty();

            const float pw = std::min(560.0f, W - 40.0f);
            const float x  = (W - pw) * 0.5f;
            float       y  = H * 0.08f;

            GUI::Label("Settings", x, y, 32, RAYWHITE); y += 48.0f;

            // --- Audio: master + per-bus volume sliders, live on the mixer ---------
            GUI::Label("Audio", x, y, 22, kHeading); y += 32.0f;

            auto& mix = AudioMixer::Get();
            auto volumeRow = [&](const char* name, float v) -> float
            {
                GUI::Label(name, x, y + 3.0f, 18, RAYWHITE);
                float nv = GUI::Slider(::Rectangle{ x + 150.0f, y, pw - 150.0f - 64.0f, 24.0f }, v);
                if (!clicksLive) nv = v;
                char pct[8];
                snprintf(pct, sizeof(pct), "%d%%", (int)(nv * 100.0f + 0.5f));
                GUI::Label(pct, x + pw - 52.0f, y + 3.0f, 18, kDim);
                y += 32.0f;
                return nv;
            };

            float nm = volumeRow("Master", std::clamp(mix.master, 0.0f, 1.0f));
            if (nm != mix.master) { mix.master = nm; GameSettings::Get().MarkDirty(); }
            for (const auto& name : mix.BusNames())
            {
                const float v  = mix.BusVolume(name);
                const float nv = volumeRow(name.c_str(), v);
                if (nv != v) { mix.SetBusVolume(name, nv); GameSettings::Get().MarkDirty(); }
            }

            // --- Controls: one row per action, name + current binding + Rebind -----
            y += 10.0f;
            GUI::Label("Controls", x, y, 22, kHeading); y += 32.0f;

            auto& actions = InputManager::Get().GetActions();
            std::vector<std::string> names;
            names.reserve(actions.size());
            for (const auto& [name, _] : actions) names.push_back(name);
            std::sort(names.begin(), names.end());

            const float backTop = H - 64.0f;
            size_t      shown   = 0;
            for (const auto& name : names)
            {
                if (y + 34.0f > backTop - 8.0f)   // simple overflow clamp for this slice
                {
                    char more[48];
                    snprintf(more, sizeof(more), "(+%d more)", (int)(names.size() - shown));
                    GUI::Label(more, x, y + 3.0f, 16, kDim);
                    break;
                }
                const ActionBinding& b         = actions[name];
                const bool           rebinding = (rebinding_ == name);
                GUI::Label(name.c_str(), x, y + 6.0f, 18, RAYWHITE);
                const std::string bind = rebinding ? "Press a key... (Esc cancels)"
                                                   : InputManager::BindingName(b);
                GUI::Label(bind.c_str(), x + 180.0f, y + 6.0f, 18, rebinding ? kAccent : kDim);
                if (GUI::Button(::Rectangle{ x + pw - 96.0f, y, 96.0f, 28.0f },
                                rebinding ? "..." : "Rebind", 16) && clicksLive)
                    rebinding_ = name;
                y += 34.0f;
                ++shown;
            }

            // Key capture for an armed rebind. Esc is skipped here on purpose — the
            // editor's Esc layer routes it to OnEscape(), which cancels the rebind —
            // so Escape can never be captured as a binding. Drain the whole queue so
            // only the first usable key this frame wins.
            if (!rebinding_.empty() && live)
            {
                for (int k = GetKeyPressed(); k != 0; k = GetKeyPressed())
                {
                    if (k == KEY_ESCAPE) continue;
                    auto it = actions.find(rebinding_);
                    if (it != actions.end())
                    {
                        it->second = ActionBinding{ k, -1, false };
                        GameSettings::Get().MarkDirty();
                    }
                    rebinding_.clear();
                    break;
                }
            }

            if (GUI::Button(::Rectangle{ x, backTop, 160.0f, 36.0f }, "Back") && clicksLive)
                CloseSettings();
        }

        /** @brief Cached metadata for a slot, or nullptr if it has no save on disk. */
        const SaveManager::SaveSlotInfo* SlotInfo(int slot) const
        {
            for (const auto& s : slots_)
                if (s.slot == slot) return &s;
            return nullptr;
        }

        /**
         * @brief Shared Save Game / Load Game page. saveMode=true lists the manual
         * slots with [Save] buttons (two-step "Overwrite?" confirm on occupied ones);
         * saveMode=false lists Autosave + the manual slots with [Load] and a two-step
         * [X] delete. Deletes run right here — pure filesystem, no Scene needed (the
         * same way Settings rebinds mutate InputManager mid-draw) — while save/load
         * are queued for the editor via Request{Save,Load}.
         */
        void DrawSlotsPage(float W, float H, bool live, bool saveMode)
        {
            const float pw = std::min(560.0f, W - 40.0f);
            const float x  = (W - pw) * 0.5f;
            float       y  = H * 0.08f;

            GUI::Label(saveMode ? "Save Game" : "Load Game", x, y, 32, RAYWHITE); y += 52.0f;

            const float rowH = 40.0f, gap = 8.0f;
            const float backTop = H - 64.0f;

            auto drawRow = [&](int slot)
            {
                if (y + rowH > backTop - 8.0f) return; // overflow clamp, like Settings
                const SaveManager::SaveSlotInfo* info = SlotInfo(slot);

                char name[24];
                if (slot == SaveManager::kAutosaveSlot) snprintf(name, sizeof(name), "Autosave");
                else                                    snprintf(name, sizeof(name), "Slot %d", slot);

                GUI::Box(::Rectangle{ x, y, pw, rowH }, Color{ 20, 20, 24, 200 },
                         Color{ 80, 80, 92, 255 }, 1.0f);
                GUI::Label(name, x + 12.0f, y + 10.0f, 18, RAYWHITE);

                if (info)
                {
                    const std::string when = SaveManager::FormatTimestamp(info->savedAt);
                    GUI::Label(info->scene.empty() ? "?" : info->scene.c_str(),
                               x + 120.0f, y + 11.0f, 16, kAccent);
                    GUI::Label(when.empty() ? "—" : when.c_str(), x + 250.0f, y + 11.0f, 16, kDim);
                }
                else
                {
                    GUI::Label("Empty", x + 120.0f, y + 11.0f, 16, kDim);
                }

                const bool confirming = (confirmSlot_ == slot);
                if (saveMode)
                {
                    const char* label = (info && confirming && !confirmIsDelete_) ? "Overwrite?" : "Save";
                    if (GUI::Button(::Rectangle{ x + pw - 116.0f, y + 5.0f, 104.0f, 30.0f }, label, 16) && live)
                    {
                        if (!info || (confirming && !confirmIsDelete_))
                        {
                            RequestSave(slot);
                            confirmSlot_ = -1;
                        }
                        else { confirmSlot_ = slot; confirmIsDelete_ = false; }
                    }
                }
                else if (info)
                {
                    if (GUI::Button(::Rectangle{ x + pw - 160.0f, y + 5.0f, 104.0f, 30.0f }, "Load", 16) && live)
                        RequestLoad(slot);
                    const char* delLabel = (confirming && confirmIsDelete_) ? "!" : "X";
                    if (GUI::Button(::Rectangle{ x + pw - 46.0f, y + 5.0f, 34.0f, 30.0f }, delLabel, 16) && live)
                    {
                        if (confirming && confirmIsDelete_)
                        {
                            SaveManager::DeleteSlot(slot);
                            confirmSlot_ = -1;
                            RefreshSlots();
                        }
                        else { confirmSlot_ = slot; confirmIsDelete_ = true; }
                    }
                }
                y += rowH + gap;
            };

            if (!saveMode) drawRow(SaveManager::kAutosaveSlot);
            for (int s = 1; s <= manualSlots_; ++s) drawRow(s);

            if (GUI::Button(::Rectangle{ x, backTop, 160.0f, 36.0f }, "Back") && live)
                CloseSlotsPage();
        }

        Page        page_     = Page::None;
        Page        returnTo_ = Page::None;
        std::string rebinding_;            // action awaiting a key, or empty
        std::string title_ = "Indium";

        // Save/load state
        MenuAction                             pending_;                 // drained by the editor
        std::vector<SaveManager::SaveSlotInfo> slots_;                   // cached ListSlots()
        int                                    confirmSlot_     = -1;    // armed two-step confirm
        bool                                   confirmIsDelete_ = false; // overwrite vs delete
        bool                                   allowManualSave_ = true;
        bool                                   allowLoad_       = true;
        int                                    manualSlots_     = 3;
    };
}
