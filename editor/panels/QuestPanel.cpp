#include "../Editor.hpp"

namespace Indium
{
    // Quest authoring + live-debug panel. Mirrors ShowStoryState: it reads authored
    // definitions when stopped and live runtime state (from StoryState, via QuestManager)
    // during Play, where it can also drive quests for testing.
    void Editor::ShowQuests()
    {
        if (!pm.IsProjectOpen()) { ImGui::TextDisabled("No project open."); return; }

        auto& qm = QuestManager::Get();
        const bool playing = (state == GameState::Play);
        qm.SetProjectPath(pm.GetCurrentProjectPath());
        if (!playing && qm.Definitions().empty()) qm.LoadAll();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        ImGui::TextDisabled("   %s", playing ? "Live Quests (runtime — discarded on Stop)" : "Quest Definitions (project's quests/ folder)");
        ImGui::PopStyleVar();
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload")) qm.LoadAll();
        if (playing)
        {
            bool logOpen = qm.LogOpen();
            ImGui::SameLine();
            if (ImGui::Checkbox("Show Log", &logOpen)) qm.SetLogOpen(logOpen);
        }
        ImGui::Separator();

        const auto& defs = qm.Definitions();
        if (defs.empty())
        {
            ImGui::TextDisabled("(No quests found — add quests/<id>.json to the project)");
            return;
        }

        for (const auto& [id, d] : defs)
        {
            ImGui::PushID(id.c_str());

            const char* modeStr = (d.mode == QuestMode::Parallel) ? "parallel" : "sequential";
            std::string header  = d.title.empty() ? id : d.title;
            if (playing)
            {
                const QuestState st    = qm.StateOf(id);
                const char*      stStr = st == QuestState::Complete ? "Complete" : st == QuestState::Active   ? "Active" : "Not started";
                header += "   [" + std::string(stStr) + "]";
            }

            if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TextDisabled("id: %s    mode: %s", id.c_str(), modeStr);
                if (!d.description.empty()) ImGui::TextWrapped("%s", d.description.c_str());

                for (std::size_t i = 0; i < d.objectives.size(); ++i)
                {
                    const QuestObjective& o = d.objectives[i];
                    if (playing)
                    {
                        ImGui::PushID((int)i);
                        bool done = qm.IsObjectiveDone(o);
                        // Toggling the box flips the backing StoryState flag — quick way to
                        // exercise quest progression without playing through the trigger.
                        if (ImGui::Checkbox("##obj", &done) && !o.completeFlag.empty())
                        {
                            if (done) StoryState::Get().SetFlag(o.completeFlag);
                            else      StoryState::Get().ClearFlag(o.completeFlag);
                        }
                        ImGui::SameLine();
                        ImGui::TextUnformatted((o.desc.empty() ? o.id : o.desc).c_str());
                        ImGui::PopID();
                    }
                    else if (!o.completeWhen.empty())
                    {
                        ImGui::BulletText("%s   (when: %s)", (o.desc.empty() ? o.id : o.desc).c_str(), o.completeWhen.c_str());
                    }
                    else
                    {
                        ImGui::BulletText("%s   (flag: %s)", (o.desc.empty() ? o.id : o.desc).c_str(), o.completeFlag.empty() ? "-" : o.completeFlag.c_str());
                    }
                }

                if (playing)
                {
                    if (ImGui::SmallButton("Start"))    qm.Start(id);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Advance"))  qm.Advance(id);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Complete")) qm.Complete(id);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Abandon"))  qm.Abandon(id);
                }
            }

            ImGui::PopID();
        }
    }
}
