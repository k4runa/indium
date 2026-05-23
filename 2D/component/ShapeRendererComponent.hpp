#pragma once
#include "Collider2D.hpp"
#include "raylib.h"
#include "imgui.h"

namespace Indium
{
    // --------------------------------------------------------------------
    // ShapeRendererComponent  —  draws a primitive shape (rect or circle).
    // Works together with BoxCollider2D / CircleCollider2D for sizing:
    //   • Rectangle  — uses owner->scale via BoxCollider2D or direct scale
    //   • Circle     — reads radius from the sibling CircleCollider2D
    // --------------------------------------------------------------------
    struct ShapeRendererComponent : Component
    {
        enum class ShapeType { Rectangle, Circle };
        ShapeType shapeType = ShapeType::Rectangle;

        void update(float, Vector2, Scene*) override {}

        void draw() const override
        {
            if (!owner || !owner->activeInHierarchy()) return;

            Vector2 gPos  = owner->getGlobalPosition();
            Vector2 gScl  = owner->getGlobalScale();
            float   gRot  = owner->getGlobalRotation();
            Color   col   = owner->color;

            if (shapeType == ShapeType::Circle)
            {
                float r = 50.0f;
                const auto* cCol = owner->getComponent<CircleCollider2D>();
                if (cCol) r = cCol->radius;
                DrawCircleV(gPos, r, col);
            }
            else
            {
                DrawRectanglePro({ gPos.x, gPos.y, gScl.x, gScl.y }, { gScl.x * 0.5f, gScl.y * 0.5f }, gRot, col);
            }
        }

        void inspect(std::function<void()> snapshotCb = {}) override
        {
            const char* types[] = { "Rectangle", "Circle" };
            int idx = static_cast<int>(shapeType);
            ImGui::Text("Shape Type");
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##ShapeType", &idx, types, 2))
            {
                if (snapshotCb) snapshotCb();
                shapeType = static_cast<ShapeType>(idx);
            }
            ImGui::PopItemWidth();
        }

        std::string getName() const override { return "ShapeRenderer"; }

        std::unique_ptr<Component> clone() const override
        {
            return std::make_unique<ShapeRendererComponent>(*this);
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j = Component::serialize();
            j["shapeType"] = static_cast<int>(shapeType);
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            if (j.contains("shapeType")) shapeType = static_cast<ShapeType>(j["shapeType"].get<int>());
        }
    };

} // namespace Indium
