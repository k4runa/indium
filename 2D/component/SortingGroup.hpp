#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../include/nlohmann/json.hpp"
#include "imgui.h"

namespace Indium
{
    // --------------------------------------------------------------------
    // SortingGroup
    //
    // Overrides the depthLayer and sortingOrder of every child entity in
    // the hierarchy each frame.  Attach this to a parent entity to control
    // the render order of its entire subtree as a single unit — equivalent
    // to Unity's Sorting Group component.
    //
    // Children retain their own depthLayer/sortingOrder values; this
    // component writes over them every update, so manual per-child tweaks
    // will be overwritten while the group is enabled.
    // --------------------------------------------------------------------
    struct SortingGroup : Component
    {
        int sortingLayer = 0;   // Coarse depth layer (maps to Entity::depthLayer)
        int sortingOrder = 0;   // Fine order within the layer (maps to Entity::sortingOrder)

        void update(float /*dt*/, Vector2 /*worldSize*/, Scene* /*scene*/) override
        {
            if (!owner) return;
            applyToChildren_(owner);
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::TextDisabled("Overrides the render depth of all\nchildren in this entity's hierarchy.");
            ImGui::Spacing();

            ImGui::Text("Sorting Layer");
            ImGui::PushItemWidth(-1);
            ImGui::DragInt("##SGLayer", &sortingLayer, 0.25f, -100, 100);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            ImGui::Text("Sorting Order");
            ImGui::PushItemWidth(-1);
            ImGui::DragInt("##SGOrder", &sortingOrder, 1.0f, -10000, 10000);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            if (owner && owner->children.empty())
                ImGui::TextDisabled("(No children — add child entities\nto see this take effect.)");
        }

        std::string getName() const override { return "SortingGroup"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<SortingGroup>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j  = Component::serialize();
            j["sortingLayer"] = sortingLayer;
            j["sortingOrder"] = sortingOrder;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("sortingLayer")) sortingLayer = j["sortingLayer"].get<int>();
            if (j.contains("sortingOrder")) sortingOrder = j["sortingOrder"].get<int>();
        }

    private:
        void applyToChildren_(Entity* e)
        {
            for (Entity* child : e->children)
            {
                child->depthLayer   = sortingLayer;
                child->sortingOrder = sortingOrder;
                applyToChildren_(child);
            }
        }
    };
}
