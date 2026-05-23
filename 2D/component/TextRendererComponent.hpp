#pragma once
#include <string>
#include <cstring>
#include <filesystem>
#include <vector>
#include "../../core/Component.hpp"
#include "../../core/AssetManager.hpp"
#include "../../core/ScriptManager.hpp"
#include "raylib.h"
#include "raymath.h"
#include "imgui.h"

namespace Indium
{
    /**
     * @brief Renders a text string in world space at the owner entity's position.
     *
     * Supports custom TTF/OTF fonts (loaded from project assets), font size,
     * letter spacing, color, and horizontal alignment (Left / Center / Right).
     *
     * Script usage:
     *   auto* txt = owner->getComponent<TextRendererComponent>();
     *   txt->text = "Score: " + std::to_string(score);
     */
    struct TextRendererComponent : Component
    {
        enum class Align { Left, Center, Right };

        std::string text      = "Text";
        std::string fontPath  = "";
        float       fontSize  = 24.0f;
        float       spacing   = 1.0f;
        Color       color     = WHITE;
        Align       align     = Align::Center;

        // ----------------------------------------------------------------
        // Font loading
        // ----------------------------------------------------------------
        void LoadFont(const std::string& path)
        {
            unloadFont_();
            fontPath = path;
            if (path.empty()) return;

            std::string resolved = resolvePath_(path);
            font_       = AssetManager::Get().GetFont(resolved, (int)fontSize * 2);
            fontLoaded_ = (font_.texture.id > 0);
        }

        // ----------------------------------------------------------------
        // Component interface
        // ----------------------------------------------------------------
        void update(float, Vector2, Scene*) override {}

        void draw() const override
        {
            if (!owner) return;

            Vector2 pos    = owner->getGlobalPosition();
            float   fSize  = fontSize * owner->getGlobalScale().y / 100.0f;
            fSize           = fmaxf(fSize, 1.0f);

            const ::Font& f = fontLoaded_ ? font_ : ::GetFontDefault();

            if (text != cachedMeasureText_ || fSize != cachedMeasureFSize_ || spacing != cachedMeasureSpacing_)
            {
                cachedMeasureText_    = text;
                cachedMeasureFSize_   = fSize;
                cachedMeasureSpacing_ = spacing;
                cachedMeasure_        = ::MeasureTextEx(f, text.c_str(), fSize, spacing);
            }
            Vector2 measured = cachedMeasure_;

            float ox = 0.0f;
            if (align == Align::Center) ox = -measured.x * 0.5f;
            else if (align == Align::Right)  ox = -measured.x;

            Vector2 drawPos = { pos.x + ox, pos.y - measured.y * 0.5f };
            ::DrawTextEx(f, text.c_str(), drawPos, fSize, spacing, color);
        }

        void destroy(Scene*) override { unloadFont_(); }

        void inspect(std::function<void()> snapshotCb) override
        {
            // --- Text content ---
            ImGui::Text("Text");
            static char textBuf[512];
            strncpy(textBuf, text.c_str(), sizeof(textBuf) - 1);
            textBuf[sizeof(textBuf) - 1] = '\0';
            ImGui::PushItemWidth(-1);
            if (ImGui::InputTextMultiline("##TextContent", textBuf, sizeof(textBuf), ImVec2(-1, 60))) {text = textBuf;}
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Spacing();

            // --- Font picker (dropdown from assets) ---
            ImGui::Text("Font");
            std::vector<std::string> fontFiles = scanAssets_({".ttf", ".otf", ".TTF", ".OTF"});

            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##FontSelect", fontPath.empty() ? "(Default font)" : fontPath.c_str()))
            {
                if (ImGui::Selectable("(Default font)", fontPath.empty()))
                {
                    unloadFont_();
                    fontPath = "";
                }
                for (const auto& f : fontFiles)
                {
                    if (ImGui::Selectable(f.c_str(), fontPath == f)) LoadFont(f);
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            // Drag-and-drop target
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    std::string dropped = (const char*)p->Data;
                    std::string ext = std::filesystem::path(dropped).extension().string();
                    if (ext == ".ttf" || ext == ".otf" || ext == ".TTF" || ext == ".OTF") LoadFont(dropped);
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::Spacing();

            // --- Font size / spacing ---
            ImGui::Text("Font Size");
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##TxtSize", &fontSize, 0.5f, 4.0f, 512.0f, "%.1f"))
            {
                if (fontLoaded_ && !fontPath.empty()) LoadFont(fontPath); // reload at new size
            }
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Text("Letter Spacing");
            ImGui::PushItemWidth(-1);
            ImGui::DragFloat("##TxtSpacing", &spacing, 0.1f, -10.0f, 50.0f, "%.1f");
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Spacing();

            // --- Alignment ---
            ImGui::Text("Alignment");
            if (ImGui::RadioButton("Left",   align == Align::Left))   align = Align::Left;
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::SameLine();
            if (ImGui::RadioButton("Center", align == Align::Center)) align = Align::Center;
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::SameLine();
            if (ImGui::RadioButton("Right",  align == Align::Right))  align = Align::Right;
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

            ImGui::Spacing();

            // --- Color ---
            ImGui::Text("Color");
            float c[4] = {
                color.r / 255.0f, color.g / 255.0f,
                color.b / 255.0f, color.a / 255.0f
            };
            ImGui::PushItemWidth(-1);
            if (ImGui::ColorEdit4("##TxtColor", c))
            {
                color.r = (unsigned char)(c[0] * 255.0f);
                color.g = (unsigned char)(c[1] * 255.0f);
                color.b = (unsigned char)(c[2] * 255.0f);
                color.a = (unsigned char)(c[3] * 255.0f);
            }
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Spacing();
            Vector2 m = ::MeasureTextEx(fontLoaded_ ? font_ : ::GetFontDefault(),
                                        text.c_str(), fontSize, spacing);
            ImGui::TextDisabled("Size: %.0f x %.0f px", m.x, m.y);
        }

        std::string getName() const override { return "TextRenderer"; }

        std::unique_ptr<Component> clone() const override
        {
            auto c = std::make_unique<TextRendererComponent>();
            c->text     = text;
            c->fontPath = fontPath;
            c->fontSize = fontSize;
            c->spacing  = spacing;
            c->color    = color;
            c->align    = align;
            if (!fontPath.empty()) c->LoadFont(fontPath);
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["text"]     = text;
            j["fontPath"] = fontPath;
            j["fontSize"] = fontSize;
            j["spacing"]  = spacing;
            j["color"]    = { color.r, color.g, color.b, color.a };
            j["align"]    = static_cast<int>(align);
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("text"))     text     = j["text"].get<std::string>();
            if (j.contains("fontPath")) fontPath = j["fontPath"].get<std::string>();
            if (j.contains("fontSize")) fontSize = j["fontSize"].get<float>();
            if (j.contains("spacing"))  spacing  = j["spacing"].get<float>();
            if (j.contains("color"))
            {
                color.r = j["color"][0].get<unsigned char>();
                color.g = j["color"][1].get<unsigned char>();
                color.b = j["color"][2].get<unsigned char>();
                color.a = j["color"][3].get<unsigned char>();
            }
            if (j.contains("align")) align = static_cast<Align>(j["align"].get<int>());
            if (!fontPath.empty()) LoadFont(fontPath);
        }

        ~TextRendererComponent() override { unloadFont_(); }

    private:
        ::Font font_       = {};
        bool   fontLoaded_ = false;

        mutable std::string cachedMeasureText_    = "";
        mutable float       cachedMeasureFSize_   = -1.0f;
        mutable float       cachedMeasureSpacing_ = -1.0f;
        mutable Vector2     cachedMeasure_        = {0.0f, 0.0f};

        void unloadFont_()
        {
            // Font handle is owned by AssetManager — do not call UnloadFont here.
            font_       = {};
            fontLoaded_ = false;
        }

        std::string resolvePath_(const std::string& path) const
        {
            if (path.empty() || std::filesystem::path(path).is_absolute()) return path;
            std::string proj = ScriptManager::Get().GetActiveProjectPath();
            return proj.empty() ? path : (std::filesystem::path(proj) / path).string();
        }

        std::vector<std::string> scanAssets_(const std::vector<std::string>& exts) const
        {
            std::vector<std::string> result;
            std::string proj = ScriptManager::Get().GetActiveProjectPath();
            if (proj.empty()) return result;

            for (const auto& base : { proj + "/Assets", proj + "/assets" })
            {
                if (!std::filesystem::exists(base)) continue;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(base))
                {
                    if (!entry.is_regular_file()) continue;
                    std::string ext = entry.path().extension().string();
                    for (const auto& e : exts) if (ext == e) { result.push_back(std::filesystem::relative(entry.path(), proj).string()); break; }
                }
            }
            return result;
        }
    };
}
