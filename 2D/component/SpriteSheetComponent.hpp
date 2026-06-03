#pragma once
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "SpriteRendererComponent.hpp"
#include "../../include/nlohmann/json.hpp"
#include "imgui.h"

namespace Indium
{
    // --------------------------------------------------------------------
    // SpriteSheetComponent  ("Sprite Sheet / Atlas")
    //
    // Slices the sibling SpriteRenderer's texture into a uniform grid and
    // shows ONE frame, selected by index.  Use it to pull a single icon /
    // tile out of an atlas, or to drive frame changes from script without a
    // full Animator clip:
    //
    //   GetComponent<SpriteSheetComponent>()->SetFrame(3);
    //
    // For looping animation use AnimatorComponent instead; this is for
    // static or script-controlled single-frame selection. Applied in
    // lateUpdate so it overrides the renderer's source rect each frame.
    // --------------------------------------------------------------------
    struct SpriteSheetComponent : Component
    {
        int columns    = 4;
        int rows       = 4;
        int frameIndex = 0;

        void update(float, Vector2, Scene*) override {}

        // --- Script-facing ---
        void SetFrame(int i) { frameIndex = i; }
        [[nodiscard]] int FrameCount() const { return columns * rows; }

        void lateUpdate(float, Vector2, Scene*) override { apply_(); }

        void apply_()
        {
            if (!owner || columns < 1 || rows < 1) return;
            auto* sr = owner->getComponent<SpriteRendererComponent>();
            if (!sr || sr->texture.id == 0) return;

            int total = columns * rows;
            int idx   = frameIndex % total;
            if (idx < 0) idx += total;

            float fw = (float)sr->texture.width  / columns;
            float fh = (float)sr->texture.height / rows;
            int   cx = idx % columns;
            int   cy = idx / columns;

            sr->sourceRec = { cx * fw, cy * fh, fw, fh };
        }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::TextDisabled("Slices the Sprite Renderer texture into a\ngrid and shows one frame.");
            ImGui::Spacing();

            ImGui::Text("Columns / Rows");
            ImGui::PushItemWidth(-1);
            if (ImGui::DragInt("##SSCols", &columns, 0.1f, 1, 256, "Cols %d")) { if (snapshotCb) snapshotCb(); }
            if (ImGui::DragInt("##SSRows", &rows,    0.1f, 1, 256, "Rows %d")) { if (snapshotCb) snapshotCb(); }
            ImGui::PopItemWidth();
            if (columns < 1) columns = 1;
            if (rows < 1) rows = 1;

            ImGui::Text("Frame Index (0..%d)", columns * rows - 1);
            ImGui::PushItemWidth(-1);
            ImGui::SliderInt("##SSFrame", &frameIndex, 0, columns * rows - 1);
            if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();
            ImGui::PopItemWidth();

            if (owner && !owner->getComponent<SpriteRendererComponent>())
                ImGui::TextColored(ImVec4(0.9f,0.6f,0.2f,1), "Needs a Sprite Renderer.");
            else
                apply_(); // live preview in the editor
        }

        std::string getName() const override { return "SpriteSheet"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<SpriteSheetComponent>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["columns"]    = columns;
            j["rows"]       = rows;
            j["frameIndex"] = frameIndex;
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("columns"))    columns    = j["columns"].get<int>();
            if (j.contains("rows"))       rows       = j["rows"].get<int>();
            if (j.contains("frameIndex")) frameIndex = j["frameIndex"].get<int>();
        }
    };
}
