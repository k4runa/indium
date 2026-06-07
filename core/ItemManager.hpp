#pragma once
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <climits>
#include "raylib.h"
#include "EventBus.hpp"
#include "events/GameEvents.hpp"
#include "StoryState.hpp"
#include "Screen.hpp"
#include "GUI.hpp"
#include "InputManager.hpp"
#include "AssetManager.hpp"
#include "../include/nlohmann/json.hpp"

namespace Indium
{
    namespace GameEvents
    {
        /**
         * @brief Fired when the player's count of an item changes, so HUD toasts /
         * audio / logs can react without polling. (StoryState already records the count
         * under item.<id>; this carries the structured "which item / how much" context,
         * mirroring QuestEvent.)
         */
        struct ItemEvent
        {
            enum class Type { Added, Removed };
            Type        type;
            std::string itemId;
            int         delta    = 0;   // signed change this event (+added / -removed)
            int         newCount = 0;   // resulting count after the change
        };
    }

    /** @brief An item definition, authored as <project>/items/<id>.json. */
    struct ItemDef
    {
        std::string              id;
        std::string              name;
        std::string              description;
        std::string              icon;            // optional image (project-relative path, or absolute)
        bool                     stackable = true;
        int                      maxStack  = 0;   // 0 = unlimited (only meaningful when stackable)
        int                      value     = 0;   // optional gameplay/economy value
        std::vector<std::string> tags;            // optional free-form categories
    };

    /**
     * @brief Global item registry + player inventory. Header-only singleton shared
     * across the script dylib boundary (like StoryState / QuestManager / DialogueManager).
     *
     * Item DEFINITIONS load from <project>/items/<id>.json. The player's inventory is
     * COUNTS stored in StoryState under the "item." namespace (the same pattern
     * QuestManager uses for "quest."):
     *
     *   item.<id> : int   (how many the player holds; absent / 0 = none)
     *
     * Because counts are ordinary StoryState ints, the inventory rides Save/Load and
     * per-scene seeding with no extra persistence code, and it plugs into the existing
     * story-expression helpers for free:
     *   - dialogue/quest conditions:  requireFlag "item.gold >= 10"   (see StoryEval)
     *   - dialogue text:              "You have {item.gold} gold."     (see StoryInterpolate)
     *   - quest re-evaluation:        Give/Take fire StoryStateChangedEvent, which
     *                                 QuestManager already listens to.
     *
     * The only thing counts can't express declaratively is the MUTATION (setFlag only
     * sets a bool true), so Give/Take live here and are called by dialogue choices,
     * Interactables and scripts.
     *
     * Item ids must be simple identifiers ([A-Za-z0-9_], no dots) so "item.<id>" stays a
     * clean StoryEval/StoryInterpolate key — the same rule quest ids follow.
     *
     * Data file (<project>/items/<id>.json):
     *   { "id": "gold", "name": "Gold", "stackable": true, "value": 1 }
     *   { "id": "key_brass", "name": "Brass Key", "stackable": false, "icon": "icons/key.png" }
     */
    class ItemManager
    {
    public:
        ItemManager(const ItemManager&)            = delete;
        ItemManager& operator=(const ItemManager&) = delete;
        ItemManager(ItemManager&&)                 = delete;
        ItemManager& operator=(ItemManager&&)      = delete;

        static ItemManager& Get() { static ItemManager inst; return inst; }

        /** @brief One inventory line: the id, its definition (null if unauthored) and count. */
        struct Entry
        {
            std::string    id;
            const ItemDef* def   = nullptr;
            int            count = 0;
        };

        /** @brief Where item files are loaded from. Set by the editor on project open. */
        void SetProjectPath(const std::string& path) { projectPath_ = path; }
        [[nodiscard]] const std::string& GetProjectPath() const { return projectPath_; }

        /** @brief Resolve an item icon path (project-relative or absolute) to a filesystem
         *  path for AssetManager::GetTexture. Empty -> empty. Mirrors
         *  DialogueManager::ResolvePortraitPath so the HUD and panel stay in step. */
        static std::string ResolveIconPath(const std::string& icon, const std::string& projectPath)
        {
            if (icon.empty()) return {};
            std::filesystem::path pp(icon);
            if (pp.is_relative() && !projectPath.empty()) pp = std::filesystem::path(projectPath) / pp;
            return pp.string();
        }

        // --- Definition loading (mirrors QuestManager) -------------------------

        /** @brief Loads every item JSON in the <project>/items folder into the table. */
        void LoadAll()
        {
            defs_.clear();
            if (projectPath_.empty()) return;
            std::error_code       ec;
            std::filesystem::path dir = std::filesystem::path(projectPath_) / "items";
            if (!std::filesystem::exists(dir, ec)) return;
            for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
            {
                if (ec) break;
                if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
                LoadFile(entry.path().string());
            }
        }

        /** @brief Loads a single item by id from <project>/items/<id>.json. */
        bool Load(const std::string& id)
        {
            if (projectPath_.empty()) { TraceLog(LOG_WARNING, "ITEM: no project path set"); return false; }
            const std::string path = (std::filesystem::path(projectPath_) / "items" / (id + ".json")).string();
            return LoadFile(path);
        }

        /** @brief Parses an item definition from JSON. Used by Load() and by tests. */
        bool LoadFromJson(const nlohmann::json& j)
        {
            ItemDef d = ParseDef(j);
            if (d.id.empty()) { TraceLog(LOG_WARNING, "ITEM: definition is missing 'id'"); return false; }
            defs_[d.id] = std::move(d);
            return true;
        }

        /** @brief All loaded definitions — used by the editor panel. */
        [[nodiscard]] const std::map<std::string, ItemDef>& Definitions() const { return defs_; }

        [[nodiscard]] const ItemDef* Definition(const std::string& id) const
        {
            auto it = defs_.find(id);
            return it == defs_.end() ? nullptr : &it->second;
        }

        /** @brief Registers/updates a definition in memory (editor authoring). */
        void SetDefinition(const ItemDef& d) { if (!d.id.empty()) defs_[d.id] = d; }

        /** @brief Mutable view of the definitions, for in-place editing by the editor panel. */
        std::map<std::string, ItemDef>& EditableDefinitions() { return defs_; }

        /** @brief Writes one definition to <project>/items/<id>.json (atomic temp+rename) and
         *  registers it in memory. Editor authoring. Returns false on I/O failure. */
        bool SaveDefinition(const ItemDef& d)
        {
            if (projectPath_.empty() || d.id.empty()) return false;
            std::error_code       ec;
            std::filesystem::path dir = std::filesystem::path(projectPath_) / "items";
            std::filesystem::create_directories(dir, ec);
            std::filesystem::path path = dir / (d.id + ".json");
            std::filesystem::path tmp  = path; tmp += ".tmp";
            {
                std::ofstream out(tmp);
                if (!out.is_open()) { TraceLog(LOG_ERROR, "ITEM: cannot write %s", tmp.string().c_str()); return false; }
                out << std::setw(2) << ToJson(d) << std::endl;
                if (!out.good()) { std::filesystem::remove(tmp, ec); return false; }
            }
            std::filesystem::rename(tmp, path, ec);
            if (ec) { TraceLog(LOG_ERROR, "ITEM: cannot finalize %s", path.string().c_str()); return false; }
            defs_[d.id] = d;
            return true;
        }

        /** @brief Deletes <project>/items/<id>.json and drops the in-memory definition. */
        bool DeleteDefinition(const std::string& id)
        {
            defs_.erase(id);
            if (projectPath_.empty() || id.empty()) return false;
            std::error_code       ec;
            std::filesystem::path path = std::filesystem::path(projectPath_) / "items" / (id + ".json");
            return std::filesystem::remove(path, ec);
        }

        // --- Player inventory (StoryState-backed) ------------------------------

        /** @brief StoryState key holding the player's count of an item (public so the editor
         *  panel can display it). Mirrors QuestManager::StateKey. */
        static std::string CountKey(const std::string& id) { return "item." + id; }

        [[nodiscard]] int Count(const std::string& id) const
        {
            return StoryState::Get().GetInt(CountKey(id), 0);
        }

        [[nodiscard]] bool Has(const std::string& id, int n = 1) const { return Count(id) >= n; }

        /** @brief Adds n of an item, clamped to the def's stack cap. Fires ItemEvent::Added.
         *  n <= 0 is a no-op. An item with no def is treated as unlimited-stackable, so simple
         *  games can use plain counters (gold, score) without authoring a definition. */
        void Give(const std::string& id, int n = 1)
        {
            if (id.empty() || n <= 0) return;
            const int before = Count(id);
            const int after  = ClampToCap(id, before + n);
            if (after == before) return;            // already at cap — nothing to add
            StoryState::Get().Set(CountKey(id), after);
            Events::Publish(GameEvents::ItemEvent{ GameEvents::ItemEvent::Type::Added, id, after - before, after });
        }

        /** @brief Removes n of an item if the player has at least n; returns false (no change)
         *  otherwise. Clears the StoryState key at 0 so an empty inventory stays clean. */
        bool Take(const std::string& id, int n = 1)
        {
            if (id.empty() || n <= 0) return false;
            const int before = Count(id);
            if (before < n) return false;
            const int after = before - n;
            if (after <= 0) StoryState::Get().Remove(CountKey(id));
            else            StoryState::Get().Set(CountKey(id), after);
            Events::Publish(GameEvents::ItemEvent{ GameEvents::ItemEvent::Type::Removed, id, after - before, after });
            return true;
        }

        /** @brief Directly sets a count (editor/debug). Clamps to the stack cap; clears at <= 0. */
        void SetCount(const std::string& id, int n)
        {
            if (id.empty()) return;
            const int before = Count(id);
            const int after  = ClampToCap(id, n);
            if (after == before) return;
            if (after <= 0) StoryState::Get().Remove(CountKey(id));
            else            StoryState::Get().Set(CountKey(id), after);
            const auto type = (after >= before) ? GameEvents::ItemEvent::Type::Added
                                                : GameEvents::ItemEvent::Type::Removed;
            Events::Publish(GameEvents::ItemEvent{ type, id, after - before, after });
        }

        /** @brief Removes an item entirely from the inventory. */
        void Remove(const std::string& id) { if (int c = Count(id)) Take(id, c); }

        /**
         * @brief The player's current inventory: every item.<id> with a positive count,
         * paired with its definition (null when unauthored). Order follows StoryState's
         * sorted key order. Used by the HUD overlay and the editor panel.
         */
        [[nodiscard]] std::vector<Entry> Contents() const
        {
            std::vector<Entry> out;
            const std::string prefix = "item.";
            for (const auto& [key, val] : StoryState::Get().Values())
            {
                if (key.size() <= prefix.size() || key.compare(0, prefix.size(), prefix) != 0) continue;
                const int* c = std::get_if<int>(&val);
                if (!c || *c <= 0) continue;
                const std::string id = key.substr(prefix.size());
                out.push_back(Entry{ id, Definition(id), *c });
            }
            return out;
        }

        // --- In-game inventory overlay (engine-drawn, like QuestManager::DrawLogGUI) ---

        [[nodiscard]] bool PanelOpen() const { return panelOpen_; }
        void SetPanelOpen(bool open) { panelOpen_ = open; }

        /**
         * @brief Draws the inventory overlay in the screen-space UI pass. Toggled by the
         * "Inventory" input action (falling back to I) when acceptInput is true — mirrors how
         * QuestManager's log toggles on "QuestLog"/J. Lists owned items with icon + count.
         */
        void DrawInventoryGUI(bool acceptInput)
        {
            if (acceptInput && (InputManager::Get().IsPressed("Inventory") || IsKeyPressed(KEY_I)))
                panelOpen_ = !panelOpen_;
            if (!panelOpen_) return;

            const std::vector<Entry> items = Contents();

            const float panelW = 300.0f, margin = 16.0f, pad = 14.0f;
            const float headerH = 30.0f, rowH = 40.0f, iconSz = 32.0f;
            const float x = margin;
            float       y = margin;

            ::Rectangle header = { x, y, panelW, headerH };
            GUI::Box(header, Color{ 12, 12, 16, 235 }, Color{ 120, 120, 140, 255 }, 2.0f);
            GUI::Label("Inventory", x + pad, y + 6.0f, 20, Color{ 230, 210, 140, 255 });
            y += headerH + 6.0f;

            const float bodyH = pad + (items.empty() ? 24.0f : (float)items.size() * rowH) + pad;
            ::Rectangle body  = { x, y, panelW, bodyH };
            GUI::Box(body, Color{ 12, 12, 16, 220 }, Color{ 90, 90, 110, 255 }, 1.0f);

            if (items.empty())
            {
                GUI::Label("(Empty)", x + pad, y + pad, 16, Color{ 170, 170, 185, 255 });
                return;
            }

            float ry = y + pad;
            for (const auto& e : items)
            {
                float tx = x + pad;

                // Optional icon, drawn in a square slot on the left of the row.
                if (e.def && !e.def->icon.empty() && !projectPath_.empty())
                {
                    Texture2D tex = AssetManager::Get().GetTexture(ResolveIconPath(e.def->icon, projectPath_));
                    if (tex.id != 0)
                    {
                        ::Rectangle ir = { tx, ry + (rowH - iconSz) * 0.5f, iconSz, iconSz };
                        GUI::Image(tex, ir);
                        GUI::Box(ir, BLANK, Color{ 120, 120, 140, 255 }, 1.0f);
                    }
                }
                tx += iconSz + 10.0f;

                const std::string label = (e.def && !e.def->name.empty()) ? e.def->name : e.id;
                GUI::Label(label.c_str(), tx, ry + (rowH - 18.0f) * 0.5f, 18, RAYWHITE);

                // Count, right-aligned within the row.
                const std::string cnt = "x" + std::to_string(e.count);
                const float       cw  = (float)MeasureText(cnt.c_str(), 18);
                GUI::Label(cnt.c_str(), x + panelW - pad - cw, ry + (rowH - 18.0f) * 0.5f, 18,
                           Color{ 210, 210, 220, 255 });

                ry += rowH;
            }
        }

        // --- Document (de)serialization (editor authoring) ---------------------

        /** @brief Serializes an item definition to its on-disk JSON shape. Empty optional
         *  fields are omitted so authored files stay clean (mirrors DialogueManager::ToJson). */
        static nlohmann::json ToJson(const ItemDef& d)
        {
            nlohmann::json j;
            j["id"]        = d.id;
            j["name"]      = d.name;
            j["stackable"] = d.stackable;
            if (!d.description.empty()) j["description"] = d.description;
            if (!d.icon.empty())        j["icon"]        = d.icon;
            if (d.maxStack != 0)        j["maxStack"]    = d.maxStack;
            if (d.value != 0)           j["value"]       = d.value;
            if (!d.tags.empty())        j["tags"]        = d.tags;
            return j;
        }

    private:
        ItemManager()  = default;
        ~ItemManager() = default;

        /** @brief Clamp a desired count to [0, cap], where cap = maxStack for a stackable item
         *  (unlimited when maxStack == 0) and 1 for a non-stackable one. Unknown ids (no def)
         *  are treated as unlimited stackable. */
        int ClampToCap(const std::string& id, int n) const
        {
            if (n < 0) return 0;
            const ItemDef* d = Definition(id);
            if (!d) return n;
            const int cap = d->stackable ? (d->maxStack > 0 ? d->maxStack : INT_MAX) : 1;
            return n > cap ? cap : n;
        }

        bool LoadFile(const std::string& path)
        {
            std::ifstream f(path);
            if (!f.is_open()) { TraceLog(LOG_WARNING, "ITEM: cannot open %s", path.c_str()); return false; }
            nlohmann::json j;
            try { f >> j; } catch (...) { TraceLog(LOG_WARNING, "ITEM: invalid JSON in %s", path.c_str()); return false; }
            return LoadFromJson(j);
        }

        static ItemDef ParseDef(const nlohmann::json& j)
        {
            ItemDef d;
            d.id          = j.value("id", std::string{});
            d.name        = j.value("name", std::string{});
            d.description = j.value("description", std::string{});
            d.icon        = j.value("icon", std::string{});
            d.stackable   = j.value("stackable", true);
            d.maxStack    = j.value("maxStack", 0);
            d.value       = j.value("value", 0);
            if (j.contains("tags") && j["tags"].is_array())
                for (const auto& t : j["tags"]) if (t.is_string()) d.tags.push_back(t.get<std::string>());
            return d;
        }

        std::string                    projectPath_;
        std::map<std::string, ItemDef> defs_;
        bool                           panelOpen_ = false;
    };
}
