#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace Indium
{
    /**
     * @brief A simple ImGui-based file browser for selecting assets.
     */
    class FileBrowser
    {
    public:
        static bool Draw(const std::string& title, std::string& outPath, const std::vector<std::string>& extensions = {})
        {
            static fs::path currentPath = fs::current_path();
            static std::string selectedFile = "";
            bool result = false;

            if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Current Path: %s", currentPath.string().c_str());
                ImGui::Separator();

                // Go up one level
                if (ImGui::Button(".."))
                {
                    if (currentPath.has_parent_path())
                        currentPath = currentPath.parent_path();
                }

                ImGui::BeginChild("FileView", ImVec2(500, 300), true);
                
                try {
                    for (const auto& entry : fs::directory_iterator(currentPath))
                    {
                        const auto& path = entry.path();
                        std::string filename = path.filename().string();

                        if (entry.is_directory())
                        {
                            if (ImGui::Selectable((filename + "/").c_str(), false, ImGuiSelectableFlags_AllowDoubleClick))
                            {
                                if (ImGui::IsMouseDoubleClicked(0))
                                {
                                    currentPath /= path.filename();
                                }
                            }
                        }
                        else
                        {
                            // Filter by extension
                            if (!extensions.empty())
                            {
                                std::string ext = path.extension().string();
                                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                                
                                bool match = false;
                                for (const auto& e : extensions)
                                {
                                    if (ext == e) { match = true; break; }
                                }
                                if (!match) continue;
                            }

                            if (ImGui::Selectable(filename.c_str(), selectedFile == filename))
                            {
                                selectedFile = filename;
                            }
                        }
                    }
                } catch (...) {
                    ImGui::TextColored(ImVec4(1,0,0,1), "Access Denied!");
                }
                
                ImGui::EndChild();

                ImGui::Separator();
                ImGui::Text("Selected: %s", selectedFile.c_str());

                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Select", ImVec2(120, 0)) && !selectedFile.empty())
                {
                    outPath = (currentPath / selectedFile).string();
                    result = true;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            return result;
        }
    };
}
