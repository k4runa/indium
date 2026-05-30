#include "../Editor.hpp"

namespace Indium
{
    void Editor::ShowStoryState()
    {
        if (!pm.IsProjectOpen()) { ImGui::TextDisabled("No project open."); return; }
        const bool playing = (state == GameState::Play);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        ImGui::TextDisabled("   " ICON_FA_FLAG "  %s", playing ? "Runtime Blackboard (live — discarded on Stop)" : "Authored Scene Flags (seeded into the blackboard on Play)");
        ImGui::PopStyleVar();
        ImGui::Separator();

        // Snapshot the entries so the backing store can be mutated while iterating.
        std::vector<std::pair<std::string, StoryValue>> entries;
        if (playing) {for (const auto& kv : StoryState::Get().Values()) entries.emplace_back(kv.first, kv.second);}
        else {for (const auto& kv : scene.storyState) entries.emplace_back(kv.first, kv.second);}

        const float addRowH = ImGui::GetFrameHeightWithSpacing() + 8.0f;
        ImGui::BeginChild("StoryStateScroll", ImVec2(0, -addRowH), false);

        if (entries.empty()) { ImGui::TextDisabled("(No story variables defined)"); }
        else if (ImGui::BeginTable("StoryStateTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Key",   ImGuiTableColumnFlags_WidthStretch, 0.45f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
            ImGui::TableSetupColumn("##rm",  ImGuiTableColumnFlags_WidthFixed,   26.0f);
            ImGui::TableHeadersRow();

            std::string removeKey;

            for (auto& entry : entries)
            {
                const std::string& key   = entry.first;
                StoryValue&        value = entry.second;

                ImGui::TableNextRow();
                ImGui::PushID(key.c_str());

                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(key.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                bool changed = false;
                std::visit([&](auto&& arg)
                {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, bool>)
                    {
                        bool b = arg;
                        if (ImGui::Checkbox("##val", &b)) { value = b; changed = true; }
                    }
                    else if constexpr (std::is_same_v<T, int>)
                    {
                        int n = arg;
                        if (ImGui::DragInt("##val", &n)) { value = n; changed = true; }
                    }
                    else if constexpr (std::is_same_v<T, float>)
                    {
                        float f = arg;
                        if (ImGui::DragFloat("##val", &f, 0.1f)) { value = f; changed = true; }
                    }
                    else if constexpr (std::is_same_v<T, std::string>)
                    {
                        char buf[256] = {};
                        strncpy(buf, arg.c_str(), sizeof(buf) - 1);
                        if (ImGui::InputText("##val", buf, sizeof(buf))) { value = std::string(buf); changed = true; }
                    }
                }, value);
                const bool activated = ImGui::IsItemActivated();
                ImGui::PopItemWidth();

                if (playing) { if (changed) StoryState::Get().Set(key, value); }
                else { if (activated) TakeSnapshot(); if (changed) scene.storyState[key] = value; }
                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton(ICON_FA_XMARK)) removeKey = key;
                ImGui::PopID();
            }

            ImGui::EndTable();

            if (!removeKey.empty())
            {
                if (playing) StoryState::Get().Remove(removeKey);
                else { TakeSnapshot(); scene.storyState.erase(removeKey); }
            }
        }

        ImGui::EndChild();

        // --- Add variable row ---
        ImGui::Separator();

        static char newKey[64]  = "";
        static int  newType     = 0; // 0 Flag(bool), 1 Int, 2 Float, 3 Text(string)
        const char* typeNames[] = { "Flag", "Int", "Float", "Text" };

        ImGui::TextDisabled("New:");
        ImGui::SameLine();
        ImGui::PushItemWidth(150.0f);
        ImGui::InputText("##newkey", newKey, sizeof(newKey));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(90.0f);
        ImGui::Combo("##newtype", &newType, typeNames, IM_ARRAYSIZE(typeNames));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Add"))
        {
            const std::string k = newKey;
            if (!k.empty())
            {
                StoryValue v{ false };
                if      (newType == 1) v = 0;
                else if (newType == 2) v = 0.0f;
                else if (newType == 3) v = std::string{};

                if (playing) StoryState::Get().Set(k, v);
                else { TakeSnapshot(); scene.storyState[k] = v; }
                newKey[0] = '\0';
            }
        }
    }
}
