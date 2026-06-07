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
#include "AssetManager.hpp"
#include "ItemManager.hpp"
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
        std::string giveItem;     // optional item id granted when chosen (ItemManager)
        int         giveCount = 1; // how many of giveItem
        std::string takeItem;     // optional item id removed when chosen
        int         takeCount = 1; // how many of takeItem
    };

    /** @brief A single line of dialogue: a speaker, body text, and optional branches. */
    struct DialogueNode
    {
        std::string id;
        std::string speaker;
        std::string text;
        std::string portrait;     // optional speaker image (project-relative path, or absolute)
        std::string setFlag;      // optional flag set true when this node is entered
        std::string next;         // narration advance target when there are no choices
        std::string giveItem;     // optional item id granted when this node is entered (ItemManager)
        int         giveCount = 1; // how many of giveItem
        std::string takeItem;     // optional item id removed when this node is entered
        int         takeCount = 1; // how many of takeItem
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

        /** @brief Resolve a node portrait path (project-relative or absolute) to a filesystem
         *  path string for AssetManager::GetTexture. Empty portrait -> empty (no texture). Shared
         *  by the runtime dialogue box and the editor's DialoguePanel preview so they stay in step. */
        static std::string ResolvePortraitPath(const std::string& portrait, const std::string& projectPath)
        {
            if (portrait.empty()) return {};
            std::filesystem::path pp(portrait);
            if (pp.is_relative() && !projectPath.empty()) pp = std::filesystem::path(projectPath) / pp;
            return pp.string();
        }

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
            const std::string setFlag   = vis[visibleIndex]->setFlag;
            const std::string next      = vis[visibleIndex]->next;
            const std::string giveItem  = vis[visibleIndex]->giveItem;
            const int         giveCount = vis[visibleIndex]->giveCount;
            const std::string takeItem  = vis[visibleIndex]->takeItem;
            const int         takeCount = vis[visibleIndex]->takeCount;
            const std::string fromId    = currentId_; // freeze so we can detect a hijack
            // Apply item grant/cost first (a visible "buy" choice is gated by requireFlag, so
            // the player can afford it). Give/Take fire StoryStateChangedEvent, which — like the
            // NarrativeEvent below — a handler might use to hijack the dialogue; the fromId check
            // after catches that. The NarrativeEvent records the flag via StoryState's
            // subscription, so don't also SetFlag here (that would double-fire for one beat).
            if (!giveItem.empty()) ItemManager::Get().Give(giveItem, giveCount);
            if (!takeItem.empty()) ItemManager::Get().Take(takeItem, takeCount);
            if (!setFlag.empty()) Events::Publish(GameEvents::NarrativeEvent{ setFlag, nullptr });
            // If a handler started a different dialogue (or ended this one), our `next`
            // is meaningless in the new graph — bail rather than jump to a stale node id.
            if (!active_ || currentId_ != fromId) return;
            selectedChoice_ = 0;
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
            // requireFlag is a full condition expression — a bare flag, !flag, comparisons
            // (coins >= 10), and &&/|| combinations; empty means always shown. See StoryEval.
            for (const auto& c : n->choices)
                if (StoryEval(c.requireFlag))
                    out.push_back(&c);
            return out;
        }

        /** @brief Stops any running dialogue. Called by the editor on Play start and Stop.
         *  Also drops the loaded graph so a stale dialogue can't bleed into the next
         *  project/scene through this long-lived singleton. */
        void End()
        {
            active_         = false;
            currentId_.clear();
            graph_          = {};
            selectedChoice_ = 0;
        }

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

            // Optional speaker portrait, inset on the left; the text column indents past it.
            // Path is project-relative (resolved against projectPath_) or absolute — a missing
            // texture just leaves no portrait and no indent, so this stays graceful.
            if (!n->portrait.empty() && !projectPath_.empty())
            {
                Texture2D tex = AssetManager::Get().GetTexture(ResolvePortraitPath(n->portrait, projectPath_));
                if (tex.id != 0)
                {
                    const float ps = box.height - pad * 2.0f;                       // square slot
                    const float tw = (float)tex.width, th = (float)tex.height;
                    const float sc = (ps / tw < ps / th) ? ps / tw : ps / th;       // fit, keep aspect
                    ::Rectangle pr = { box.x + pad + (ps - tw * sc) * 0.5f,
                                       box.y + pad + (ps - th * sc) * 0.5f,
                                       tw * sc, th * sc };
                    GUI::Image(tex, pr);
                    GUI::Box(pr, BLANK, Color{ 120, 120, 140, 255 }, 1.0f);
                    x = box.x + pad + ps + pad;                                      // indent text past the slot
                }
            }
            // Right edge of the text column, so widths shrink correctly when indented.
            const float contentW = box.x + box.width - pad - x;

            if (!n->speaker.empty())
            {
                const std::string speaker = StoryInterpolate(n->speaker);
                GUI::Label(speaker.c_str(), x, y, 22, Color{ 120, 200, 255, 255 });
                y += 30.0f;
            }

            // Resolve {var} tokens against StoryState, then reveal the line with a typewriter
            // effect. Layout + returned height are for the FULL text, so the choices below sit
            // at a fixed position and don't jump as glyphs appear.
            const std::string body = StoryInterpolate(n->text);
            // Advance the typewriter only while the game is live (acceptInput) — when paused, or
            // while an editor field owns the keyboard, the reveal holds instead of running on.
            if (acceptInput) revealTime_ += GetFrameTime();
            const int shown = (int)(revealTime_ * kRevealCharsPerSec);

            // Reveal against the glyphs LabelWrapped actually draws (revealable), NOT body.size():
            // wrap spaces and '\n' aren't drawn, so comparing to the raw length would leave choices
            // hidden for a beat after the text had finished appearing.
            ::Rectangle textArea = { x, y, contentW, 90.0f };
            int revealable = 0;
            y += GUI::LabelWrapped(body.c_str(), textArea, 20, RAYWHITE, 4, shown, &revealable) + 14.0f;
            const bool revealing = shown < revealable;

            // Still typing: a press/click completes the line instantly instead of advancing;
            // choices and the continue prompt appear only once it is fully shown.
            if (revealing)
            {
                if (acceptInput && (Screen::MousePressed() || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)))
                    revealTime_ = (float)revealable / kRevealCharsPerSec + 1.0f;
                return;
            }

            auto vis = VisibleChoices();
            if (!vis.empty())
            {
                // Keep the selected index in range — the visible set can change
                // mid-dialogue if a flag flips a `requireFlag` gate.
                if (selectedChoice_ < 0 || selectedChoice_ >= (int)vis.size()) selectedChoice_ = 0;

                // Keyboard navigation: up/down (and W/S) to move, Enter/Space to confirm.
                if (acceptInput)
                {
                    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selectedChoice_ = (selectedChoice_ + 1) % (int)vis.size();
                    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) selectedChoice_ = (selectedChoice_ - 1 + (int)vis.size()) % (int)vis.size();
                }

                for (int i = 0; i < (int)vis.size(); ++i)
                {
                    ::Rectangle row = { x, y, contentW, 30.0f };
                    // Number prefix only for the first 9; the rest just show the text.
                    const std::string prefix = (i < 9) ? (std::to_string(i + 1) + ".  ") : std::string("   ");
                    const std::string label  = prefix + StoryInterpolate(vis[i]->text);

                    bool clicked = false;
                    if (i == selectedChoice_)
                    {
                        // Render the selected choice ourselves so the keyboard highlight
                        // isn't overwritten by GUI::Button's default background.
                        Vector2 m       = Screen::MousePosition();
                        bool    hovered = CheckCollisionPointRec(m, row);
                        Color   bg      = hovered ? (Screen::MouseDown() ? Color{ 30, 30, 36, 240 } : Color{ 95, 95, 110, 240 })
                                                  : Color{ 70, 70, 85, 230 };
                        GUI::Box(row, bg, Color{ 200, 200, 220, 255 }, 2.0f);
                        GUI::LabelCentered(label.c_str(), row, 18, RAYWHITE);
                        clicked = hovered && Screen::MousePressed();
                    }
                    else
                    {
                        clicked = GUI::Button(row, label.c_str(), 18);
                    }
                    const bool numKeyed   = acceptInput && i < 9 && IsKeyPressed(KEY_ONE + i);
                    const bool enterKeyed = acceptInput && i == selectedChoice_ && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE));
                    if (acceptInput && (clicked || numKeyed || enterKeyed)) { Choose(i); return; }
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

        // --- Document (de)serialization ------------------------------------------
        // Shared by the editor's Dialogue panel and the runtime loader so both speak
        // exactly one on-disk format. The editor holds dialogues as an ordered
        // std::vector<DialogueNode>; the runtime indexes them into DialogueGraph::nodes
        // (see ParseGraph, which delegates here).

        /** @brief Serializes an ordered node list + start id to the dialogue JSON shape:
         *  {start, nodes:{id:{speaker,text,setFlag?,next?,choices?}}}. Empty-id nodes are
         *  skipped and empty optional fields omitted, so authored files stay clean. */
        static nlohmann::json ToJson(const std::string& start, const std::vector<DialogueNode>& nodes)
        {
            nlohmann::json out;
            out["start"] = start;
            nlohmann::json njs = nlohmann::json::object();
            for (const auto& n : nodes)
            {
                if (n.id.empty()) continue;
                nlohmann::json o;
                o["speaker"] = n.speaker;
                o["text"]    = n.text;
                if (!n.portrait.empty()) o["portrait"] = n.portrait;
                if (!n.setFlag.empty())  o["setFlag"]  = n.setFlag;
                if (!n.giveItem.empty()) { o["giveItem"] = n.giveItem; o["giveCount"] = n.giveCount; }
                if (!n.takeItem.empty()) { o["takeItem"] = n.takeItem; o["takeCount"] = n.takeCount; }
                if (!n.next.empty())    o["next"]    = n.next;
                if (!n.choices.empty())
                {
                    nlohmann::json cs = nlohmann::json::array();
                    for (const auto& c : n.choices)
                    {
                        nlohmann::json cj;
                        cj["text"] = c.text;
                        cj["next"] = c.next;
                        if (!c.setFlag.empty())     cj["setFlag"]     = c.setFlag;
                        if (!c.requireFlag.empty()) cj["requireFlag"] = c.requireFlag;
                        if (!c.giveItem.empty()) { cj["giveItem"] = c.giveItem; cj["giveCount"] = c.giveCount; }
                        if (!c.takeItem.empty()) { cj["takeItem"] = c.takeItem; cj["takeCount"] = c.takeCount; }
                        cs.push_back(std::move(cj));
                    }
                    o["choices"] = std::move(cs);
                }
                njs[n.id] = std::move(o);
            }
            out["nodes"] = std::move(njs);
            return out;
        }

        /** @brief Parses the dialogue JSON shape into an ordered node list + start id.
         *  Inverse of ToJson; node order follows JSON key order. */
        static void FromJson(const nlohmann::json& j, std::string& start, std::vector<DialogueNode>& nodes)
        {
            start = j.value("start", std::string{});
            nodes.clear();
            if (j.contains("nodes") && j["nodes"].is_object())
                for (auto it = j["nodes"].begin(); it != j["nodes"].end(); ++it)
                {
                    const auto&  nj = it.value();
                    DialogueNode n;
                    n.id      = it.key();
                    n.speaker  = nj.value("speaker", std::string{});
                    n.text     = nj.value("text", std::string{});
                    n.portrait = nj.value("portrait", std::string{});
                    n.setFlag  = nj.value("setFlag", std::string{});
                    n.next    = nj.value("next", std::string{});
                    n.giveItem  = nj.value("giveItem", std::string{});
                    n.giveCount = nj.value("giveCount", 1);
                    n.takeItem  = nj.value("takeItem", std::string{});
                    n.takeCount = nj.value("takeCount", 1);
                    if (nj.contains("choices") && nj["choices"].is_array())
                        for (const auto& cj : nj["choices"])
                        {
                            DialogueChoice c;
                            c.text        = cj.value("text", std::string{});
                            c.next        = cj.value("next", std::string{});
                            c.setFlag     = cj.value("setFlag", std::string{});
                            c.requireFlag = cj.value("requireFlag", std::string{});
                            c.giveItem    = cj.value("giveItem", std::string{});
                            c.giveCount   = cj.value("giveCount", 1);
                            c.takeItem    = cj.value("takeItem", std::string{});
                            c.takeCount   = cj.value("takeCount", 1);
                            n.choices.push_back(std::move(c));
                        }
                    nodes.push_back(std::move(n));
                }
        }

        /** @brief Returns a start id guaranteed to reference an existing node: keeps the
         *  given start when it matches a node, else falls back to the first node, else ""
         *  (no nodes). The editor calls this after a node is deleted so the start can't be
         *  left dangling (which would make DialogueManager::Start end the dialogue at once). */
        static std::string NormalizeStart(const std::string& start, const std::vector<DialogueNode>& nodes)
        {
            if (nodes.empty()) return {};
            for (const auto& n : nodes) if (n.id == start) return start;
            return nodes.front().id;
        }

    private:
        DialogueManager()  = default;
        ~DialogueManager() = default;

        void GoTo(const std::string& id)
        {
            if (id.empty()) { End(); return; }
            auto it = graph_.nodes.find(id);
            if (it == graph_.nodes.end()) { TraceLog(LOG_WARNING, "DIALOGUE: missing node '%s'", id.c_str()); End(); return; }

            currentId_      = id;
            selectedChoice_ = 0;   // fresh node — default to first choice
            revealTime_     = 0.0f; // restart the typewriter for the new line
            // Copy before publishing: a handler (or ItemManager Give/Take, which fires
            // StoryStateChangedEvent) may rebuild graph_ and dangle `it`.
            // Publishing alone sets the flag via StoryState's subscription (no double event).
            const std::string setFlag   = it->second.setFlag;
            const std::string giveItem  = it->second.giveItem;
            const int         giveCount = it->second.giveCount;
            const std::string takeItem  = it->second.takeItem;
            const int         takeCount = it->second.takeCount;
            if (!giveItem.empty()) ItemManager::Get().Give(giveItem, giveCount);
            if (!takeItem.empty()) ItemManager::Get().Take(takeItem, takeCount);
            if (!setFlag.empty()) Events::Publish(GameEvents::NarrativeEvent{ setFlag, nullptr });
        }

        static DialogueGraph ParseGraph(const nlohmann::json& j)
        {
            DialogueGraph g;
            std::vector<DialogueNode> nodes;
            FromJson(j, g.start, nodes);            // one shared parse path (also used by the editor + tests)
            for (auto& n : nodes) g.nodes[n.id] = std::move(n);
            return g;
        }

        std::string   projectPath_;
        DialogueGraph graph_;
        std::string   currentId_;
        bool          active_         = false;
        int           selectedChoice_ = 0;    // index into VisibleChoices for keyboard nav
        float         revealTime_     = 0.0f;  // seconds the current line has been on screen (typewriter)

        // Typewriter speed in glyphs/second — brisk without feeling instant.
        static constexpr float kRevealCharsPerSec = 45.0f;
    };
}
