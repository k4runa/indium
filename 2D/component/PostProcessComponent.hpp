#pragma once
#include "../../core/Component.hpp"
#include "../../include/nlohmann/json.hpp"
#include "imgui.h"

namespace Indium
{
    // --------------------------------------------------------------------
    // PostProcessComponent
    //
    // A full-screen screen-space effect applied to the rendered viewport.
    // This component is a *marker + settings* holder — the actual GPU work
    // is done by the editor's PostProcessManager, which collects every
    // enabled PostProcessComponent in the scene each frame and chains their
    // shaders over the viewport texture (in entity order).
    //
    // Put several on different entities to stack effects (e.g. ColorGrade +
    // Vignette + Scanlines for a CRT look). Typically lives on the Camera.
    // --------------------------------------------------------------------

    // NOTE: keep this enum in sync with PostProcessManager's shader table.
    enum class PostEffect
    {
        Grayscale = 0,
        Invert,
        Vignette,
        ChromaticAberration,
        Scanlines,
        Pixelate,
        ColorGrade,
        COUNT
    };

    struct PostProcessComponent : Component
    {
        PostEffect effect = PostEffect::Vignette;

        // Generic blend amount (0 = off, 1 = full) — used by most effects.
        float intensity = 1.0f;

        // Effect-specific parameters (only the relevant ones are shown/used):
        float vignetteRadius = 0.75f;   // Vignette: 0..1, smaller = tighter
        float aberration     = 3.0f;    // ChromaticAberration: pixel offset
        float scanlineDensity = 1.0f;   // Scanlines: line frequency multiplier
        float pixelSize      = 4.0f;    // Pixelate: block size in pixels
        float brightness     = 0.0f;    // ColorGrade: -1..1 additive
        float contrast       = 1.0f;    // ColorGrade: 0..2 multiplier
        float saturation     = 1.0f;    // ColorGrade: 0..2 (0 = grayscale)

        void update(float, Vector2, Scene*) override {}

        static const char* EffectName(PostEffect e)
        {
            switch (e)
            {
                case PostEffect::Grayscale:           return "Grayscale";
                case PostEffect::Invert:              return "Invert";
                case PostEffect::Vignette:            return "Vignette";
                case PostEffect::ChromaticAberration: return "Chromatic Aberration";
                case PostEffect::Scanlines:           return "Scanlines / CRT";
                case PostEffect::Pixelate:            return "Pixelate";
                case PostEffect::ColorGrade:          return "Color Grade";
                default:                              return "Unknown";
            }
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::TextDisabled("Full-screen effect applied to the viewport.");
            ImGui::Spacing();

            // Effect selector
            ImGui::Text("Effect");
            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##PFXEffect", EffectName(effect)))
            {
                for (int i = 0; i < (int)PostEffect::COUNT; i++)
                {
                    PostEffect e = (PostEffect)i;
                    bool sel = (e == effect);
                    if (ImGui::Selectable(EffectName(e), sel)) { if (snapshotCb) snapshotCb(); effect = e; }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::Spacing();

            auto slider = [&](const char* label, const char* id, float* v, float lo, float hi)
            {
                ImGui::Text("%s", label);
                ImGui::PushItemWidth(-1);
                ImGui::SliderFloat(id, v, lo, hi, "%.2f");
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
                ImGui::PopItemWidth();
            };

            // Common
            if (effect != PostEffect::ColorGrade && effect != PostEffect::Pixelate)
                slider("Intensity", "##PFXIntensity", &intensity, 0.0f, 1.0f);

            switch (effect)
            {
                case PostEffect::Vignette:
                    slider("Radius", "##PFXVigR", &vignetteRadius, 0.1f, 1.5f);
                    break;
                case PostEffect::ChromaticAberration:
                    slider("Offset (px)", "##PFXAberr", &aberration, 0.0f, 20.0f);
                    break;
                case PostEffect::Scanlines:
                    slider("Density", "##PFXScan", &scanlineDensity, 0.25f, 4.0f);
                    break;
                case PostEffect::Pixelate:
                    slider("Pixel Size", "##PFXPix", &pixelSize, 1.0f, 64.0f);
                    break;
                case PostEffect::ColorGrade:
                    slider("Brightness", "##PFXBright", &brightness, -1.0f, 1.0f);
                    slider("Contrast",   "##PFXContr",  &contrast,    0.0f, 2.0f);
                    slider("Saturation", "##PFXSat",    &saturation,  0.0f, 2.0f);
                    break;
                default: break;
            }
        }

        std::string getName() const override { return "PostProcess"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<PostProcessComponent>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j     = Component::serialize();
            j["effect"]          = (int)effect;
            j["intensity"]       = intensity;
            j["vignetteRadius"]  = vignetteRadius;
            j["aberration"]      = aberration;
            j["scanlineDensity"] = scanlineDensity;
            j["pixelSize"]       = pixelSize;
            j["brightness"]      = brightness;
            j["contrast"]        = contrast;
            j["saturation"]      = saturation;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("effect"))          effect          = (PostEffect)j["effect"].get<int>();
            if (j.contains("intensity"))       intensity       = j["intensity"].get<float>();
            if (j.contains("vignetteRadius"))  vignetteRadius  = j["vignetteRadius"].get<float>();
            if (j.contains("aberration"))      aberration      = j["aberration"].get<float>();
            if (j.contains("scanlineDensity")) scanlineDensity = j["scanlineDensity"].get<float>();
            if (j.contains("pixelSize"))       pixelSize       = j["pixelSize"].get<float>();
            if (j.contains("brightness"))      brightness      = j["brightness"].get<float>();
            if (j.contains("contrast"))        contrast        = j["contrast"].get<float>();
            if (j.contains("saturation"))      saturation      = j["saturation"].get<float>();
        }
    };
}
