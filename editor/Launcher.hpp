#pragma once
#include "imgui.h"
#include "../core/ProjectManager.hpp"
#include "../tools/FileBrowser.hpp"
#include "panels/BusyOverlay.hpp"
#include "raylib.h"
#include <string>
#include <map>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#if !defined(_WIN32)
#include <unistd.h>
#include <sys/wait.h>
#endif

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

            // Async project creation
            std::future<bool> createFuture;
            std::atomic<int>  createStep{0}; // 1=structure, 2=compiling, 3=done
            float             createSpin      = 0.0f;
            std::string       createProjName;
            std::string       createErrDetail;
            bool              createFailed    = false;
            float             createErrTimer  = 0.0f;

            // Disk delete confirmation
            std::string       pendingDiskDeletePath;
            bool              wantOpenDeleteModal_ = false; // deferred OpenPopup flag

            void RefreshRecents()
            {
                recentProjects = pm->GetRecentProjects();

                // Remove entries whose folder no longer exists on disk so the list
                // stays accurate without the user having to manually clean it up.
                recentProjects.erase(
                    std::remove_if(recentProjects.begin(), recentProjects.end(),
                        [](const RecentProject& rp)
                        {
                            std::error_code ec;
                            return !fs::exists(fs::path(rp.path) / "project.indp", ec);
                        }),
                    recentProjects.end());
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

                // Minimalist dark theme for the launcher. The launcher is always a dark
                // hub regardless of the editor's active theme, so it must push its OWN
                // full palette — text, frames, buttons, headers — not just backgrounds.
                // Otherwise a Light editor theme leaves dark text on the dark hub bg,
                // making titles/buttons invisible. Keep LAUNCHER_STYLE_COLORS in sync
                // with the PopStyleColor(...) count at the end of Draw().
                ImGui::PushStyleColor(ImGuiCol_WindowBg,         ImVec4(0.039f, 0.039f, 0.039f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ChildBg,         ImVec4(0.039f, 0.039f, 0.039f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_PopupBg,         ImVec4(0.051f, 0.051f, 0.051f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg,ImVec4(0.0f,   0.0f,   0.0f,   0.3f));
                ImGui::PushStyleColor(ImGuiCol_Text,            ImVec4(0.90f,  0.90f,  0.90f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_TextDisabled,    ImVec4(0.45f,  0.45f,  0.45f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBg,         ImVec4(0.10f,  0.10f,  0.11f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImVec4(0.14f,  0.14f,  0.15f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive,   ImVec4(0.16f,  0.16f,  0.18f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_Button,          ImVec4(0.12f,  0.12f,  0.13f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,   ImVec4(0.18f,  0.18f,  0.19f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,    ImVec4(0.22f,  0.22f,  0.24f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border,          ImVec4(0.15f,  0.15f,  0.15f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_Separator,       ImVec4(0.18f,  0.18f,  0.18f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_Header,          ImVec4(0.16f,  0.16f,  0.18f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,   ImVec4(0.22f,  0.22f,  0.25f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,    ImVec4(0.26f,  0.26f,  0.30f,  1.0f));
                ImGui::PushStyleColor(ImGuiCol_CheckMark,       ImVec4(0.90f,  0.90f,  0.90f,  1.0f));

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

                    // "Open" is a ghost button: transparent at rest, fading to a
                    // visible subtle fill on hover. The hover colour needs a non-zero
                    // alpha or the hover state is invisible (both were 0.0 before).
                    ImVec4 btnBase = ImVec4(0.30f, 0.30f, 0.30f, 0.0f);
                    ImVec4 btnHov  = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
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
                        wantOpenDeleteModal_ = false; // reset each frame

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

                            // Date (or "On disk" badge) on the far right
                            bool onDiskOnly = rp.lastOpened.empty();
                            const char* dateText = onDiskOnly ? "On disk" : rp.lastOpened.c_str();
                            ImU32 dateCol = onDiskOnly ? ImColor(100, 150, 100, 200) : ImColor(80, 80, 80, 255);
                            float dateWidth = ImGui::CalcTextSize(dateText).x * (onDiskOnly ? 1.0f : 0.8f);
                            drawList->AddText(ImGui::GetFont(), onDiskOnly ? ImGui::GetFontSize() : ImGui::GetFontSize() * 0.8f,
                                              ImVec2(p1.x - dateWidth - 25, p0.y + 35), dateCol, dateText);

                            if (clicked)
                            {
                                try
                                {
                                    if (pm->LoadProject(rp.path, *scene)) projectLoaded = true;
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
                                if (ImGui::Selectable(ICON_FA_XMARK "  Remove from list"))
                                    toRemove = rp.path;
                                if (ImGui::Selectable(ICON_FA_TRASH "  Delete project folder..."))
                                {
                                    // Do NOT call OpenPopup here — we are inside a popup and
                                    // Selectable auto-closes it, which would kill the child
                                    // popup immediately. Defer to after EndPopup instead.
                                    pendingDiskDeletePath = rp.path;
                                    wantOpenDeleteModal_  = true;
                                }
                                if (ImGui::Selectable(ICON_FA_FOLDER_OPEN "  Reveal in Explorer"))
                                {
#if defined(_WIN32)
                                    std::string cmd = "start \"\" \"" + rp.path + "\"";
                                    system(cmd.c_str());
#else
                                    const char* tool =
#if defined(__APPLE__)
                                        "open";
#else
                                        "xdg-open";
#endif
                                    pid_t pid = fork();
                                    if (pid == 0)
                                    {
                                        execlp(tool, tool, rp.path.c_str(), nullptr);
                                        _exit(1);
                                    }
                                    else if (pid > 0) waitpid(pid, nullptr, WNOHANG);
#endif
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

                        // Open the delete-confirm modal here (outside any popup scope)
                        // so ImGui can properly display it without the parent context
                        // menu immediately destroying it.
                        if (wantOpenDeleteModal_)
                        {
                            ImGui::OpenPopup("Confirm Delete Project");
                            wantOpenDeleteModal_ = false;
                        }

                        // Disk delete confirmation modal
                        ImGui::SetNextWindowSize(ImVec2(-1, 0), ImGuiCond_Appearing);
                        if (ImGui::BeginPopupModal("Confirm Delete Project", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                        {
                            ImGui::Text("This will permanently delete the project folder\nand all its contents:\n\n%s\n\nThis cannot be undone.", pendingDiskDeletePath.c_str());
                            ImGui::Dummy(ImVec2(0, 10));
                            ImGui::Separator();
                            ImGui::Dummy(ImVec2(0, 10));

                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
                            if (ImGui::Button("Delete Forever", ImVec2(140, 0)))
                            {
                                try { fs::remove_all(pendingDiskDeletePath); } catch (...) {}
                                pm->RemoveRecentProject(pendingDiskDeletePath);
                                pendingDiskDeletePath.clear();
                                RefreshRecents();
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::PopStyleColor(2);

                            ImGui::SameLine();
                            if (ImGui::Button("Cancel", ImVec2(100, 0)))
                            {
                                pendingDiskDeletePath.clear();
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
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
                    if (ImGui::InputText("##defpath", defaultProjPath, sizeof(defaultProjPath))) { selectedLocation = std::string(defaultProjPath); }
                    ImGui::PopItemWidth();
                    ImGui::PopStyleVar();

                    ImGui::SameLine();
                    ImVec4 btnBase = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
                    ImVec4 btnHov  = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);
                    if (AnimatedButton("Save", ImVec2(100, 30), btnBase, btnHov)) { pm->SetDefaultProjectPath(selectedLocation); }
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

                    // Inline validation
                    auto nameHasInvalidChars = [](const char* s)
                    {
                        std::string n(s);
                        if (n.find('/') != std::string::npos)  return true;
                        if (n.find('\\') != std::string::npos) return true;
                        if (n.find("..") != std::string::npos) return true;
                        return false;
                    };
                    bool projNameEmpty    = (newProjName[0] == '\0' || std::string(newProjName).find_first_not_of(" \t") == std::string::npos);
                    bool projNameInvalid  = !projNameEmpty && nameHasInvalidChars(newProjName);
                    if (projNameEmpty)        ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION "  Name cannot be empty or whitespace-only.");
                    else if (projNameInvalid) ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION "  Name cannot contain  /  \\  or  ..");

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
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN, ImVec2(45, 36))) { showLocationBrowser = true; }
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
                    if (projNameEmpty || projNameInvalid || selectedLocation.empty()) ImGui::BeginDisabled();
                    ImVec4 accentBase = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
                    ImVec4 accentHov  = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
                    if (AnimatedButton("Create", ImVec2(120, 40), accentBase, accentHov))
                    {
                        std::string loc      = selectedLocation;
                        std::string projName = std::string(newProjName);
                        std::string projPath = fullPath;
                        createProjName  = projName;
                        createFailed    = false;
                        createErrTimer  = 0.0f;
                        createStep      = 1;
                        ImGui::CloseCurrentPopup();

                        try
                        {
                            createFuture = std::async(std::launch::async, [this, loc, projName, projPath, scene]() -> bool {
                                // Check before trying so we can give a precise error
                                if (fs::exists(fs::path(loc) / projName))
                                {
                                    createErrDetail = "A folder named \"" + projName + "\" already exists at this location.";
                                    createStep = 0;
                                    return false;
                                }
                                if (!pm->CreateProject(loc, projName))
                                {
                                    createErrDetail = "Could not create project folder. Check permissions.";
                                    createStep = 0;
                                    return false;
                                }
                                createErrDetail.clear();
                                createStep = 2;
                                if (!pm->LoadProject(projPath, *scene))
                                {
                                    createErrDetail = "Project created but failed to load.";
                                    createStep = 0;
                                    return false;
                                }
                                createStep = 3;
                                return true;
                            });
                        }
                        catch (...)
                        {
                            createErrDetail = "Failed to start background thread.";
                            createFailed    = true;
                            createStep      = 0;
                        }
                    }
                    if (projNameEmpty || projNameInvalid || selectedLocation.empty()) ImGui::EndDisabled();
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
                                projectLoaded = true;
                                RefreshRecents();
                            }
                        }
                    }
                    showOpenBrowser = false;
                }
                if (!ImGui::IsPopupOpen("Open Project")) showOpenBrowser = false;

                ImGui::EndChild(); // MainContent
                ImGui::End(); // Indium Hub
                ImGui::PopStyleColor(18); // matches the launcher palette pushed at the top of Draw()

                // --- Async creation overlay ---
                // Primary trigger is createFuture.valid() so there is no race with createStep.
                // createFailed keeps the error card alive after the future is consumed.
                if (createFuture.valid() || createFailed)
                {
                    if (!createFailed)
                    {
                        // Success path: reuse the shared busy overlay so the launcher
                        // and the editor show identical, theme-matched feedback.
                        static const char* kStepLabel[] = { "Setting up project...", "Setting up project...", "Compiling scripts...", "Finishing up..." };
                        int step = createStep.load();
                        const char* label = (step >= 1 && step <= 3) ? kStepLabel[step] : kStepLabel[0];
                        BusyOverlay::Draw(createProjName.c_str(), label);
                    }
                    else
                    {
                        // Error state — auto-dismisses after 3 seconds. Neutral-gray
                        // card to match the theme; only the error line is tinted.
                        ImGuiViewport* vp = ImGui::GetMainViewport();
                        ImDrawList*    dl = ImGui::GetForegroundDrawList();
                        dl->AddRectFilled(vp->Pos,
                                          {vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y},
                                          IM_COL32(0, 0, 0, 185));
                        const float cw = 380.0f, ch = 180.0f;
                        ImVec2 ca = {vp->Pos.x + vp->Size.x * 0.5f - cw * 0.5f,
                                     vp->Pos.y + vp->Size.y * 0.5f - ch * 0.5f};
                        ImVec2 cb = {ca.x + cw, ca.y + ch};
                        dl->AddRectFilled(ca, cb, IM_COL32(28, 28, 28, 255), 12.0f);
                        dl->AddRect      (ca, cb, IM_COL32(70, 70, 70, 255), 12.0f, 0, 1.5f);

                        const char* errLine1 = "Project creation failed.";
                        ImVec2 e1 = ImGui::CalcTextSize(errLine1);
                        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                                    {ca.x + cw * 0.5f - e1.x * 0.5f, ca.y + 50.0f},
                                    IM_COL32(220, 80, 80, 255), errLine1);

                        const std::string& detail = createErrDetail;
                        if (!detail.empty())
                        {
                            ImVec2 e2 = ImGui::CalcTextSize(detail.c_str());
                            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                                        {ca.x + cw * 0.5f - e2.x * 0.5f, ca.y + 82.0f},
                                        IM_COL32(200, 165, 80, 255), detail.c_str());
                        }

                        const char* errLine3 = "Dismissing in a moment...";
                        ImVec2 e3 = ImGui::CalcTextSize(errLine3);
                        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f,
                                    {ca.x + cw * 0.5f - e3.x * 0.425f, ca.y + 120.0f},
                                    IM_COL32(100, 100, 105, 200), errLine3);

                        createErrTimer += GetFrameTime();
                        if (createErrTimer > 3.0f) { createFailed = false; createErrTimer = 0.0f; }
                    }

                    // Non-blocking poll — check if background thread finished
                    if (createFuture.valid() &&
                        createFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                    {
                        bool ok = false;
                        try { ok = createFuture.get(); } catch (...) {}
                        createStep = 0;
                        if (ok)  { RefreshRecents(); projectLoaded = true; }
                        else     { createFailed = true; createErrTimer = 0.0f; }
                    }
                }

                return projectLoaded;
            }
    };
}
