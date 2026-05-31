#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>
#include "raylib.h"
#include "EventBus.hpp"
#include "events/GameEvents.hpp"
#include "StoryState.hpp"
#include "Screen.hpp"
#include "GUI.hpp"
#include "InputManager.hpp"
#include "../include/nlohmann/json.hpp"

namespace Indium
{
    namespace GameEvents
    {
        /**
         * @brief Fired when a quest changes phase, so HUD / audio / logs can react
         * without polling. (StoryState already records the boolean flags; this carries
         * the structured "which quest / which objective" context.)
         */
        struct QuestEvent
        {
            enum class Type { Started, ObjectiveCompleted, Completed, Abandoned };
            Type        type;
            std::string questId;
            std::string objectiveId; // set for ObjectiveCompleted, empty otherwise
        };
    }

    /** @brief How a quest's objectives are structured. */
    enum class QuestMode { Sequential, Parallel };

    /** @brief A quest's lifecycle phase. Stored as an int in StoryState. */
    enum class QuestState { NotStarted = 0, Active = 1, Complete = 2 };

    /** @brief One objective within a quest. Done when its completeFlag is set in StoryState. */
    struct QuestObjective
    {
        std::string id;
        std::string desc;
        std::string completeFlag; // StoryState flag that marks this objective complete
    };

    /** @brief A quest definition, authored as <project>/quests/<id>.json. */
    struct QuestDef
    {
        std::string                 id;
        std::string                 title;
        std::string                 description;
        QuestMode                   mode      = QuestMode::Sequential;
        bool                        autoStart = false;
        std::vector<QuestObjective> objectives;
        std::string                 completeFlag;        // optional flag set when the quest completes
        std::vector<std::string>    rewards;             // optional flags set on completion
    };

    /**
     * @brief Global runtime quest tracker. Header-only singleton shared across the
     * script dylib boundary (like StoryState / DialogueManager).
     *
     * All progress lives in StoryState under the "quest." namespace, so it rides
     * Save/Load and per-scene seeding with no extra persistence code:
     *
     *   quest.<id>.state : int  (QuestState: 0 NotStarted, 1 Active, 2 Complete)
     *   quest.<id>.step  : int  (current objective index — Sequential mode only)
     *
     * Objective completion is *derived* from each objective's completeFlag, which is a
     * normal StoryState flag. Dialogue choices and InteractableComponents already set
     * flags (via NarrativeEvent), so quests advance with zero coupling back to those
     * systems: the tracker simply re-evaluates active quests whenever StoryState
     * changes. Scripts may also drive quests directly (Start/Advance/Complete/etc.).
     *
     * Data file (<project>/quests/<id>.json):
     *   {
     *     "id": "find_sword", "title": "The Lost Blade", "mode": "sequential",
     *     "objectives": [
     *       { "id": "talk_smith", "desc": "Speak to the blacksmith", "completeFlag": "talked_to_smith" },
     *       { "id": "get_blade",  "desc": "Find the blade",          "completeFlag": "has_blade" }
     *     ],
     *     "completeFlag": "quest_find_sword_done", "rewards": ["gold_given"]
     *   }
     */
    class QuestManager
    {
    public:
        QuestManager(const QuestManager&)            = delete;
        QuestManager& operator=(const QuestManager&) = delete;
        QuestManager(QuestManager&&)                 = delete;
        QuestManager& operator=(QuestManager&&)      = delete;

        static QuestManager& Get() { static QuestManager inst; return inst; }

        /** @brief Where quest files are loaded from. Set by the editor on project open. */
        void SetProjectPath(const std::string& path) { projectPath_ = path; }

        // --- Definition loading -------------------------------------------------

        /** @brief Loads every quest JSON in the <project>/quests folder into the table. */
        void LoadAll()
        {
            defs_.clear();
            if (projectPath_.empty()) return;
            std::error_code             ec;
            std::filesystem::path       dir = std::filesystem::path(projectPath_) / "quests";
            if (!std::filesystem::exists(dir, ec)) return;
            for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
            {
                if (ec) break;
                if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
                LoadFile(entry.path().string());
            }
        }

        /** @brief Loads a single quest by id from <project>/quests/<id>.json. */
        bool Load(const std::string& id)
        {
            if (projectPath_.empty()) { TraceLog(LOG_WARNING, "QUEST: no project path set"); return false; }
            const std::string path = (std::filesystem::path(projectPath_) / "quests" / (id + ".json")).string();
            return LoadFile(path);
        }

        /** @brief Parses a quest definition from JSON. Used by Load() and by tests. */
        bool LoadFromJson(const nlohmann::json& j)
        {
            QuestDef d = ParseDef(j);
            if (d.id.empty()) { TraceLog(LOG_WARNING, "QUEST: definition is missing 'id'"); return false; }
            defs_[d.id] = std::move(d);
            return true;
        }

        /** @brief All loaded definitions — used by the editor panel. */
        [[nodiscard]] const std::map<std::string, QuestDef>& Definitions() const { return defs_; }

        [[nodiscard]] const QuestDef* Definition(const std::string& id) const
        {
            auto it = defs_.find(id);
            return it == defs_.end() ? nullptr : &it->second;
        }

        // --- Runtime control (script + editor facing) --------------------------

        /** @brief Begins a quest: marks it Active at step 0 and fires QuestEvent::Started.
         *  No-op for an unknown or already-completed quest. */
        void Start(const std::string& id)
        {
            const QuestDef* d = Definition(id);
            if (!d)                                  { TraceLog(LOG_WARNING, "QUEST: Start unknown quest '%s'", id.c_str()); return; }
            if (StateOf(id) == QuestState::Complete) return;
            SetStateKey(id, QuestState::Active);
            StoryState::Get().Set(StepKey(id), 0);
            Events::Publish(GameEvents::QuestEvent{ GameEvents::QuestEvent::Type::Started, id, "" });
            Evaluate(); // a freshly started quest may already have objectives satisfied
        }

        /** @brief Marks one objective complete by setting its flag. Script-facing alternative
         *  to letting dialogue / interaction set the flag. The flag change re-evaluates quests. */
        void CompleteObjective(const std::string& id, const std::string& objectiveId)
        {
            const QuestDef* d = Definition(id);
            if (!d) return;
            for (const auto& o : d->objectives)
                if (o.id == objectiveId)
                {
                    if (!o.completeFlag.empty()) StoryState::Get().SetFlag(o.completeFlag);
                    return;
                }
        }

        /** @brief Sequential: advances to the next objective (completing the quest past the
         *  last). Parallel: re-evaluates. Lets a script step a quest without a flag. */
        void Advance(const std::string& id)
        {
            const QuestDef* d = Definition(id);
            if (!d || StateOf(id) != QuestState::Active) return;
            if (d->mode == QuestMode::Sequential)
            {
                const int step = StoryState::Get().GetInt(StepKey(id), 0) + 1;
                StoryState::Get().Set(StepKey(id), step);
                if (step >= (int)d->objectives.size()) Finish(*d);
            }
            else Evaluate();
        }

        /** @brief Force-completes a quest regardless of its objective flags. */
        void Complete(const std::string& id)
        {
            const QuestDef* d = Definition(id);
            if (d && StateOf(id) != QuestState::Complete) Finish(*d);
        }

        /** @brief Abandons a quest (back to NotStarted) and fires QuestEvent::Abandoned. */
        void Abandon(const std::string& id)
        {
            if (StateOf(id) == QuestState::NotStarted) return;
            SetStateKey(id, QuestState::NotStarted);
            StoryState::Get().Remove(StepKey(id));
            Events::Publish(GameEvents::QuestEvent{ GameEvents::QuestEvent::Type::Abandoned, id, "" });
        }

        // --- Queries -----------------------------------------------------------

        [[nodiscard]] QuestState StateOf(const std::string& id) const
        {
            return (QuestState)StoryState::Get().GetInt(StateKey(id), (int)QuestState::NotStarted);
        }
        [[nodiscard]] bool IsActive(const std::string& id)   const { return StateOf(id) == QuestState::Active; }
        [[nodiscard]] bool IsComplete(const std::string& id) const { return StateOf(id) == QuestState::Complete; }

        /** @brief Current objective index (Sequential). */
        [[nodiscard]] int CurrentStep(const std::string& id) const { return StoryState::Get().GetInt(StepKey(id), 0); }

        /** @brief True if the objective's completeFlag is set in StoryState. */
        [[nodiscard]] bool IsObjectiveDone(const QuestObjective& o) const
        {
            return !o.completeFlag.empty() && StoryState::Get().HasFlag(o.completeFlag);
        }

        /** @brief Ids of every quest currently Active. */
        [[nodiscard]] std::vector<std::string> ActiveQuests() const
        {
            std::vector<std::string> out;
            for (const auto& [id, d] : defs_)
                if (StateOf(id) == QuestState::Active) out.push_back(id);
            return out;
        }

        // --- Lifecycle (editor) ------------------------------------------------

        /** @brief (Re)establishes the StoryState subscription and starts any autoStart
         *  quests. Must be called on Play start: EventBus::Clear() on Stop wipes every
         *  channel, so this long-lived singleton would otherwise silently stop advancing
         *  quests on the next Play (mirrors StoryState::SubscribeToEvents). */
        void SubscribeToEvents()
        {
            // Move-assigning the handle unsubscribes any previous one, so this is idempotent.
            changeSub_ = Events::Subscribe<GameEvents::StoryStateChangedEvent>(
                [this](const GameEvents::StoryStateChangedEvent&) { Evaluate(); });
            for (auto& [id, d] : defs_)
                if (d.autoStart && StateOf(id) == QuestState::NotStarted) Start(id);
            Evaluate();
        }

        /** @brief Drops the runtime subscription. Called on Stop. Definitions are kept. */
        void Reset()
        {
            changeSub_.Unsubscribe();
            evaluating_ = false;
            evalDirty_  = false;
        }

        // --- In-game quest log overlay (engine-drawn, like DialogueManager::DrawGUI) ---

        /** @brief Whether the quest-log overlay is currently shown. */
        [[nodiscard]] bool LogOpen() const { return logOpen_; }
        void SetLogOpen(bool open) { logOpen_ = open; }

        /**
         * @brief Draws the quest-log overlay in the screen-space UI pass. Toggled by the
         * "QuestLog" input action (falling back to J) when acceptInput is true — mirrors how
         * PlayerInteractor falls back to E for Interact. Lists each active quest's objectives
         * with a checkbox derived from StoryState, so it works for both quest modes.
         */
        void DrawLogGUI(bool acceptInput)
        {
            if (acceptInput && (InputManager::Get().IsPressed("QuestLog") || IsKeyPressed(KEY_J)))
                logOpen_ = !logOpen_;
            if (!logOpen_) return;

            const float panelW = 320.0f, margin = 16.0f, pad = 14.0f, lineH = 22.0f;
            const int   titleSize = 18, objSize = 15;
            float       x = (float)Screen::Width() - panelW - margin;
            float       y = margin;

            ::Rectangle header = { x, y, panelW, 30.0f };
            GUI::Box(header, Color{ 12, 12, 16, 235 }, Color{ 120, 120, 140, 255 }, 2.0f);
            GUI::Label("Quests", x + pad, y + 6.0f, 20, Color{ 230, 210, 140, 255 });
            y += 30.0f + 6.0f;

            const std::vector<std::string> active = ActiveQuests();
            if (active.empty())
            {
                ::Rectangle b = { x, y, panelW, 34.0f };
                GUI::Box(b, Color{ 12, 12, 16, 210 }, Color{ 80, 80, 95, 255 }, 1.0f);
                GUI::Label("(No active quests)", x + pad, y + 9.0f, 16, Color{ 170, 170, 185, 255 });
                return;
            }

            for (const auto& id : active)
            {
                const QuestDef* d = Definition(id);
                if (!d) continue;
                const float boxH = pad + (float)titleSize + 6.0f + (float)d->objectives.size() * lineH + pad;
                ::Rectangle box = { x, y, panelW, boxH };
                GUI::Box(box, Color{ 12, 12, 16, 220 }, Color{ 90, 90, 110, 255 }, 1.0f);

                float ty = y + pad;
                GUI::Label(d->title.empty() ? id.c_str() : d->title.c_str(), x + pad, ty, titleSize, Color{ 230, 210, 140, 255 });
                ty += (float)titleSize + 6.0f;

                for (const auto& o : d->objectives)
                {
                    const bool        done = IsObjectiveDone(o);
                    const std::string line = (done ? "[x] " : "[ ] ") + (o.desc.empty() ? o.id : o.desc);
                    const Color       c    = done ? Color{ 130, 200, 130, 255 } : Color{ 210, 210, 220, 255 };
                    GUI::Label(line.c_str(), x + pad + 4.0f, ty, objSize, c);
                    ty += lineH;
                }
                y += boxH + 8.0f;
            }
        }

        // --- StoryState key layout (public so the editor panel can display them) ---

        static std::string StateKey(const std::string& id) { return "quest." + id + ".state"; }
        static std::string StepKey(const std::string& id)  { return "quest." + id + ".step"; }

    private:
        QuestManager()  = default;
        ~QuestManager() = default;

        void SetStateKey(const std::string& id, QuestState s) { StoryState::Get().Set(StateKey(id), (int)s); }

        /** @brief Completes a quest: records the state, publishes the completion flag and
         *  rewards as NarrativeEvents (so StoryState records them and other quests/dialogue
         *  can gate on completion), and fires QuestEvent::Completed. */
        void Finish(const QuestDef& d)
        {
            SetStateKey(d.id, QuestState::Complete);
            // Publish (don't SetFlag) so StoryState's NarrativeEvent subscription records the
            // flag exactly once — mirrors DialogueManager, which avoids a double StoryState event.
            if (!d.completeFlag.empty()) Events::Publish(GameEvents::NarrativeEvent{ d.completeFlag, nullptr });
            for (const auto& r : d.rewards)
                if (!r.empty()) Events::Publish(GameEvents::NarrativeEvent{ r, nullptr });
            Events::Publish(GameEvents::QuestEvent{ GameEvents::QuestEvent::Type::Completed, d.id, "" });
        }

        /**
         * @brief Re-evaluates every Active quest against current StoryState, advancing or
         * completing as objectives are satisfied.
         *
         * Re-entrancy: setting quest.* keys and publishing completion flags fires
         * StoryStateChangedEvent again, which would re-enter here. We coalesce with the
         * evaluating_/evalDirty_ pair (a re-entrant call just marks the pass dirty and the
         * outermost call loops until stable) — the same shape as StoryState::NotifyChange.
         */
        void Evaluate()
        {
            if (evaluating_) { evalDirty_ = true; return; }
            evaluating_ = true;
            do
            {
                evalDirty_ = false;
                for (auto& [id, d] : defs_)
                {
                    if (StateOf(id) != QuestState::Active) continue;
                    if (d.objectives.empty()) { Finish(d); continue; }

                    if (d.mode == QuestMode::Sequential)
                    {
                        int step = StoryState::Get().GetInt(StepKey(id), 0);
                        // Walk past every objective already satisfied (handles out-of-order flags).
                        while (step < (int)d.objectives.size() && IsObjectiveDone(d.objectives[step]))
                        {
                            Events::Publish(GameEvents::QuestEvent{
                                GameEvents::QuestEvent::Type::ObjectiveCompleted, id, d.objectives[step].id });
                            ++step;
                            StoryState::Get().Set(StepKey(id), step);
                        }
                        if (step >= (int)d.objectives.size()) Finish(d);
                    }
                    else // Parallel — complete when every objective is done
                    {
                        bool all = true;
                        for (const auto& o : d.objectives)
                            if (!IsObjectiveDone(o)) { all = false; break; }
                        if (all) Finish(d);
                    }
                }
            } while (evalDirty_);
            evaluating_ = false;
        }

        bool LoadFile(const std::string& path)
        {
            std::ifstream f(path);
            if (!f.is_open()) { TraceLog(LOG_WARNING, "QUEST: cannot open %s", path.c_str()); return false; }
            nlohmann::json j;
            try { f >> j; } catch (...) { TraceLog(LOG_WARNING, "QUEST: invalid JSON in %s", path.c_str()); return false; }
            return LoadFromJson(j);
        }

        static QuestDef ParseDef(const nlohmann::json& j)
        {
            QuestDef d;
            d.id           = j.value("id", std::string{});
            d.title        = j.value("title", std::string{});
            d.description  = j.value("description", std::string{});
            d.autoStart    = j.value("autoStart", false);
            d.completeFlag = j.value("completeFlag", std::string{});
            const std::string mode = j.value("mode", std::string{ "sequential" });
            d.mode = (mode == "parallel") ? QuestMode::Parallel : QuestMode::Sequential;

            if (j.contains("objectives") && j["objectives"].is_array())
                for (const auto& oj : j["objectives"])
                {
                    QuestObjective o;
                    o.id           = oj.value("id", std::string{});
                    o.desc         = oj.value("desc", std::string{});
                    o.completeFlag = oj.value("completeFlag", std::string{});
                    d.objectives.push_back(std::move(o));
                }

            if (j.contains("rewards") && j["rewards"].is_array())
                for (const auto& rj : j["rewards"])
                    if (rj.is_string()) d.rewards.push_back(rj.get<std::string>());

            return d;
        }

        std::string                     projectPath_;
        std::map<std::string, QuestDef> defs_;
        SubscriptionHandle              changeSub_;
        bool                            evaluating_ = false;
        bool                            evalDirty_  = false;
        bool                            logOpen_    = false;
    };
}
