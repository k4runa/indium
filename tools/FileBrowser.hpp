#pragma once

#include "raylib.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

namespace Indium
{
    /**
     * @brief A modern ImGui-based file browser with sidebar bookmarks,
     *        address bar with autocomplete, and file type filtering.
     */
    class FileBrowser
    {
    private:
        /**
         * @brief Internal persistent state for the file browser.
         *
         * Using a static struct keeps state between frames without
         * requiring external storage, while keeping the API simple.
         */
        struct State
        {
            fs::path        currentPath;
            std::string     selectedFile;
            char            addressBar[512]     = {};
            char            searchFilter[128]   = {};
            bool            initialized         = false;
            bool            addressBarFocused   = false;
            bool            showSuggestions     = false;

            // Sidebar bookmarks
            std::vector<std::pair<std::string, fs::path>> bookmarks;

            // Autocomplete suggestions
            std::vector<std::string> suggestions;

            // Cached directory entries for performance
            std::vector<fs::directory_entry> cachedEntries;
            fs::path cachedPath;

            void Init()
            {
                if (initialized) return;
                initialized = true;

                currentPath = fs::current_path();
                SyncAddressBar();

                // Default bookmarks
                const char* home = getenv("HOME");
                if (home)
                {
                    fs::path homePath(home);
                    bookmarks.push_back({"Home",      homePath});
                    bookmarks.push_back({"Desktop",   homePath / "Desktop"});
                    bookmarks.push_back({"Documents", homePath / "Documents"});
                    bookmarks.push_back({"Pictures",  homePath / "Pictures"});
                    bookmarks.push_back({"Downloads", homePath / "Downloads"});
                    bookmarks.push_back({"Project",   fs::current_path()});
                }
            }

            void SyncAddressBar()
            {
                std::string pathStr = currentPath.string();
                strncpy(addressBar, pathStr.c_str(), sizeof(addressBar) - 1);
                addressBar[sizeof(addressBar) - 1] = '\0';
            }

            void NavigateTo(const fs::path& path)
            {
                if (fs::is_directory(path))
                {
                    currentPath = fs::canonical(path);
                    selectedFile.clear();
                    searchFilter[0] = '\0';
                    SyncAddressBar();
                    RefreshCache();
                }
            }

            void RefreshCache()
            {
                cachedEntries.clear();
                cachedPath = currentPath;
                try
                {
                    for (const auto& entry : fs::directory_iterator(currentPath))
                    {
                        cachedEntries.push_back(entry);
                    }
                    // Sort: directories first, then files, alphabetically
                    std::sort(cachedEntries.begin(), cachedEntries.end(),
                        [](const fs::directory_entry& a, const fs::directory_entry& b)
                        {
                            if (a.is_directory() != b.is_directory())
                                return a.is_directory() > b.is_directory();
                            return a.path().filename().string() < b.path().filename().string();
                        });
                }
                catch (...) {}
            }

            void UpdateSuggestions()
            {
                suggestions.clear();
                std::string input(addressBar);
                if (input.empty()) return;

                fs::path inputPath(input);
                fs::path parentDir;
                std::string prefix;

                if (input.back() == '/')
                {
                    parentDir = inputPath;
                    prefix = "";
                }
                else
                {
                    parentDir = inputPath.parent_path();
                    prefix = inputPath.filename().string();
                }

                if (!fs::is_directory(parentDir)) return;

                try
                {
                    std::string prefixLower = prefix;
                    std::transform(prefixLower.begin(), prefixLower.end(), prefixLower.begin(), ::tolower);

                    for (const auto& entry : fs::directory_iterator(parentDir))
                    {
                        std::string name = entry.path().filename().string();
                        std::string nameLower = name;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

                        if (prefix.empty() || nameLower.find(prefixLower) == 0)
                        {
                            std::string fullPath = entry.path().string();
                            if (entry.is_directory()) fullPath += "/";
                            suggestions.push_back(fullPath);
                        }

                        if (suggestions.size() >= 8) break;
                    }
                }
                catch (...) {}
            }
        };

        static State& GetState()
        {
            static State state;
            return state;
        }

        /**
         * @brief Formats a byte count into a human-readable string (KB, MB, etc.).
         */
        static std::string FormatSize(uintmax_t bytes)
        {
            if (bytes < 1024)              return std::to_string(bytes) + " B";
            if (bytes < 1024 * 1024)       return std::to_string(bytes / 1024) + " KB";
            if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
            return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
        }

        /**
         * @brief Draws the left sidebar containing bookmarks and quick-access shortcuts.
         */
        static void DrawSidebar(State& s)
        {
            ImGui::BeginChild("##sidebar", ImVec2(160, -ImGui::GetFrameHeightWithSpacing()), true);
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Quick Access");
            ImGui::Separator();

            int bookmarkToRemove = -1;
            for (int i = 0; i < (int)s.bookmarks.size(); i++)
            {
                auto& [label, path] = s.bookmarks[i];
                if (!fs::exists(path)) continue;

                bool isActive = (s.currentPath == path);
                if (ImGui::Selectable(label.c_str(), isActive))
                {
                    s.NavigateTo(path);
                }

                // Right-click to remove custom bookmarks (keep first 6 defaults)
                if (i >= 6 && ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Remove Bookmark"))
                    {
                        bookmarkToRemove = i;
                    }
                    ImGui::EndPopup();
                }
            }

            if (bookmarkToRemove >= 0)
            {
                s.bookmarks.erase(s.bookmarks.begin() + bookmarkToRemove);
            }

            ImGui::Separator();

            // Add current directory as bookmark
            if (ImGui::Button("+ Bookmark", ImVec2(-1, 0)))
            {
                std::string name = s.currentPath.filename().string();
                if (name.empty()) name = s.currentPath.string();

                // Don't add duplicates
                bool exists = false;
                for (const auto& [l, p] : s.bookmarks)
                {
                    if (p == s.currentPath) { exists = true; break; }
                }
                if (!exists)
                {
                    s.bookmarks.push_back({name, s.currentPath});
                }
            }

            ImGui::EndChild();
        }

        /**
         * @brief Draws the address bar with autocomplete suggestion dropdown.
         */
        static void DrawAddressBar(State& s)
        {
            ImGui::SetNextItemWidth(-1);
            bool edited = ImGui::InputText("##address", s.addressBar, sizeof(s.addressBar),
                ImGuiInputTextFlags_EnterReturnsTrue);

            // Track focus for showing suggestions
            s.addressBarFocused = ImGui::IsItemActive();

            if (edited)
            {
                // User pressed Enter — navigate to typed path
                fs::path newPath(s.addressBar);
                if (fs::is_directory(newPath))
                {
                    s.NavigateTo(newPath);
                }
                s.showSuggestions = false;
            }
            else if (s.addressBarFocused && ImGui::IsItemEdited())
            {
                // User is typing — update suggestions
                s.UpdateSuggestions();
                s.showSuggestions = !s.suggestions.empty();
            }

            // Draw autocomplete dropdown
            if (s.showSuggestions && !s.suggestions.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 0.95f));
                ImGui::BeginTooltip();

                for (const auto& suggestion : s.suggestions)
                {
                    if (ImGui::Selectable(suggestion.c_str()))
                    {
                        strncpy(s.addressBar, suggestion.c_str(), sizeof(s.addressBar) - 1);
                        s.addressBar[sizeof(s.addressBar) - 1] = '\0';

                        fs::path selected(suggestion);
                        if (fs::is_directory(selected))
                        {
                            s.NavigateTo(selected);
                        }
                        s.showSuggestions = false;
                    }
                }

                ImGui::EndTooltip();
                ImGui::PopStyleColor();
            }
        }

        /**
         * @brief Draws the main file listing area with directory entries.
         */
        static void DrawFileList(State& s, const std::vector<std::string>& extensions)
        {
            // Navigation: Go up one level
            if (s.currentPath.has_parent_path() && s.currentPath.parent_path() != s.currentPath)
            {
                if (ImGui::Selectable("..  (Parent Directory)", false))
                {
                    s.NavigateTo(s.currentPath.parent_path());
                }
            }

            ImGui::Separator();

            // Refresh cache if needed
            if (s.cachedPath != s.currentPath)
            {
                s.RefreshCache();
            }

            // Search filter
            std::string filterStr(s.searchFilter);
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

            for (const auto& entry : s.cachedEntries)
            {
                const auto& path = entry.path();
                std::string filename = path.filename().string();

                // Skip hidden files
                if (!filename.empty() && filename[0] == '.') continue;

                // Apply search filter
                if (!filterStr.empty())
                {
                    std::string nameLower = filename;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (nameLower.find(filterStr) == std::string::npos) continue;
                }

                if (entry.is_directory())
                {
                    // Directory entry with folder icon
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
                    std::string dirLabel = "[D]  " + filename;
                    if (ImGui::Selectable(dirLabel.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            s.NavigateTo(path);
                        }
                    }

                    // Right-click to bookmark a folder
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Add to Bookmarks"))
                        {
                            bool exists = false;
                            for (const auto& [l, p] : s.bookmarks)
                            {
                                if (p == path) { exists = true; break; }
                            }
                            if (!exists)
                            {
                                s.bookmarks.push_back({filename, path});
                            }
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::PopStyleColor();
                }
                else
                {
                    // File extension filter
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

                    // File entry with size info
                    std::string sizeStr;
                    try { sizeStr = FormatSize(entry.file_size()); }
                    catch (...) { sizeStr = "???"; }

                    bool isSelected = (s.selectedFile == filename);
                    std::string fileLabel = "[F]  " + filename;

                    if (ImGui::Selectable(fileLabel.c_str(), isSelected))
                    {
                        s.selectedFile = filename;
                    }

                    // Show file size on the same line, right-aligned
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
                    ImGui::TextDisabled("%s", sizeStr.c_str());
                }
            }
        }

    public:
        /**
         * @brief Draws the file browser modal and returns true when a file is selected.
         *
         * @param title     The modal popup title (must match ImGui::OpenPopup call).
         * @param outPath   Output parameter: the full path of the selected file.
         * @param extensions Optional filter list (e.g., {".png", ".jpg"}).
         * @return true if a file was selected and confirmed.
         */
        static bool Draw(const std::string& title, std::string& outPath,
                         const std::vector<std::string>& extensions = {},
                         bool selectDirectory = false)

        {
            State& s = GetState();
            s.Init();

            bool result = false;

            // Size the browser proportionally to the application window
            float screenW = (float)GetScreenWidth();
            float screenH = (float)GetScreenHeight();
            ImGui::SetNextWindowSize(ImVec2(screenW * 0.55f, screenH * 0.65f), ImGuiCond_FirstUseEver);
            if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoScrollbar))
            {
                // === Address Bar ===
                DrawAddressBar(s);

                ImGui::Spacing();

                // === Main Content: Sidebar + File List (side by side) ===
                // We compute an explicit height for both children so ImGui
                // can lay them out horizontally via SameLine().
                float bottomBarHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
                float contentHeight   = ImGui::GetContentRegionAvail().y - bottomBarHeight;

                // Sidebar (left)
                ImGui::BeginChild("##sidebar", ImVec2(160, contentHeight), true);
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Quick Access");
                ImGui::Separator();

                int bookmarkToRemove = -1;
                for (int i = 0; i < (int)s.bookmarks.size(); i++)
                {
                    auto& [label, path] = s.bookmarks[i];
                    if (!fs::exists(path)) continue;

                    bool isActive = (s.currentPath == path);
                    if (ImGui::Selectable(label.c_str(), isActive))
                    {
                        s.NavigateTo(path);
                    }

                    if (i >= 6 && ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Remove Bookmark"))
                            bookmarkToRemove = i;
                        ImGui::EndPopup();
                    }
                }

                if (bookmarkToRemove >= 0)
                    s.bookmarks.erase(s.bookmarks.begin() + bookmarkToRemove);

                ImGui::Separator();
                if (ImGui::Button("+ Bookmark", ImVec2(-1, 0)))
                {
                    std::string bname = s.currentPath.filename().string();
                    if (bname.empty()) bname = s.currentPath.string();
                    bool exists = false;
                    for (const auto& [l, p] : s.bookmarks)
                        if (p == s.currentPath) { exists = true; break; }
                    if (!exists)
                        s.bookmarks.push_back({bname, s.currentPath});
                }
                ImGui::EndChild();

                // File List (right, same line)
                ImGui::SameLine();
                ImGui::BeginChild("##filelist", ImVec2(0, contentHeight), true);

                try
                {
                    DrawFileList(s, extensions);
                }
                catch (...)
                {
                    ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Cannot access this directory.");
                }

                ImGui::EndChild();

                // === Bottom Bar: Search + Selected file + Buttons ===
                // Search filter
                ImGui::SetNextItemWidth(150);
                ImGui::InputTextWithHint("##search", "Search...", s.searchFilter, sizeof(s.searchFilter));
                ImGui::SameLine();

                // Selected file display
                if (selectDirectory)
                {
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Folder: %s", s.currentPath.filename().string().c_str());
                }
                else if (!s.selectedFile.empty())
                {
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", s.selectedFile.c_str());
                }
                else
                {
                    ImGui::TextDisabled("No file selected");
                }

                // Buttons (right-aligned)
                ImGui::SameLine(ImGui::GetWindowWidth() - 200);
                if (ImGui::Button("Cancel", ImVec2(90, 0)))
                {
                    s.showSuggestions = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                bool canSelect = selectDirectory || !s.selectedFile.empty();
                if (!canSelect) ImGui::BeginDisabled();
                if (ImGui::Button("Select", ImVec2(90, 0)))
                {
                    if (selectDirectory)
                    {
                        outPath = s.currentPath.string();
                    }
                    else
                    {
                        outPath = (s.currentPath / s.selectedFile).string();
                    }
                    result = true;
                    s.showSuggestions = false;
                    ImGui::CloseCurrentPopup();
                }
                if (!canSelect) ImGui::EndDisabled();

                ImGui::EndPopup();
            }

            return result;
        }
    };
}
