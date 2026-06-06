#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "AnimatorComponent.hpp"
#include "../../include/nlohmann/json.hpp"
#include "raylib.h"
#include "raymath.h"   // FloatEquals — tolerant float compare for Equals/NotEquals
#include "imgui.h"
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

namespace Indium
{
    // --------------------------------------------------------------------
    // AnimatorStateMachineComponent
    //
    // A data-driven state machine that sits on top of AnimatorComponent. Each
    // STATE names a clip to play; PARAMETERS form a small blackboard scripts
    // write to; TRANSITIONS move between states when their CONDITIONS pass.
    // The machine only decides *which* clip should be playing and calls
    // AnimatorComponent::Play() — the Animator still does the frame stepping and
    // the SpriteRenderer still reads the source rect. This keeps idle<->walk<->jump
    // automatic instead of toggled by hand from scripts.
    //
    // Script usage:
    //   auto* sm = entity->GetComponent<AnimatorStateMachineComponent>();
    //   sm->SetFloat("speed", Vector2Length(entity->velocity));
    //   sm->SetBool("grounded", grounded);
    //   sm->SetTrigger("jump");
    //   const std::string& s = sm->CurrentState();
    // --------------------------------------------------------------------
    struct AnimatorStateMachineComponent : Component
    {
        enum class ParamType { Float, Bool, Trigger };
        enum class Op        { Greater, Less, Equals, NotEquals, IsTrue, IsFalse, Triggered };

        struct Param
        {
            std::string name;
            ParamType   type   = ParamType::Float;
            float       number = 0.0f;   // Float value
            bool        flag   = false;  // Bool / Trigger value
        };

        struct State
        {
            std::string name;
            std::string clip;   // AnimatorComponent clip to Play() on entry
        };

        struct Condition
        {
            std::string param;
            Op          op        = Op::Greater;
            float       threshold = 0.0f;
        };

        struct Transition
        {
            std::string             from;            // "" / "Any State" = from any state
            std::string             to;
            std::vector<Condition>  conditions;      // ANDed together
            bool                    hasExitTime = false; // wait for a non-looping clip to finish
            float                   minTime     = 0.0f;  // min seconds in 'from' before allowed
        };

        std::vector<Param>      params;
        std::vector<State>      states;
        std::vector<Transition> transitions;
        std::string             defaultState;

        // --- Runtime (not serialized) ---
        std::string currentState_;
        float       timeInState_ = 0.0f;

        // --- Script-facing controls --------------------------------------------
        void  SetFloat(const std::string& n, float v) { if (Param* p = findParam_(n)) if (p->type == ParamType::Float)   p->number = v; }
        void  SetBool (const std::string& n, bool v)  { if (Param* p = findParam_(n)) if (p->type == ParamType::Bool)    p->flag   = v; }
        void  SetTrigger(const std::string& n)        { if (Param* p = findParam_(n)) if (p->type == ParamType::Trigger) p->flag   = true; }
        void  ResetTrigger(const std::string& n)      { if (Param* p = findParam_(n)) if (p->type == ParamType::Trigger) p->flag   = false; }
        [[nodiscard]] float GetFloat(const std::string& n) { Param* p = findParam_(n); return p ? p->number : 0.0f; }
        [[nodiscard]] bool  GetBool (const std::string& n) { Param* p = findParam_(n); return p ? p->flag   : false; }

        /** @brief Force the machine into a state immediately (also plays its clip). */
        void Play(const std::string& stateName) { enterState_(stateName); }
        [[nodiscard]] const std::string& CurrentState() const { return currentState_; }

        // --- Engine hooks ------------------------------------------------------
        void start(Scene*) override
        {
            for (auto& p : params) if (p.type == ParamType::Trigger) p.flag = false;
            currentState_.clear();
            timeInState_ = 0.0f;
            enterState_(resolveDefault_());
        }

        void update(float dt, Vector2, Scene*) override
        {
            if (states.empty()) return;
            if (currentState_.empty() || !findState_(currentState_))
                enterState_(resolveDefault_()); // re-resolve if current state was deleted at runtime

            timeInState_ += dt;

            for (const auto& t : transitions)
            {
                const bool fromAny = t.from.empty() || t.from == "Any State";
                if (!fromAny && t.from != currentState_) continue;
                if (fromAny  && t.to == currentState_)    continue; // don't re-enter self every frame
                if (!findState_(t.to)) continue;                    // dangling target: skip without consuming triggers
                if (t.minTime > 0.0f && timeInState_ < t.minTime) continue;
                if (t.hasExitTime && !clipFinished_()) continue;
                if (!conditionsPass_(t)) continue;

                consumeTriggers_(t);
                enterState_(t.to);
                return; // at most one transition per frame
            }
        }

        std::string getName() const override { return "AnimatorStateMachine"; }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<AnimatorStateMachineComponent>(*this);
            c->currentState_.clear();   // runtime state never carries across a clone
            c->timeInState_ = 0.0f;
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["defaultState"] = defaultState;

            nlohmann::json pj = nlohmann::json::array();
            for (const auto& p : params)
                pj.push_back({ {"name", p.name}, {"type", (int)p.type}, {"number", p.number}, {"flag", p.flag} });
            j["params"] = pj;

            nlohmann::json sj = nlohmann::json::array();
            for (const auto& s : states)
                sj.push_back({ {"name", s.name}, {"clip", s.clip} });
            j["states"] = sj;

            nlohmann::json tj = nlohmann::json::array();
            for (const auto& t : transitions)
            {
                nlohmann::json cj = nlohmann::json::array();
                for (const auto& c : t.conditions)
                    cj.push_back({ {"param", c.param}, {"op", (int)c.op}, {"threshold", c.threshold} });
                tj.push_back({ {"from", t.from}, {"to", t.to}, {"hasExitTime", t.hasExitTime},
                               {"minTime", t.minTime}, {"conditions", cj} });
            }
            j["transitions"] = tj;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("defaultState")) defaultState = j["defaultState"].get<std::string>();

            params.clear();
            if (j.contains("params"))
                for (const auto& pj : j["params"])
                {
                    Param p;
                    if (pj.contains("name"))   p.name   = pj["name"].get<std::string>();
                    if (pj.contains("type"))   p.type   = (ParamType)pj["type"].get<int>();
                    if (pj.contains("number")) p.number = pj["number"].get<float>();
                    if (pj.contains("flag"))   p.flag   = pj["flag"].get<bool>();
                    params.push_back(std::move(p));
                }

            states.clear();
            if (j.contains("states"))
                for (const auto& sj : j["states"])
                {
                    State s;
                    if (sj.contains("name")) s.name = sj["name"].get<std::string>();
                    if (sj.contains("clip")) s.clip = sj["clip"].get<std::string>();
                    states.push_back(std::move(s));
                }

            transitions.clear();
            if (j.contains("transitions"))
                for (const auto& tj : j["transitions"])
                {
                    Transition t;
                    if (tj.contains("from"))        t.from        = tj["from"].get<std::string>();
                    if (tj.contains("to"))          t.to          = tj["to"].get<std::string>();
                    if (tj.contains("hasExitTime")) t.hasExitTime = tj["hasExitTime"].get<bool>();
                    if (tj.contains("minTime"))     t.minTime     = tj["minTime"].get<float>();
                    if (tj.contains("conditions"))
                        for (const auto& cj : tj["conditions"])
                        {
                            Condition c;
                            if (cj.contains("param"))     c.param     = cj["param"].get<std::string>();
                            if (cj.contains("op"))        c.op        = (Op)cj["op"].get<int>();
                            if (cj.contains("threshold")) c.threshold = cj["threshold"].get<float>();
                            t.conditions.push_back(std::move(c));
                        }
                    transitions.push_back(std::move(t));
                }
        }

        void inspect(std::function<void()> snapshotCb) override;

    private:
        Param* findParam_(const std::string& n) { for (auto& p : params) if (p.name == n) return &p; return nullptr; }
        const State* findState_(const std::string& n) const { for (auto& s : states) if (s.name == n) return &s; return nullptr; }

        void enterState_(const std::string& name)
        {
            const State* s = findState_(name);
            if (!s) return; // ignore unknown/dangling names (deleted or renamed state)
            currentState_ = name;
            timeInState_  = 0.0f;
            if (owner)
                if (auto* anim = owner->getComponent<AnimatorComponent>())
                    anim->Play(s->clip);
        }

        // Entry state: the authored default if it still exists, else the first
        // state — so deleting the state marked Default can't strand the machine.
        std::string resolveDefault_() const
        {
            if (!defaultState.empty() && findState_(defaultState)) return defaultState;
            return states.empty() ? std::string{} : states.front().name;
        }

        bool clipFinished_() const
        {
            if (!owner) return true;
            auto* anim = owner->getComponent<AnimatorComponent>();
            if (!anim) return true;
            return !anim->playing; // non-looping clips set playing=false when they end
        }

        bool conditionsPass_(const Transition& t) const
        {
            for (const auto& c : t.conditions)
            {
                const Param* p = nullptr;
                for (const auto& q : params) if (q.name == c.param) { p = &q; break; }
                if (!p) return false;

                // Normalise the param to BOTH a numeric and a boolean view so a
                // condition behaves sensibly however its op pairs with the param's
                // type: a Bool/Trigger reads as 1/0 under the numeric ops, and a
                // Float reads as "non-zero" under the boolean ops. Without this a
                // numeric op on a Bool would silently compare the unused (always-
                // default) `number` field, leaving the condition permanently dead.
                const float num  = (p->type == ParamType::Float) ? p->number
                                                                  : (p->flag ? 1.0f : 0.0f);
                const bool  flag = (p->type == ParamType::Float) ? (p->number != 0.0f)
                                                                  : p->flag;
                switch (c.op)
                {
                    // Equals/NotEquals use a tolerant compare: exact == on a
                    // continuously-varying float almost never matches.
                    case Op::Greater:   if (!(num >  c.threshold))          return false; break;
                    case Op::Less:      if (!(num <  c.threshold))          return false; break;
                    case Op::Equals:    if (!FloatEquals(num, c.threshold)) return false; break;
                    case Op::NotEquals: if ( FloatEquals(num, c.threshold)) return false; break;
                    case Op::IsTrue:
                    case Op::Triggered: if (!flag)                          return false; break;
                    case Op::IsFalse:   if ( flag)                          return false; break;
                }
            }
            return true; // empty condition list == unconditional
        }

        void consumeTriggers_(const Transition& t)
        {
            for (const auto& c : t.conditions)
                if (Param* p = findParam_(c.param))
                    if (p->type == ParamType::Trigger) p->flag = false;
        }
    };

    // -----------------------------------------------------------------------
    // Inspector (kept out-of-class so the data model above stays readable).
    // -----------------------------------------------------------------------
    inline void AnimatorStateMachineComponent::inspect(std::function<void()> snapshotCb)
    {
        auto snap = [&]{ if (snapshotCb) snapshotCb(); };

        // --- Animator dependency -------------------------------------------
        AnimatorComponent* anim = owner ? owner->getComponent<AnimatorComponent>() : nullptr;
        if (!anim)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Needs an Animator on this entity.");
            if (owner && ImGui::Button("Add Animator", ImVec2(-1, 0)))
            {
                snap();
                owner->addComponent<AnimatorComponent>();
            }
            ImGui::Spacing();
        }

        // Clip names from the sibling Animator (for the state -> clip dropdown).
        std::vector<std::string> clipNames;
        if (anim) for (auto& [name, clip] : anim->clips) clipNames.push_back(name);

        // Live readout during Play.
        if (!currentState_.empty())
        {
            ImGui::TextDisabled("Current:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "%s", currentState_.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%.2fs)", timeInState_);
        }

        // ===================================================================
        // Parameters
        // ===================================================================
        if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(8.0f);
            const char* typeNames[] = { "Float", "Bool", "Trigger" };
            int paramToDelete = -1;

            for (int i = 0; i < (int)params.size(); i++)
            {
                Param& p = params[i];
                ImGui::PushID(i);

                char nameBuf[64] = {};
                strncpy(nameBuf, p.name.c_str(), sizeof(nameBuf) - 1);
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::InputText("##pname", nameBuf, sizeof(nameBuf)))
                {
                    snap();
                    std::string oldName = p.name;
                    p.name = nameBuf;
                    if (oldName != p.name) // keep conditions pointing at the renamed parameter
                        for (auto& tr : transitions)
                            for (auto& cond : tr.conditions)
                                if (cond.param == oldName) cond.param = p.name;
                }

                ImGui::SameLine();
                int t = (int)p.type;
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::Combo("##ptype", &t, typeNames, 3)) { snap(); p.type = (ParamType)t; }

                ImGui::SameLine();
                ImGui::SetNextItemWidth(90.0f);
                if (p.type == ParamType::Float)
                {
                    ImGui::DragFloat("##pval", &p.number, 0.1f);
                    if (ImGui::IsItemActivated()) snap();
                }
                else if (p.type == ParamType::Bool)
                {
                    ImGui::Checkbox("##pflag", &p.flag);
                    if (ImGui::IsItemActivated()) snap();
                }
                else // Trigger
                {
                    if (p.flag) ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "set");
                    else if (ImGui::SmallButton("Fire")) p.flag = true; // runtime test
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("x")) paramToDelete = i;
                ImGui::PopID();
            }

            if (paramToDelete >= 0) { snap(); params.erase(params.begin() + paramToDelete); }

            if (ImGui::Button("+ Parameter", ImVec2(-1, 0)))
            {
                snap();
                params.push_back({ "param" + std::to_string(params.size()), ParamType::Float, 0.0f, false });
            }
            ImGui::Unindent(8.0f);
        }

        // ===================================================================
        // States
        // ===================================================================
        if (ImGui::CollapsingHeader("States", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(8.0f);
            int stateToDelete = -1;

            for (int i = 0; i < (int)states.size(); i++)
            {
                State& s = states[i];
                ImGui::PushID(1000 + i);

                bool isDefault = (defaultState == s.name);
                if (ImGui::RadioButton("##def", isDefault)) { snap(); defaultState = s.name; }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Default (entry) state");

                ImGui::SameLine();
                char nameBuf[64] = {};
                strncpy(nameBuf, s.name.c_str(), sizeof(nameBuf) - 1);
                ImGui::SetNextItemWidth(110.0f);
                if (ImGui::InputText("##sname", nameBuf, sizeof(nameBuf)))
                {
                    std::string newName = nameBuf;
                    // Reject empty / the reserved "Any State" sentinel: an empty name
                    // collides with currentState_'s "no state" meaning, and either value
                    // would make this state's outgoing transitions read as "from any".
                    if (!newName.empty() && newName != "Any State")
                    {
                        snap();
                        std::string oldName = s.name;
                        s.name = newName;
                        if (defaultState == oldName) defaultState = newName; // keep entry pointer valid
                        for (auto& tr : transitions) // repoint transitions at the renamed state
                        {
                            if (tr.from == oldName) tr.from = newName;
                            if (tr.to   == oldName) tr.to   = newName;
                        }
                    }
                }

                ImGui::SameLine();
                ImGui::TextDisabled("->");
                ImGui::SameLine();

                // Clip: dropdown when the Animator has clips, else free text.
                ImGui::SetNextItemWidth(120.0f);
                if (!clipNames.empty())
                {
                    int sel = -1;
                    for (int c = 0; c < (int)clipNames.size(); c++) if (clipNames[c] == s.clip) { sel = c; break; }
                    std::vector<const char*> ptrs;
                    for (auto& n : clipNames) ptrs.push_back(n.c_str());
                    if (ImGui::Combo("##sclip", &sel, ptrs.data(), (int)ptrs.size())) { snap(); s.clip = clipNames[sel]; }
                }
                else
                {
                    char clipBuf[64] = {};
                    strncpy(clipBuf, s.clip.c_str(), sizeof(clipBuf) - 1);
                    if (ImGui::InputText("##sclip", clipBuf, sizeof(clipBuf))) { snap(); s.clip = clipBuf; }
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("x")) stateToDelete = i;
                ImGui::PopID();
            }

            if (stateToDelete >= 0)
            {
                snap();
                std::string removed = states[stateToDelete].name;
                states.erase(states.begin() + stateToDelete);
                if (defaultState == removed) defaultState.clear(); // resolveDefault_ falls back to first state
                // Drop transitions that referenced the removed state ("Any State" survives).
                transitions.erase(std::remove_if(transitions.begin(), transitions.end(),
                    [&](const Transition& tr){ return tr.from == removed || tr.to == removed; }),
                    transitions.end());
            }

            if (ImGui::Button("+ State", ImVec2(-1, 0)))
            {
                snap();
                State s; s.name = "state" + std::to_string(states.size());
                if (!clipNames.empty()) s.clip = clipNames.front();
                if (states.empty() && defaultState.empty()) defaultState = s.name;
                states.push_back(std::move(s));
            }
            ImGui::Unindent(8.0f);
        }

        // ===================================================================
        // Transitions
        // ===================================================================
        if (ImGui::CollapsingHeader("Transitions", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(8.0f);

            // Option lists for the from/to dropdowns.
            std::vector<std::string> fromOpts = { "Any State" };
            for (auto& s : states) fromOpts.push_back(s.name);
            std::vector<std::string> toOpts;
            for (auto& s : states) toOpts.push_back(s.name);

            auto stringCombo = [&](const char* id, std::string& value,
                                   const std::vector<std::string>& opts, bool anyIsEmpty)
            {
                int sel = 0;
                for (int k = 0; k < (int)opts.size(); k++)
                    if (opts[k] == value || (anyIsEmpty && k == 0 && (value.empty() || value == "Any State"))) { sel = k; break; }
                std::vector<const char*> ptrs;
                for (auto& o : opts) ptrs.push_back(o.c_str());
                ImGui::SetNextItemWidth(110.0f);
                if (!ptrs.empty() && ImGui::Combo(id, &sel, ptrs.data(), (int)ptrs.size()))
                {
                    snap();
                    value = (anyIsEmpty && sel == 0) ? "" : opts[sel];
                    return true;
                }
                return false;
            };

            const char* opLabels[] = { ">", "<", "==", "!=", "is true", "is false", "triggered" };
            int transToDelete = -1;

            for (int i = 0; i < (int)transitions.size(); i++)
            {
                Transition& tr = transitions[i];
                ImGui::PushID(2000 + i);
                ImGui::Separator();

                stringCombo("##from", tr.from, fromOpts, true);
                ImGui::SameLine(); ImGui::TextDisabled(">>"); ImGui::SameLine();
                stringCombo("##to", tr.to, toOpts, false);
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) transToDelete = i;

                ImGui::Checkbox("Has Exit Time", &tr.hasExitTime);
                if (ImGui::IsItemActivated()) snap();
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80.0f);
                ImGui::DragFloat("Min Time", &tr.minTime, 0.05f, 0.0f, 1000.0f, "%.2f");
                if (ImGui::IsItemActivated()) snap();

                // Exit Time waits for the from-state's clip to stop playing; a looping
                // clip never does, so the transition would never fire — warn the author.
                if (tr.hasExitTime && anim && !tr.from.empty() && tr.from != "Any State")
                {
                    auto sit = std::find_if(states.begin(), states.end(),
                                            [&](const State& s){ return s.name == tr.from; });
                    if (sit != states.end())
                    {
                        auto cit = anim->clips.find(sit->clip);
                        if (cit != anim->clips.end() && cit->second.loop)
                            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                                               "  Exit Time ignored: clip '%s' loops", sit->clip.c_str());
                    }
                }

                // Conditions
                int condToDelete = -1;
                for (int c = 0; c < (int)tr.conditions.size(); c++)
                {
                    Condition& cond = tr.conditions[c];
                    ImGui::PushID(c);
                    ImGui::Indent(12.0f);

                    // param dropdown
                    int psel = -1;
                    std::vector<const char*> pptrs;
                    for (int k = 0; k < (int)params.size(); k++)
                    {
                        pptrs.push_back(params[k].name.c_str());
                        if (params[k].name == cond.param) psel = k;
                    }
                    ImGui::SetNextItemWidth(100.0f);
                    if (!pptrs.empty() && ImGui::Combo("##cparam", &psel, pptrs.data(), (int)pptrs.size()))
                    { snap(); cond.param = params[psel].name; }

                    ImGui::SameLine();
                    int op = (int)cond.op;
                    ImGui::SetNextItemWidth(70.0f);
                    if (ImGui::Combo("##cop", &op, opLabels, 7)) { snap(); cond.op = (Op)op; }

                    // threshold only for numeric comparisons
                    if (cond.op == Op::Greater || cond.op == Op::Less ||
                        cond.op == Op::Equals  || cond.op == Op::NotEquals)
                    {
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(70.0f);
                        ImGui::DragFloat("##cthr", &cond.threshold, 0.1f);
                        if (ImGui::IsItemActivated()) snap();
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("x")) condToDelete = c;

                    ImGui::Unindent(12.0f);
                    ImGui::PopID();
                }
                if (condToDelete >= 0) { snap(); tr.conditions.erase(tr.conditions.begin() + condToDelete); }

                ImGui::Indent(12.0f);
                if (ImGui::SmallButton("+ Condition"))
                {
                    snap();
                    Condition c;
                    if (!params.empty()) c.param = params.front().name;
                    tr.conditions.push_back(c);
                }
                ImGui::Unindent(12.0f);

                ImGui::PopID();
            }

            if (transToDelete >= 0) { snap(); transitions.erase(transitions.begin() + transToDelete); }

            ImGui::Separator();
            if (ImGui::Button("+ Transition", ImVec2(-1, 0)))
            {
                snap();
                Transition tr;
                if (!states.empty()) tr.to = states.front().name;
                transitions.push_back(std::move(tr));
            }
            ImGui::Unindent(8.0f);
        }
    }
}
