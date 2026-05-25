#pragma once
#include "raylib.h"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>
#include "EventBus.hpp"
#include "events/GameEvents.hpp"
#include "StoryState.hpp"
#include "Screen.hpp"
#include "GUI.hpp"
#include "../include/nlohmann/json.hpp"

namespace Indium
{
    /** @brief One selectable option on a dialogue node. */
    struct DialogueChoice
    {
        std::string text;         // shown to the player
        std::string next;         // node id to jump to ("" ends the dialogue)
        std::string setFlag;      // optional StoryState flag set true when chosen
        std::string requireFlag;  // optional: choice only shown while this flag is true
    };

    /** @brief A single line of dialogue: a speaker, body text, and optional branches. */
    struct DialogueNode
    {
        std::string id;
        std::string speaker;
        std::string text;
        std::string setFlag;      // optional flag set true when this node is entered
        std::string next;         // narration advance target when there are no choices
        std::vector<DialogueChoice> choices;
    };

    struct DialogueGraph
    {
        std::string                         start;   // first node id ("" = first in map)
        std::map<std::string, DialogueNode> nodes;
    };

    /**
     * @brief Global runtime dialogue runner. Header-only singleton shared across the
     * script dylib boundary (like StoryState / InputManager).
     *
     * Data lives in <project>/dialogue/<name>.json:
     *
     *   {
     *     "start": "greet",
     *     "nodes": {
     *       "greet": {
     *         "speaker": "Alice",
     *         "text": "Have we met before?",
     *         "choices": [
     *           { "text": "In the village.", "next": "remember", "requireFlag": "met_in_village" },
     *           { "text": "I don't think so.", "next": "intro", "setFlag": "denied_meeting" },
     *           { "text": "[Leave]", "next": "" }
     *         ]
     *       },
     *       "intro": { "speaker": "Alice", "text": "Well met. I'm Alice.", "next": "" }
     *     }
     *   }
     *
     * Branching reads StoryState (requireFlag gates a choice's visibility); choosing
     * writes StoryState (setFlag) and fires a NarrativeEvent so the beat is recorded.
     * The engine draws the active box in the screen-space UI pass (DrawGUI), so a
     * dialogue can be driven entirely from editor data — but scripts may also call
     * Start()/Advance()/Choose()/IsActive()/Current().
     */
    class DialogueManager
    {
    public:
        DialogueManager(const DialogueManager&)            = delete;
        DialogueManager& operator=(const DialogueManager&) = delete;
        DialogueManager(DialogueManager&&)                 = delete;
        DialogueManager& operator=(DialogueManager&&)      = delete;

        static DialogueManager& Get() { static DialogueManager inst; return inst; }

        /** @brief Where dialogue files are loaded from. Set by the editor on project open. */
        void SetProjectPath(const std::string& path) { projectPath_ = path; }

        /** @brief Loads <project>/dialogue/<name>.json and begins at its start node.
         *  Returns false (and logs) if the file is missing or malformed. */
        bool Start(const std::string& name)
        {
            if (projectPath_.empty()) { TraceLog(LOG_WARNING, "DIALOGUE: no project path set"); return false; }

            std::string path = (std::filesystem::path(projectPath_) / "dialogue" / (name + ".json")).string();
            std::ifstream f(path);
            if (!f.is_open()) { TraceLog(LOG_WARNING, "DIALOGUE: cannot open %s", path.c_str()); return false; }

            nlohmann::json j;
            try { f >> j; } catch (...) { TraceLog(LOG_WARNING, "DIALOGUE: invalid JSON in %s", path.c_str()); return false; }

            graph_ = ParseGraph(j);
            if (graph_.nodes.empty()) { TraceLog(LOG_WARNING, "DIALOGUE: '%s' has no nodes", name.c_str()); return false; }

            const std::string startId = !graph_.start.empty() ? graph_.start : graph_.nodes.begin()->first;
            active_ = true;
            GoTo(startId);
            return active_;
        }

        /** @brief Advances a narration node (no visible choices) to its `next`, or ends.
         *  No-op on a choice node — call Choose() instead. */
        void Advance()
        {
            const DialogueNode* n = Current();
            if (!n) return;
            if (!VisibleChoices().empty()) return;
            GoTo(n->next);
        }

        /** @brief Selects the i-th *visible* choice: applies its flag/event, then jumps. */
        void Choose(int visibleIndex)
        {
            auto vis = VisibleChoices();
            if (visibleIndex < 0 || visibleIndex >= (int)vis.size()) return;
            // Copy out of the node before publishing: a NarrativeEvent/StoryStateChanged
            // handler may call Start(), which rebuilds graph_ and would dangle the choice.
            const std::string setFlag = vis[visibleIndex]->setFlag;
            const std::string next    = vis[visibleIndex]->next;
            // The NarrativeEvent records the flag via StoryState's subscription, so don't
            // also SetFlag here — that would fire StoryStateChangedEvent twice for one beat.
            if (!setFlag.empty()) Events::Publish(GameEvents::NarrativeEvent{ setFlag, nullptr });
            GoTo(next);
        }

        [[nodiscard]] bool IsActive() const { return active_; }

        /** @brief The node currently on screen, or nullptr when no dialogue is running. */
        [[nodiscard]] const DialogueNode* Current() const
        {
            if (!active_) return nullptr;
            auto it = graph_.nodes.find(currentId_);
            return it == graph_.nodes.end() ? nullptr : &it->second;
        }

        /** @brief Choices on the current node whose requireFlag (if any) is satisfied. */
        [[nodiscard]] std::vector<const DialogueChoice*> VisibleChoices() const
        {
            std::vector<const DialogueChoice*> out;
            const DialogueNode* n = Current();
            if (!n) return out;
            for (const auto& c : n->choices)
                if (c.requireFlag.empty() || StoryState::Get().HasFlag(c.requireFlag))
                    out.push_back(&c);
            return out;
        }

        /** @brief Stops any running dialogue. Called by the editor on Play start and Stop. */
        void End() { active_ = false; currentId_.clear(); }

        /**
         * @brief Engine-side render + input, called by the editor in the screen-space pass.
         * Draws nothing when inactive; processes advance/choose input only when acceptInput.
         */
        void DrawGUI(bool acceptInput)
        {
            const DialogueNode* n = Current();
            if (!n) return;

            const float W      = (float)Screen::Width();
            const float H      = (float)Screen::Height();
            const float margin = 40.0f;
            const float boxH   = 200.0f;
            ::Rectangle box = { margin, H - boxH - margin, W - margin * 2.0f, boxH };

            GUI::Box(box, Color{ 12, 12, 16, 235 }, Color{ 120, 120, 140, 255 }, 2.0f);

            const float pad = 18.0f;
            float x = box.x + pad;
            float y = box.y + pad;

            if (!n->speaker.empty())
            {
                GUI::Label(n->speaker.c_str(), x, y, 22, Color{ 120, 200, 255, 255 });
                y += 30.0f;
            }

            ::Rectangle textArea = { x, y, box.width - pad * 2.0f, 90.0f };
            y += GUI::LabelWrapped(n->text.c_str(), textArea, 20, RAYWHITE) + 14.0f;

            auto vis = VisibleChoices();
            if (!vis.empty())
            {
                for (int i = 0; i < (int)vis.size(); ++i)
                {
                    ::Rectangle row = { x, y, box.width - pad * 2.0f, 30.0f };
                    const std::string label = std::to_string(i + 1) + ".  " + vis[i]->text;
                    const bool clicked = GUI::Button(row, label.c_str(), 18);
                    const bool keyed   = acceptInput && i < 9 && IsKeyPressed(KEY_ONE + i);
                    if (acceptInput && (clicked || keyed)) { Choose(i); return; }
                    y += 34.0f;
                }
            }
            else
            {
                GUI::Label("[Space] Continue", box.x + box.width - 170.0f, box.y + box.height - 30.0f,
                           16, Color{ 180, 180, 195, 255 });
                if (acceptInput && (Screen::MousePressed() || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)))
                {
                    Advance();
                    return;
                }
            }
        }

    private:
        DialogueManager()  = default;
        ~DialogueManager() = default;

        void GoTo(const std::string& id)
        {
            if (id.empty()) { End(); return; }
            auto it = graph_.nodes.find(id);
            if (it == graph_.nodes.end()) { TraceLog(LOG_WARNING, "DIALOGUE: missing node '%s'", id.c_str()); End(); return; }

            currentId_ = id;
            // Copy before publishing: a handler may rebuild graph_ and dangle `it`.
            // Publishing alone sets the flag via StoryState's subscription (no double event).
            const std::string setFlag = it->second.setFlag;
            if (!setFlag.empty()) Events::Publish(GameEvents::NarrativeEvent{ setFlag, nullptr });
        }

        static DialogueGraph ParseGraph(const nlohmann::json& j)
        {
            DialogueGraph g;
            g.start = j.value("start", std::string{});
            if (j.contains("nodes") && j["nodes"].is_object())
            {
                for (auto it = j["nodes"].begin(); it != j["nodes"].end(); ++it)
                {
                    const auto&  nj = it.value();
                    DialogueNode n;
                    n.id      = it.key();
                    n.speaker = nj.value("speaker", std::string{});
                    n.text    = nj.value("text", std::string{});
                    n.setFlag = nj.value("setFlag", std::string{});
                    n.next    = nj.value("next", std::string{});
                    if (nj.contains("choices") && nj["choices"].is_array())
                    {
                        for (const auto& cj : nj["choices"])
                        {
                            DialogueChoice c;
                            c.text        = cj.value("text", std::string{});
                            c.next        = cj.value("next", std::string{});
                            c.setFlag     = cj.value("setFlag", std::string{});
                            c.requireFlag = cj.value("requireFlag", std::string{});
                            n.choices.push_back(std::move(c));
                        }
                    }
                    g.nodes[n.id] = std::move(n);
                }
            }
            return g;
        }

        std::string   projectPath_;
        DialogueGraph graph_;
        std::string   currentId_;
        bool          active_ = false;
    };
}
