#include "../Editor.hpp"

namespace Indium
{
    // Item authoring + live-inventory panel. Mirrors ShowQuests: editable definitions
    // (saved to items/<id>.json) when stopped; live runtime inventory (counts read from
    // StoryState, via ItemManager) during Play, where it can give/take for testing.
    void Editor::ShowItems()
    {
        if (!pm.IsProjectOpen()) { ImGui::TextDisabled("No project open."); return; }

        auto&      im      = ItemManager::Get();
        const bool playing = (state == GameState::Play);
        im.SetProjectPath(pm.GetCurrentProjectPath());
        if (!playing && im.Definitions().empty()) im.LoadAll();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        ImGui::TextDisabled("   %s", playing ? "Live Inventory (runtime — discarded on Stop)"
                                             : "Item Definitions (project's items/ folder)");
        ImGui::PopStyleVar();
        ImGui::SameLine();
        if (ImGui::SmallButton("Reload")) im.LoadAll();
        if (playing)
        {
            bool hud = im.PanelOpen();
            ImGui::SameLine();
            if (ImGui::Checkbox("Show HUD", &hud)) im.SetPanelOpen(hud);
        }
        ImGui::Separator();

        // ---- Live runtime inventory + test controls ----
        if (playing)
        {
            const auto contents = im.Contents();
            if (contents.empty()) ImGui::TextDisabled("(Inventory empty)");
            for (const auto& e : contents)
            {
                ImGui::PushID(e.id.c_str());
                ImGui::TextUnformatted((e.def && !e.def->name.empty()) ? e.def->name.c_str() : e.id.c_str());
                ImGui::SameLine(190.0f);
                ImGui::Text("x%d", e.count);
                ImGui::SameLine();
                if (ImGui::SmallButton("+")) im.Give(e.id, 1);
                ImGui::SameLine();
                if (ImGui::SmallButton("-")) im.Take(e.id, 1);
                ImGui::PopID();
            }

            if (!im.Definitions().empty())
            {
                ImGui::Separator();
                ImGui::TextDisabled("Give item (testing)");
                for (const auto& [id, d] : im.Definitions())
                {
                    ImGui::PushID(id.c_str());
                    if (ImGui::SmallButton("Give")) im.Give(id, 1);
                    ImGui::SameLine();
                    ImGui::TextUnformatted(d.name.empty() ? id.c_str() : d.name.c_str());
                    ImGui::PopID();
                }
            }
            return;
        }

        // ---- Authoring (stopped) ----
        if (ImGui::Button("+ New Item"))
        {
            // Unique default id, persisted immediately so it survives a Reload.
            std::string base = "new_item", id = base;
            for (int n = 1; im.Definition(id); ++n) id = base + "_" + std::to_string(n);
            ItemDef d; d.id = id; d.name = "New Item"; d.stackable = true;
            im.SaveDefinition(d);
        }
        ImGui::Separator();

        if (im.Definitions().empty())
        {
            ImGui::TextDisabled("(No items — click New Item, or add items/<id>.json)");
            return;
        }

        // Deferred mutations: never re-key/erase the map while iterating it.
        bool        doSave = false, doDelete = false;
        std::string saveOldId, deleteId;
        ItemDef     saveDef;

        for (auto& [key, def] : im.EditableDefinitions())
        {
            ImGui::PushID(key.c_str());
            const std::string header = (def.name.empty() ? key : def.name) + "  (" + key + ")";
            if (ImGui::CollapsingHeader(header.c_str()))
            {
                char buf[256];
                auto field = [&](const char* label, const char* id, std::string& value)
                {
                    ImGui::TextDisabled("%s", label);
                    strncpy(buf, value.c_str(), sizeof(buf) - 1);
                    buf[sizeof(buf) - 1] = '\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText(id, buf, sizeof(buf))) value = buf;
                };

                field("Id (renamed on Save)",         "##id",   def.id);
                field("Name",                         "##name", def.name);
                field("Description",                  "##desc", def.description);
                field("Icon (project-relative path)", "##icon", def.icon);

                ImGui::Checkbox("Stackable", &def.stackable);
                if (def.stackable)
                {
                    ImGui::TextDisabled("Max Stack (0 = unlimited)");
                    ImGui::SetNextItemWidth(-1);
                    ImGui::DragInt("##max", &def.maxStack, 0.1f, 0, 9999);
                }
                ImGui::TextDisabled("Value");
                ImGui::SetNextItemWidth(-1);
                ImGui::DragInt("##val", &def.value, 0.1f, 0, 999999);

                if (ImGui::SmallButton("Save"))   { doSave = true; saveOldId = key; saveDef = def; }
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) { doDelete = true; deleteId = key; }
            }
            ImGui::PopID();
        }

        if (doSave)
        {
            // A changed id means a new file: drop the old one (file + key), then write the new.
            if (!saveDef.id.empty() && saveDef.id != saveOldId) im.DeleteDefinition(saveOldId);
            im.SaveDefinition(saveDef);
        }
        if (doDelete) im.DeleteDefinition(deleteId);
    }
}
