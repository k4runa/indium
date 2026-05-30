#include "../Editor.hpp"

namespace Indium
{
    void Editor::ApplyDarkTheme(ImVec4* colors)
    {
        /**
         * @brief A modern dark theme for indium engine
         */

        // Define dark theme background and border colors for core ImGui UI elements.
        colors[ImGuiCol_MenuBarBg]              = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_WindowBg]               = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_ChildBg]                = ImVec4(24 / 255.0f, 24 / 255.0f, 24 / 255.0f, 1.0f);
        colors[ImGuiCol_PopupBg]                = ImVec4(24 / 255.0f, 24 / 255.0f, 24 / 255.0f, 1.0f);
        colors[ImGuiCol_Border]                 = ImVec4(40 / 255.0f, 40 / 255.0f, 40 / 255.0f, 1.0f);

        // Title bar background colors for ImGui windows (normal, active, and collapsed states).
        colors[ImGuiCol_TitleBg]                = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 1.0f);

        // Header background colors for ImGui interactive elements (tree nodes, collapsible headers, selectable sections).
        colors[ImGuiCol_Header]                 = ImVec4(30 / 255.0f, 30 / 255.0f, 30 / 255.0f, 1.0f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(45 / 255.0f, 45 / 255.0f, 45 / 255.0f, 1.0f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(55 / 255.0f, 55 / 255.0f, 55 / 255.0f, 1.0f);

        // Button color states for interactive controls (normal, hovered, active/pressed).
        colors[ImGuiCol_Button]                 = ImVec4(35 / 255.0f, 35 / 255.0f, 35 / 255.0f, 1.0f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(50 / 255.0f, 50 / 255.0f, 50 / 255.0f, 1.0f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(65 / 255.0f, 65 / 255.0f, 65 / 255.0f, 1.0f);

        // Input field (frame) background colors for editable widgets such as text inputs, sliders, and numeric fields.
        colors[ImGuiCol_FrameBg]                = ImVec4(25 / 255.0f, 25 / 255.0f, 25 / 255.0f, 1.0f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(35 / 255.0f, 35 / 255.0f, 35 / 255.0f, 1.0f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(45 / 255.0f, 45 / 255.0f, 45 / 255.0f, 1.0f);

        // Slider grab handle and checkbox checkmark colors for interactive controls.
        colors[ImGuiCol_SliderGrab]             = ImVec4(255 / 255.0f, 255 / 255.0f, 255 / 255.0f, 1.0f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(220 / 255.0f, 220 / 255.0f, 220 / 255.0f, 1.0f);
        colors[ImGuiCol_CheckMark]              = ImVec4(255 / 255.0f, 255 / 255.0f, 255 / 255.0f, 1.0f);

        // Scrollbar styling for ImGui (background, grab handle, and hover state).
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(18 / 255.0f, 18 / 255.0f, 18 / 255.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(40 / 255.0f, 40 / 255.0f, 40 / 255.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(60 / 255.0f, 60 / 255.0f, 60 / 255.0f, 1.0f);

        // Tab colors - Matching the stealth theme
        colors[ImGuiCol_Tab]                    = ImVec4(20 / 255.0f, 20 / 255.0f, 20 / 255.0f, 1.0f);
        colors[ImGuiCol_TabHovered]             = ImVec4(45 / 255.0f, 45 / 255.0f, 45 / 255.0f, 1.0f);
        colors[ImGuiCol_TabActive]              = ImVec4(35 / 255.0f, 35 / 255.0f, 35 / 255.0f, 1.0f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(20 / 255.0f, 20 / 255.0f, 20 / 255.0f, 1.0f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(30 / 255.0f, 30 / 255.0f, 30 / 255.0f, 1.0f);

        // Text color settings for ImGui UI elements (normal and disabled states).
        colors[ImGuiCol_Text]                   = ImVec4(230 / 255.0f, 230 / 255.0f, 230 / 255.0f, 1.0f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(100 / 255.0f, 100 / 255.0f, 100 / 255.0f, 1.0f);
    }

    void Editor::ApplyLightTheme(ImVec4* colors)
    {
        /**
         * @brief A modern light theme for indium engine
        */

        // Define light theme background and border colors for various ImGui UI elements
        colors[ImGuiCol_MenuBarBg]              = ImVec4(235 / 255.0f, 235 / 255.0f, 235 / 255.0f, 1.0f);
        colors[ImGuiCol_WindowBg]               = ImVec4(245 / 255.0f, 245 / 255.0f, 245 / 255.0f, 1.0f);
        colors[ImGuiCol_ChildBg]                = ImVec4(250 / 255.0f, 250 / 255.0f, 250 / 255.0f, 1.0f);
        colors[ImGuiCol_PopupBg]                = ImVec4(255 / 255.0f, 255 / 255.0f, 255 / 255.0f, 1.0f);
        colors[ImGuiCol_Border]                 = ImVec4(190 / 255.0f, 190 / 255.0f, 190 / 255.0f, 1.0f);

        // Title bar background colors for ImGui windows (normal, active, and collapsed states).
        colors[ImGuiCol_TitleBg]                = ImVec4(235 / 255.0f, 235 / 255.0f, 235 / 255.0f, 1.0f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(235 / 255.0f, 235 / 255.0f, 235 / 255.0f, 1.0f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(235 / 255.0f, 235 / 255.0f, 235 / 255.0f, 1.0f);

        // Header background colors for ImGui elements (e.g., tree nodes, collapsible headers, and selectable sections).
        colors[ImGuiCol_Header]                 = ImVec4(205 / 255.0f, 205 / 255.0f, 205 / 255.0f, 1.0f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(185 / 255.0f, 185 / 255.0f, 185 / 255.0f, 1.0f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(165 / 255.0f, 165 / 255.0f, 165 / 255.0f, 1.0f);

        // Button color states for ImGui interactive controls (normal, hovered, and active/pressed).
        colors[ImGuiCol_Button]                 = ImVec4(220 / 255.0f, 220 / 255.0f, 220 / 255.0f, 1.0f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(200 / 255.0f, 200 / 255.0f, 200 / 255.0f, 1.0f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(180 / 255.0f, 180 / 255.0f, 180 / 255.0f, 1.0f);

        // Input field (frame) background colors for ImGui editable widgets such as text inputs, sliders, and numeric fields.
        colors[ImGuiCol_FrameBg]                = ImVec4(255 / 255.0f, 255 / 255.0f, 255 / 255.0f, 1.0f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(235 / 255.0f, 235 / 255.0f, 235 / 255.0f, 1.0f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(220 / 255.0f, 220 / 255.0f, 220 / 255.0f, 1.0f);

        // Slider grab handle and checkbox checkmark colors for ImGui interactive controls.
        colors[ImGuiCol_SliderGrab]             = ImVec4(90 / 255.0f, 90 / 255.0f, 90 / 255.0f, 1.0f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(60 / 255.0f, 60 / 255.0f, 60 / 255.0f, 1.0f);
        colors[ImGuiCol_CheckMark]              = ImVec4(40 / 255.0f, 40 / 255.0f, 40 / 255.0f, 1.0f);

        // Scrollbar styling for ImGui (background, grab handle, and hover state).
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(230 / 255.0f, 230 / 255.0f, 230 / 255.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(180 / 255.0f, 180 / 255.0f, 180 / 255.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(150 / 255.0f, 150 / 255.0f, 150 / 255.0f, 1.0f);

        // Tab colors for light theme.
        colors[ImGuiCol_Tab]                    = ImVec4(210 / 255.0f, 210 / 255.0f, 210 / 255.0f, 1.0f);
        colors[ImGuiCol_TabHovered]             = ImVec4(190 / 255.0f, 190 / 255.0f, 190 / 255.0f, 1.0f);
        colors[ImGuiCol_TabActive]              = ImVec4(230 / 255.0f, 230 / 255.0f, 230 / 255.0f, 1.0f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(215 / 255.0f, 215 / 255.0f, 215 / 255.0f, 1.0f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(225 / 255.0f, 225 / 255.0f, 225 / 255.0f, 1.0f);

        // Text color settings for ImGui UI elements (normal and disabled states).
        colors[ImGuiCol_Text]                   = ImVec4(25 / 255.0f, 25 / 255.0f, 25 / 255.0f, 1.0f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(120 / 255.0f, 120 / 255.0f, 120 / 255.0f, 1.0f);
    }

    void Editor::ApplyTheme(std::string theme)
    {
        this->THEME_STYLE = theme;

        // Persist so the choice survives restarts
        {
            nlohmann::json prefs;
            std::ifstream inPf(PrefsPath());
            if (inPf.is_open()) { try { prefs = nlohmann::json::parse(inPf); } catch (...) {} }
            prefs["theme"] = theme;
            std::ofstream outPf(PrefsPath());
            if (outPf.is_open()) outPf << prefs.dump(4);
        }

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // --- Global Visual Styling ---
        style.WindowRounding    = 0.0f;          // Sharp windows for industrial look
        style.ChildRounding     = 0.0f;          // Sharp child panels
        style.FrameRounding     = 5.0f;          // Rounded buttons and widgets
        style.PopupRounding     = 5.0f;          // Rounded popups
        style.ScrollbarRounding = 12.0f;
        style.GrabRounding      = 5.0f;
        style.TabRounding       = 0.0f;          // Sharp tabs to match windows

        style.WindowBorderSize  = 1.5f;
        style.FrameBorderSize   = 1.5f;
        style.PopupBorderSize   = 1.5f;

        style.WindowPadding     = ImVec2(12.0f, 10.0f);
        style.FramePadding      = ImVec2(8.0f, 5.0f);
        style.ItemSpacing       = ImVec2(8.0f, 7.0f);
        style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
        style.GrabMinSize       = 15.0f;

        // Theme selection logic
        // Based on the provided THEME_STYLE string, the corresponding color palette
        // is applied. This allows dynamic switching between dark and light modes.
        theme == "dark" ? ApplyDarkTheme(colors) : ApplyLightTheme(colors);
    }
}
