#pragma once
#include <string>
#include <vector>
#include <cstring>
#include <utility>
#include "../../core/Component.hpp"
#include "../../core/Entity.hpp"
#include "../../core/ItemManager.hpp"
#include "raylib.h"
#include "imgui.h"

namespace Indium
{
    /**
     * @brief A per-entity item container: chests, NPC stock, loot piles.
     *
     * The PLAYER's inventory is the global StoryState item.* namespace (see ItemManager);
     * this component is for everything that is NOT the player. It holds its own stacks,
     * serialized with the entity in the scene file, and can pour them into the player's
     * inventory with TransferAllTo() — typically driven by an InteractableComponent on the
     * same entity with `lootContainer` set, so a chest works with no script.
     */
    struct InventoryComponent : Component
    {
        struct ItemStack
        {
            std::string id;
            int         count = 1;
        };

        std::vector<ItemStack> contents;

        void update(float, Vector2, Scene*) override {}

        std::string getName() const override { return "Inventory"; }

        /** @brief Pours this container into the player inventory (ItemManager). Stacks that
         *  don't fully fit (stack caps) keep their remainder, so nothing is silently lost. */
        void TransferAllTo(ItemManager& im)
        {
            std::vector<ItemStack> remaining;
            for (const auto& s : contents)
            {
                if (s.id.empty() || s.count <= 0) continue;
                const int before = im.Count(s.id);
                im.Give(s.id, s.count);
                const int added = im.Count(s.id) - before;
                if (s.count - added > 0) remaining.push_back({ s.id, s.count - added });
            }
            contents = std::move(remaining);
        }

        void TransferAllTo() { TransferAllTo(ItemManager::Get()); }

        void inspect(std::function<void()> snapshotCb) override
        {
            ImGui::TextDisabled("Contents (item id + count)");

            int  removeIdx = -1;
            char buf[128];
            for (std::size_t i = 0; i < contents.size(); ++i)
            {
                ImGui::PushID((int)i);
                strncpy(buf, contents[i].id.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputText("##id", buf, sizeof(buf))) contents[i].id = buf;
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

                ImGui::SameLine();
                ImGui::SetNextItemWidth(90);
                ImGui::DragInt("##cnt", &contents[i].count, 0.1f, 1, 9999);
                if (ImGui::IsItemActivated() && snapshotCb) snapshotCb();

                ImGui::SameLine();
                if (ImGui::SmallButton("X")) removeIdx = (int)i;
                ImGui::PopID();
            }
            if (removeIdx >= 0)
            {
                if (snapshotCb) snapshotCb();
                contents.erase(contents.begin() + removeIdx);
            }

            if (ImGui::SmallButton("+ Add Item Stack"))
            {
                if (snapshotCb) snapshotCb();
                contents.push_back({ "", 1 });
            }
        }

        std::unique_ptr<Component> clone() const override
        {
            auto c      = std::make_unique<InventoryComponent>();
            c->enabled  = enabled;
            c->contents = contents;
            return c;
        }

        nlohmann::json serialize() const override
        {
            nlohmann::json j   = Component::serialize();
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& s : contents) arr.push_back({ { "id", s.id }, { "count", s.count } });
            j["contents"] = std::move(arr);
            return j;
        }

        void deserialize(const nlohmann::json& j) override
        {
            Component::deserialize(j);
            contents.clear();
            if (j.contains("contents") && j["contents"].is_array())
                for (const auto& sj : j["contents"])
                    contents.push_back({ sj.value("id", std::string{}), sj.value("count", 1) });
        }
    };
}
