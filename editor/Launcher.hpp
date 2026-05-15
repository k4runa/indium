#pragma once
#include "imgui.h"
#include "../core/ProjectManager.hpp"
#include "../tools/FileBrowser.hpp"
#include "raylib.h"
#include <string>

namespace Indium
{
    /**
     * @brief The Project Hub / Launcher interface.
     *
     * This is the first screen the user sees when opening the engine.
     * It handles creating new projects, loading recent ones, and general settings.
     */
    class Launcher
    {
    private:
        ProjectManager* pm;
        std::vector<RecentProject> recentProjects;

        // New Project Modal State
        char newProjName[128] = "MyNewGame";
        std::string selectedLocation = "";
        bool showLocationBrowser = false;

        // Open Project State
        bool showOpenBrowser = false;

        void RefreshRecents()
        {
            recentProjects = pm->GetRecentProjects();
        }

    public:
        Launcher(ProjectManager* projectManager) : pm(projectManager)
        {
            RefreshRecents();

            // Set default location to HOME folder
            const char* home = getenv("HOME");
            if (home) selectedLocation = std::string(home) + "/IndiumProjects";
            else selectedLocation = "C:/IndiumProjects";
        }

        /**
         * @brief Draws the full Launcher UI.
         * @return true if a project was loaded and we should transition to Editor mode.
         */
        bool Draw(Scene* scene)
        {
            bool projectLoaded = false;

            // Make the window fullscreen
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            // Minimalist dark theme for the launcher
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));

            ImGui::Begin("Indium Hub", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

            /* --------------------- LEFT SIDEBAR --------------------- */
            ImGui::BeginChild("Sidebar", ImVec2(200, 0), true);

            ImGui::SetCursorPosY(20);
            ImGui::SetWindowFontScale(1.5f);
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), " Indium");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextDisabled("   Game Engine");

            ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            ImGui::Selectable(" Projects", true);
            ImGui::Selectable(" Installs", false);
            ImGui::Selectable(" Learn", false);
            ImGui::Selectable(" Settings", false);

            ImGui::EndChild();
            ImGui::SameLine();

            /* --------------------- MAIN CONTENT --------------------- */
            ImGui::BeginChild("MainContent", ImVec2(0, 0), false);

            ImGui::SetCursorPosY(20);
            ImGui::Text("Projects");
            ImGui::SameLine(ImGui::GetWindowWidth() - 250);

            if (ImGui::Button("Open", ImVec2(100, 30)))
            {
                showOpenBrowser = true;
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.6f, 0.9f, 1.0f));
            if (ImGui::Button("New Project", ImVec2(120, 30)))
            {
                ImGui::OpenPopup("Create New Project");
            }
            ImGui::PopStyleColor(2);

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            // Recent Projects List
            if (recentProjects.empty())
            {
                ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2 - 20);
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 100);
                ImGui::TextDisabled("You have no recent projects.");
            }
            else
            {
                if (ImGui::BeginTable("RecentProjectsTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_PadOuterX))
                {
                    ImGui::TableSetupColumn("Project Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
                    ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthStretch, 3.0f);
                    ImGui::TableSetupColumn("Last Opened", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableHeadersRow();

                    std::string toRemove = "";

                    for (const auto& rp : recentProjects)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);

                        if (ImGui::Selectable(rp.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                        {
                            // Load this project!
                            if (pm->LoadProject(rp.path, *scene))
                            {
                                projectLoaded = true;
                            }
                            else
                            {
                                // Remove if missing?
                                toRemove = rp.path;
                            }
                        }

                        // Right click context menu
                        if (ImGui::BeginPopupContextItem(("CTX_" + rp.path).c_str()))
                        {
                            if (ImGui::MenuItem("Remove from list")) toRemove = rp.path;
                            if (ImGui::MenuItem("Reveal in File Explorer"))
                            {
                                // Simple system call (Linux specific, but good enough for now)
                                std::string cmd = "xdg-open \"" + rp.path + "\" &";
                                system(cmd.c_str());
                            }
                            ImGui::EndPopup();
                        }

                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextDisabled("%s", rp.path.c_str());

                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextDisabled("%s", rp.lastOpened.c_str());
                    }
                    ImGui::EndTable();

                    if (!toRemove.empty())
                    {
                        pm->RemoveRecentProject(toRemove);
                        RefreshRecents();
                    }
                }
            }

            // --- Modals ---

            // 1. New Project Modal
            ImGui::SetNextWindowSize(ImVec2(500, 250), ImGuiCond_Appearing);
            if (ImGui::BeginPopupModal("Create New Project", nullptr, ImGuiWindowFlags_NoResize))
            {
                ImGui::Text("Project Name:");
                ImGui::InputText("##projname", newProjName, sizeof(newProjName));

                ImGui::Spacing();
                ImGui::Text("Location:");
                ImGui::InputText("##projloc", (char*)selectedLocation.c_str(), selectedLocation.capacity(), ImGuiInputTextFlags_ReadOnly);
                ImGui::SameLine();
                if (ImGui::Button("..."))
                {
                    showLocationBrowser = true;
                }

                ImGui::Spacing(); ImGui::Spacing();
                std::string fullPath = selectedLocation + "/" + newProjName;
                ImGui::TextDisabled("Will be created at: %s", fullPath.c_str());

                ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
                if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
                ImGui::SameLine(ImGui::GetWindowWidth() - 110);

                if (strlen(newProjName) == 0 || selectedLocation.empty()) ImGui::BeginDisabled();
                if (ImGui::Button("Create", ImVec2(100, 0)))
                {
                    if (pm->CreateProject(selectedLocation, newProjName))
                    {
                        // Auto-load it
                        if (pm->LoadProject(fullPath, *scene))
                        {
                            projectLoaded = true;
                        }
                        RefreshRecents();
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (strlen(newProjName) == 0 || selectedLocation.empty()) ImGui::EndDisabled();

                ImGui::EndPopup();
            }

            if (showLocationBrowser) ImGui::OpenPopup("Select Project Location");
            std::string selectedLoc;
            if (FileBrowser::Draw("Select Project Location", selectedLoc, {}, true)) // true for folder selection
            {
                selectedLocation = selectedLoc;
                showLocationBrowser = false;
            }
            if (!ImGui::IsPopupOpen("Select Project Location")) showLocationBrowser = false;

            // 2. Open Project Browser
            if (showOpenBrowser) ImGui::OpenPopup("Open Project");
            std::string openedProj;
            if (FileBrowser::Draw("Open Project", openedProj, {}, true)) // true for folder selection
            {
                if (pm->LoadProject(openedProj, *scene))
                {
                    projectLoaded = true;
                    RefreshRecents();
                }
                showOpenBrowser = false;
            }
            if (!ImGui::IsPopupOpen("Open Project")) showOpenBrowser = false;

            ImGui::EndChild(); // MainContent
            ImGui::End(); // Indium Hub

            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);

            return projectLoaded;
        }
    };
}
