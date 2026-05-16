#pragma once
#include "imgui.h"
#include "../core/ProjectManager.hpp"
#include "../tools/FileBrowser.hpp"
#include "raylib.h"
#include <string>
#include <map>
#include <algorithm>

namespace Indium
{
    /**
     * @brief The Project Hub / Launcher interface.
     *
     * This is the first screen the user sees when opening the engine.
     * It handles creating new projects, loading recent ones, and general settings.
     */
    enum class LauncherTab { Projects, Settings };

    class Launcher
    {
        private:
            ProjectManager* pm;
            std::vector<RecentProject> recentProjects;
            LauncherTab currentTab = LauncherTab::Projects;
            std::map<ImGuiID, float> animStates;

            // Settings State
            char defaultProjPath[256] = "";

            // New Project Modal State
            char newProjName[128] = "MyNewGame";
            std::string selectedLocation = "";
            bool showLocationBrowser = false;

            // Open Project State
            bool showOpenBrowser = false;

            // Loading Transition State
            bool isTransitioning = false;
            float transitionTimer = 0.0f;
            const float transitionDuration = 0.6f;

            void RefreshRecents()
            {
                recentProjects = pm->GetRecentProjects();
            }

            bool DrawSidebarItem(const char* label, bool selected)
            {
                ImGuiID id = ImGui::GetID(label);
                float& anim = animStates[id];

                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 size = ImVec2(220, 45);
                ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);

                ImGui::InvisibleButton(label, size);
                bool hovered = ImGui::IsItemHovered();
                bool clicked = ImGui::IsItemClicked();

                if (hovered || selected) anim += GetFrameTime() * 10.0f;
                else anim -= GetFrameTime() * 10.0f;
                anim = std::clamp(anim, 0.0f, 1.0f);

                ImDrawList* drawList = ImGui::GetWindowDrawList();

                // Background
                ImVec4 baseCol = ImVec4(0, 0, 0, 0);
                ImVec4 hovCol = selected ? ImVec4(0.07f, 0.07f, 0.07f, 1.0f) : ImVec4(0.05f, 0.05f, 0.05f, 1.0f);

                ImU32 bgCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    baseCol.x + (hovCol.x - baseCol.x) * anim,
                    baseCol.y + (hovCol.y - baseCol.y) * anim,
                    baseCol.z + (hovCol.z - baseCol.z) * anim,
                    anim
                ));

                drawList->AddRectFilled(p0, p1, bgCol, 6.0f);

                // Active/Hover Indicator
                float indicatorAlpha = selected ? 1.0f : (anim * 0.5f);
                if (indicatorAlpha > 0.0f)
                {
                    ImU32 indicatorCol = ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, indicatorAlpha));
                    drawList->AddRectFilled(ImVec2(p0.x, p0.y + 12), ImVec2(p0.x + 4, p1.y - 12), indicatorCol, 2.0f);
                }

                drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(p0.x + 25, p0.y + 13), selected ? ImColor(255, 255, 255) : ImColor(180, 180, 180), label);

                return clicked;
            }

        public:
            Launcher(ProjectManager* projectManager) : pm(projectManager)
            {
                RefreshRecents();

                // Set default location
                std::string prefPath = pm->GetDefaultProjectPath();
                if (prefPath.empty())
                {
                    const char* home = getenv("HOME");
                    if (home) selectedLocation = std::string(home) + "/IndiumProjects";
                    else selectedLocation = "C:/IndiumProjects";
                    pm->SetDefaultProjectPath(selectedLocation);
                }
                else
                {
                    selectedLocation = prefPath;
                }

                strncpy(defaultProjPath, selectedLocation.c_str(), sizeof(defaultProjPath));
            }

            /** @brief Draws an animated button that smoothly fades on hover */
            bool AnimatedButton(const char* label, ImVec2 size, ImVec4 baseCol, ImVec4 hoverCol)
            {
                ImGuiID id = ImGui::GetID(label);
                float& state = animStates[id];

                ImVec4 col;
                col.x = baseCol.x + (hoverCol.x - baseCol.x) * state;
                col.y = baseCol.y + (hoverCol.y - baseCol.y) * state;
                col.z = baseCol.z + (hoverCol.z - baseCol.z) * state;
                col.w = baseCol.w + (hoverCol.w - baseCol.w) * state;

                ImGui::PushStyleColor(ImGuiCol_Button, col);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col); // Prevent ImGui snap
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, hoverCol);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

                bool pressed = ImGui::Button(label, size);

                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();

                if (ImGui::IsItemHovered()) state += GetFrameTime() * 8.0f;
                else state -= GetFrameTime() * 8.0f;

                state = std::clamp(state, 0.0f, 1.0f);
                return pressed;
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

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

                // Minimalist dark theme for the launcher
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.039f, 0.039f, 0.039f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.039f, 0.039f, 0.039f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.051f, 0.051f, 0.051f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));

                ImGui::Begin("Indium Hub", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

                ImGui::PopStyleVar(3); // Restore normal padding for child elements and popups!

                /* --------------------- LEFT SIDEBAR --------------------- */
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.04f, 0.04f, 1.0f));
                ImGui::BeginChild("Sidebar", ImVec2(240, 0), false);

                ImGui::SetCursorPosY(30);
                ImGui::SetCursorPosX(20);
                ImGui::SetWindowFontScale(1.8f);
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Indium");
                ImGui::SetWindowFontScale(1.0f);
                ImGui::SetCursorPosX(20);
                ImGui::TextDisabled("Hub");

                ImGui::Dummy(ImVec2(0, 5));
                ImGui::SetCursorPosX(20);
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 5));

                // Tabs
                ImGui::SetCursorPosX(10);
                if (DrawSidebarItem("Projects", currentTab == LauncherTab::Projects)) currentTab = LauncherTab::Projects;
                ImGui::SetCursorPosX(10);
                if (DrawSidebarItem("Settings", currentTab == LauncherTab::Settings)) currentTab = LauncherTab::Settings;
                ImGui::EndChild();
                ImGui::PopStyleColor();
                ImGui::SameLine();

                /* --------------------- MAIN CONTENT --------------------- */
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(40, 40));
                ImGui::BeginChild("MainContent", ImVec2(0, 0), false);
                ImGui::PopStyleVar();
                ImGui::Dummy(ImVec2(0, 30)); // Top margin

                ImGui::SetCursorPosX(40);

                if (currentTab == LauncherTab::Projects)
                {
                    ImGui::SetWindowFontScale(1.5f);
                    ImGui::Text("Projects");
                    ImGui::SetWindowFontScale(1.0f);

                    ImGui::SameLine(ImGui::GetWindowWidth() - 320);

                    ImVec4 btnBase = ImVec4(0.5f, 0.5f, 0.5f, 0.0f);
                    ImVec4 btnHov  = ImVec4(0.7f, 0.7f, 0.7f, 0.0f);
                    ImVec4 accentBase = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
                    ImVec4 accentHov  = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);

                    if (AnimatedButton("Open", ImVec2(100, 35), btnBase, btnHov))
                    {
                        showOpenBrowser = true;
                    }
                    ImGui::SameLine();
                    if (AnimatedButton("New Project", ImVec2(140, 35), accentBase, accentHov))
                    {
                        ImGui::OpenPopup("Create New Project");
                    }

                    ImGui::Dummy(ImVec2(0, 20));
                    ImGui::SetCursorPosX(40);
                    ImGui::PushItemWidth(ImGui::GetWindowWidth() - 80);
                    ImGui::Separator();
                    ImGui::PopItemWidth();
                    ImGui::Dummy(ImVec2(0, 20));

                    // Recent Projects List
                    if (recentProjects.empty())
                    {
                        ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2 - 20);
                        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 100);
                        ImGui::TextDisabled("You have no recent projects.");
                    }
                    else
                    {
                        std::string toRemove = "";
                        static std::string missingProjectPath = "";

                        for (const auto& rp : recentProjects)
                        {
                            ImGui::SetCursorPosX(40);
                            ImGuiID id = ImGui::GetID(rp.path.c_str());
                            float& state = animStates[id];

                            ImVec2 p0 = ImGui::GetCursorScreenPos();
                            float cardWidth = ImGui::GetContentRegionAvail().x - 40.0f;
                            ImVec2 size = ImVec2(cardWidth, 85);
                            ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);

                            ImGui::InvisibleButton(rp.path.c_str(), size);
                            bool hovered = ImGui::IsItemHovered();
                            bool clicked = ImGui::IsItemClicked();

                            if (hovered) state += GetFrameTime() * 10.0f;
                            else state -= GetFrameTime() * 10.0f;
                            state = std::clamp(state, 0.0f, 1.0f);

                            // Interpolate colors for hover animation
                            ImVec4 baseCol = ImVec4(0.045f, 0.045f, 0.045f, 1.0f);
                            ImVec4 hovCol = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
                            ImU32 bgCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                                baseCol.x + (hovCol.x - baseCol.x) * state,
                                baseCol.y + (hovCol.y - baseCol.y) * state,
                                baseCol.z + (hovCol.z - baseCol.z) * state,
                                1.0f
                            ));
                            ImU32 borderCol = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, state * 0.5f + 0.1f));

                            ImDrawList* drawList = ImGui::GetWindowDrawList();
                            drawList->AddRectFilled(p0, p1, bgCol, 8.0f);
                            drawList->AddRect(p0, p1, borderCol, 8.0f, 0, 1.5f);

                            // Draw Icons & Text
                            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.25f, ImVec2(p0.x + 25, p0.y + 18), ImColor(250, 250, 250, 255), rp.name.c_str());
                            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f, ImVec2(p0.x + 25, p0.y + 48), ImColor(110, 110, 110, 255), rp.path.c_str());

                            // Date on the far right
                            const char* dateText = rp.lastOpened.c_str();
                            float dateWidth = ImGui::CalcTextSize(dateText).x;
                            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.8f, ImVec2(p1.x - dateWidth - 25, p0.y + 35), ImColor(80, 80, 80, 255), dateText);

                            if (clicked)
                            {
                                try
                                {
                                    if (pm->LoadProject(rp.path, *scene)) isTransitioning = true;
                                    else
                                    {
                                        std::cout << "WARNING: Could not find path: " << rp.path << std::endl;
                                        missingProjectPath = rp.path;
                                        ImGui::OpenPopup("Project Not Found");
                                    }
                                }
                                catch(const std::exception& e)
                                {
                                    std::cerr << e.what() << '\n';
                                }
                            }

                            // Right click context menu
                            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
                            if (ImGui::BeginPopupContextItem(("CTX_" + rp.path).c_str()))
                            {
                                if (ImGui::Selectable(ICON_FA_TRASH "  Remove from list")) toRemove = rp.path;
                                if (ImGui::Selectable(ICON_FA_FOLDER_OPEN "  Reveal in Explorer"))
                                {
                                    std::string cmd = "xdg-open \"" + rp.path + "\" &";
                                    system(cmd.c_str());
                                }
                                ImGui::EndPopup();
                            }
                            ImGui::PopStyleVar();

                            ImGui::Dummy(ImVec2(0, 12)); // Consistent gap between cards
                        }

                        // Project Not Found Modal (Outside loop to avoid ID conflicts)
                        ImGui::SetNextWindowSize(ImVec2(-1, 0), ImGuiCond_Appearing);
                        if (ImGui::BeginPopupModal("Project Not Found", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                        {
                            ImGui::Text("The project at the following path could not be found:\n\n%s\n\nWant to delete it from your recent projects?", missingProjectPath.c_str());
                            ImGui::Dummy(ImVec2(0, 10));
                            ImGui::Separator();
                            ImGui::Dummy(ImVec2(0, 10));

                            if (ImGui::Button("Delete", ImVec2(120, 0)))
                            {
                                toRemove = missingProjectPath;
                                ImGui::CloseCurrentPopup();
                            }

                            ImGui::SameLine();
                            if (ImGui::Button("Cancel", ImVec2(120, 0)))
                            {
                                ImGui::CloseCurrentPopup();
                            }

                            ImGui::EndPopup();
                        }

                        if (!toRemove.empty())
                        {
                            pm->RemoveRecentProject(toRemove);
                            RefreshRecents();
                        }
                    }
                }
                else if (currentTab == LauncherTab::Settings)
                {
                    ImGui::SetWindowFontScale(1.5f);
                    ImGui::Text("Settings");
                    ImGui::SetWindowFontScale(1.0f);

                    ImGui::Dummy(ImVec2(0, 20));
                    ImGui::SetCursorPosX(40);
                    ImGui::PushItemWidth(ImGui::GetWindowWidth() - 80);
                    ImGui::Separator();
                    ImGui::PopItemWidth();
                    ImGui::Dummy(ImVec2(0, 20));

                    ImGui::SetCursorPosX(40);
                    ImGui::TextDisabled("Manage engine-wide preferences and paths.");

                    ImGui::Dummy(ImVec2(0, 30));
                    ImGui::SetCursorPosX(40);
                    ImGui::Text("Default Projects Path");

                    ImGui::Dummy(ImVec2(0, 5));
                    ImGui::SetCursorPosX(40);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
                    ImGui::PushItemWidth(500);
                    if (ImGui::InputText("##defpath", defaultProjPath, sizeof(defaultProjPath)))
                    {
                        selectedLocation = std::string(defaultProjPath);
                    }
                    ImGui::PopItemWidth();
                    ImGui::PopStyleVar();

                    ImGui::SameLine();
                    ImVec4 btnBase = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
                    ImVec4 btnHov  = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);
                    if (AnimatedButton("Save", ImVec2(100, 30), btnBase, btnHov))
                    {
                        pm->SetDefaultProjectPath(selectedLocation);
                    }
                }

                // --- Modals ---

                // 1. New Project Modal
                ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_Always); // Drastically increased height
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30, 30));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));

                if (ImGui::BeginPopupModal("Create New Project", nullptr, ImGuiWindowFlags_NoResize))
                {
                    ImGui::SetWindowFontScale(1.1f);
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), ICON_FA_PEN_TO_SQUARE "  Configure Your Project");
                    ImGui::SetWindowFontScale(1.0f);
                    ImGui::Dummy(ImVec2(0, 15));

                    ImGui::TextDisabled("Project Name");
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 10));
                    ImGui::PushItemWidth(-1); // Full width
                    ImGui::InputText("##projname", newProjName, sizeof(newProjName));
                    ImGui::PopItemWidth();
                    ImGui::PopStyleVar();

                    ImGui::Dummy(ImVec2(0, 15));

                    ImGui::TextDisabled("Location");
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 10));
                    float avail = ImGui::GetContentRegionAvail().x;
                    ImGui::PushItemWidth(avail - 55);
                    char locBuf[512] = {};
                    strncpy(locBuf, selectedLocation.c_str(), sizeof(locBuf) - 1);
                    ImGui::InputText("##projloc", locBuf, sizeof(locBuf), ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopItemWidth();
                    ImGui::PopStyleVar();

                    ImGui::SameLine(avail - 45);
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN, ImVec2(45, 36)))
                    {
                        showLocationBrowser = true;
                    }

                    ImGui::Dummy(ImVec2(0, 25));
                    ImGui::Separator();
                    ImGui::Dummy(ImVec2(0, 10));

                    std::string fullPath = selectedLocation + "/" + newProjName;
                    ImGui::TextDisabled(ICON_FA_CIRCLE_INFO "  The project will be initialized at:");
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    ImGui::TextWrapped("%s", fullPath.c_str());
                    ImGui::PopStyleColor();

                    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 70);

                    if (ImGui::Button("Cancel", ImVec2(120, 40))) ImGui::CloseCurrentPopup();
                    ImGui::SameLine(ImGui::GetWindowWidth() - 150);

                    if (strlen(newProjName) == 0 || selectedLocation.empty()) ImGui::BeginDisabled();

                    ImVec4 accentBase = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
                    ImVec4 accentHov  = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
                    if (AnimatedButton("Create", ImVec2(120, 40), accentBase, accentHov))
                    {
                        if (pm->CreateProject(selectedLocation, newProjName))
                        {
                            if (pm->LoadProject(fullPath, *scene)) isTransitioning = true;
                            RefreshRecents();
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    if (strlen(newProjName) == 0 || selectedLocation.empty()) ImGui::EndDisabled();

                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar(3);

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
                std::string openedProjFile;
                if (FileBrowser::Draw("Open Project", openedProjFile, {".indp"}, false)) // false = select file, not directory
                {
                    if (!openedProjFile.empty())
                    {
                        fs::path p(openedProjFile);
                        if (p.has_parent_path())
                        {
                            std::string projectDir = p.parent_path().string();
                            if (pm->LoadProject(projectDir, *scene))
                            {
                                isTransitioning = true;
                                RefreshRecents();
                            }
                        }
                    }
                    showOpenBrowser = false;
                }
                if (!ImGui::IsPopupOpen("Open Project")) showOpenBrowser = false;

                // --- Loading Transition Overlay ---
                if (isTransitioning)
                {
                    transitionTimer += GetFrameTime();
                    float alpha = std::clamp(transitionTimer / transitionDuration, 0.0f, 1.0f);

                    ImDrawList* drawList = ImGui::GetForegroundDrawList();
                    drawList->AddRectFilled(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), ImGui::GetColorU32(ImVec4(0, 0, 0, alpha)));

                    // Loading Text
                    const char* loadingText = "Loading Project...";
                    ImVec2 textSize = ImGui::CalcTextSize(loadingText);
                    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.5f, ImVec2(viewport->Pos.x + (viewport->Size.x - textSize.x) / 2, viewport->Pos.y + (viewport->Size.y - textSize.y) / 2), ImColor(255, 255, 255, (int)(alpha * 255)), loadingText);

                    if (transitionTimer >= transitionDuration)
                    {
                        projectLoaded = true;
                        isTransitioning = false;
                        transitionTimer = 0.0f;
                    }
                }

                ImGui::EndChild(); // MainContent
                ImGui::End(); // Indium Hub

                ImGui::PopStyleColor(4);

                return projectLoaded;
            }
    };
}
