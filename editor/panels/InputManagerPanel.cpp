#include "../Editor.hpp"

namespace Indium
{
    void Editor::ShowInputManager()
    {
        ImGui::SetNextWindowSize(ImVec2(480, 420), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(200, 200), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin(ICON_FA_GAMEPAD "  Input Action Map", &showInputManager_, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::End();
            return;
        }

        ImGui::TextDisabled("Define named actions that scripts query via InputManager::Get().IsPressed(\"Jump\")");
        ImGui::Spacing();

        // --- Key capture overlay ---
        if (capturingKey_)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.08f, 0.05f, 1.0f));
            ImGui::BeginChild("##capture", ImVec2(-1, 48), true);
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), ICON_FA_KEYBOARD "  Press any key or mouse button for action \"%s\"  (Esc = cancel)", capturingAction_.c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();

            // Detect pressed key
            int pressed = GetKeyPressed();
            if (pressed == KEY_ESCAPE) { capturingKey_ = false; }
            else if (pressed > 0)
            {
                InputManager::Get().SetAction(capturingAction_, pressed);
                capturingKey_ = false;
                if (pm.IsProjectOpen()) InputManager::Get().Save(pm.GetCurrentProjectPath() + "/input.json");
            }
            else
            {
                // Check mouse buttons
                for (int mb = 0; mb < 5; mb++)
                {
                    if (IsMouseButtonPressed((MouseButton)mb))
                    {
                        InputManager::Get().SetMouseAction(capturingAction_, mb);
                        capturingKey_ = false;
                        if (pm.IsProjectOpen()) InputManager::Get().Save(pm.GetCurrentProjectPath() + "/input.json");
                        break;
                    }
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
        }

        // --- Action table ---
        auto& actions = InputManager::Get().GetActions();

        // Collect sorted list for stable display order
        std::vector<std::string> actionNames;
        actionNames.reserve(actions.size());
        for (auto& [name, _] : actions) actionNames.push_back(name);
        std::sort(actionNames.begin(), actionNames.end());
        std::string toRemove;

        if (!actionNames.empty() && ImGui::BeginTable("##ActTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, -60.0f)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Action",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("##ops",   ImGuiTableColumnFlags_WidthFixed,  84.0f);
            ImGui::TableHeadersRow();

            for (const auto& name : actionNames)
            {
                auto it = actions.find(name);
                if (it == actions.end()) continue;
                const ActionBinding& b = it->second;

                ImGui::TableNextRow();
                ImGui::PushID(name.c_str());

                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(name.c_str());

                ImGui::TableSetColumnIndex(1);
                std::string bindStr = InputManager::BindingName(b);
                bool isCapturing = (capturingKey_ && capturingAction_ == name);
                if (isCapturing) { ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "..."); }
                else { ImGui::TextUnformatted(bindStr.c_str()); }

                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton(isCapturing ? "Cancel" : "Rebind"))
                {
                    if (isCapturing) capturingKey_ = false;
                    else { capturingKey_ = true; capturingAction_ = name; }
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
                if (ImGui::SmallButton(ICON_FA_TRASH)) toRemove = name;
                ImGui::PopStyleColor();
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
        else if (actionNames.empty())
        {
            ImGui::TextDisabled("(no actions defined)");
            ImGui::Spacing();
        }

        if (!toRemove.empty())
        {
            InputManager::Get().RemoveAction(toRemove);
            if (pm.IsProjectOpen()) InputManager::Get().Save(pm.GetCurrentProjectPath() + "/input.json");
        }

        // --- Add new action row ---
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushItemWidth(180.0f);
        ImGui::InputText("##NewActionName", newActionNameBuf_, sizeof(newActionNameBuf_));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        bool canAdd = newActionNameBuf_[0] != '\0' && !InputManager::Get().HasAction(newActionNameBuf_);
        if (!canAdd) ImGui::BeginDisabled();
        if (ImGui::Button("Add Action"))
        {
            InputManager::Get().SetAction(newActionNameBuf_, 0);
            capturingKey_        = true;
            capturingAction_     = newActionNameBuf_;
            newActionNameBuf_[0] = '\0';
            if (pm.IsProjectOpen()) InputManager::Get().Save(pm.GetCurrentProjectPath() + "/input.json");
        }
        if (!canAdd) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FLOPPY_DISK "  Save"))
        {
            if (pm.IsProjectOpen()) InputManager::Get().Save(pm.GetCurrentProjectPath() + "/input.json");
        }

        ImGui::End();
    }
}
