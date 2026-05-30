#include "../Editor.hpp"

namespace Indium
{
    void Editor::ShowConsole()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        if (ImGui::Button(ICON_FA_TRASH "  Clear")) { consoleLogs.clear(); }
        ImGui::SameLine();
        ImGui::TextDisabled("   " ICON_FA_TERMINAL "  System Console");
        ImGui::PopStyleVar();
        ImGui::Separator();
        ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& log : consoleLogs)
        {
            ImGui::TextColored(log.color, "%s %s", log.icon, log.level.c_str());
            ImGui::SameLine();
            ImGui::Text("%s", log.message.c_str());
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
    }
}
