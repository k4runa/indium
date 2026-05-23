#pragma once
#include "Component.hpp"
#include "imgui.h"

namespace Indium
{
    // Stores the raw JSON of a component whose type is not currently registered.
    // Serializes back to the original blob on save, so no data is lost even when
    // a script fails to compile or is renamed. Replaced by the real component if
    // a later hot-reload resolves the type.
    struct PlaceholderComponent : Component
    {
        std::string    unknownType;
        nlohmann::json rawJson;

        void update(float, Vector2, Scene*) override {}

        std::string getName() const override
        {
            return unknownType.empty() ? "Placeholder" : unknownType;
        }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<PlaceholderComponent>(*this);
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
            ImGui::TextWrapped("[Missing] %s", unknownType.c_str());
            ImGui::PopStyleColor();
            ImGui::TextDisabled("Script not compiled or type not registered.");
            ImGui::TextDisabled("Component data is preserved for re-save.");
        }

        nlohmann::json serialize() const override { return rawJson; }

        void deserialize(const nlohmann::json& j) override
        {
            rawJson = j;
            if (j.contains("type")) unknownType = j["type"].get<std::string>();
        }
    };
}
