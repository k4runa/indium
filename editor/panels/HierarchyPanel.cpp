#include "../Editor.hpp"

namespace Indium
{
    void Editor::ShowHierarchy()
    {
        ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
        if (ImGui::Button("+ Add Entity", ImVec2(-1, 0))) ImGui::OpenPopup("AddEntityPopup");
        ImGui::PopStyleVar();

        if (ImGui::BeginPopup("AddEntityPopup"))
        {
            ImGui::TextDisabled("Create New...");
            ImGui::Separator();
            if (ImGui::MenuItem("Circle"))          { TakeSnapshot(); auto e = factory.CreateCircle(scene);    e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            if (ImGui::MenuItem("Rectangle"))       { TakeSnapshot(); auto e = factory.CreateRectangle(scene); e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            if (ImGui::MenuItem("Surface"))         { TakeSnapshot(); auto e = factory.CreatePlane(scene);     e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            if (ImGui::MenuItem("Image (Sprite)"))  { TakeSnapshot(); auto e = factory.CreateSprite(scene);    e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            if (ImGui::MenuItem("Tilemap"))         { TakeSnapshot(); auto e = factory.CreateTilemap(scene);   e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            if (ImGui::MenuItem("Camera"))          { TakeSnapshot(); auto e = factory.CreateCamera(scene);    e->position = editorCamera.target; scene.entities.push_back(std::move(e)); }
            ImGui::EndPopup();
        }

        // --- Search box ---
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##hierSearch", ICON_FA_MAGNIFYING_GLASS "  Search entities...",
                                 hierarchySearchBuf_, sizeof(hierarchySearchBuf_));
        std::string hfilter = hierarchySearchBuf_;
        std::transform(hfilter.begin(), hfilter.end(), hfilter.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });
        const bool hSearching = !hfilter.empty();
        auto nameMatches = [&](const std::string& name)
        {
            if (!hSearching) return true;
            std::string n = name;
            std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c){ return (char)std::tolower(c); });
            return n.find(hfilter) != std::string::npos;
        };

        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5));

        std::vector<int> entitiesToDelete;

        // Icon for an entity by type / component.
        auto iconFor = [](Entity* e) -> const char*
        {
            for (const auto& c : e->components) if (dynamic_cast<CameraComponent*>(c.get())) return ICON_FA_CAMERA;
            const std::string& t = e->getType();
            if (t == "Circle")    return ICON_FA_CIRCLE;
            if (t == "Rectangle") return ICON_FA_VECTOR_SQUARE;
            if (t == "Plane")     return ICON_FA_LAYER_GROUP;
            if (t == "Sprite")    return ICON_FA_IMAGE;
            if (t == "Tilemap")   return ICON_FA_TABLE_CELLS;
            return ICON_FA_CUBE;
        };

        // Begin an in-place rename for an entity (F2 / double-click).
        auto beginRename = [&](Entity* e)
        {
            renamingEntityId_   = e->id;
            renameFocusPending_ = true;
            strncpy(entityRenameBuf_, e->name.c_str(), sizeof(entityRenameBuf_) - 1);
            entityRenameBuf_[sizeof(entityRenameBuf_) - 1] = '\0';
        };

        // Draw the rename text field for `e`. Returns true if it consumed this row
        // (i.e. the entity is currently being renamed).
        auto drawRename = [&](Entity* e) -> bool
        {
            if (renamingEntityId_ != e->id) return false;
            ImGui::SetNextItemWidth(-1);
            if (renameFocusPending_) { ImGui::SetKeyboardFocusHere(); renameFocusPending_ = false; }
            bool done = ImGui::InputText("##rename", entityRenameBuf_, sizeof(entityRenameBuf_),
                                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            if (done || ImGui::IsItemDeactivated())
            {
                if (entityRenameBuf_[0] != '\0') { TakeSnapshot(); e->name = entityRenameBuf_; }
                renamingEntityId_ = -1;
            }
            return true;
        };

        std::unordered_map<Entity*, int> entityIndexMap;
        entityIndexMap.reserve(scene.entities.size());
        for (int i = 0; i < (int)scene.entities.size(); i++) entityIndexMap[scene.entities[i].get()] = i;

        // Lambda: recursively draw an entity as a tree node
        std::function<void(Entity*, int)> DrawEntityNode;
        DrawEntityNode = [&](Entity* entity, int index)
        {
            ImGui::PushID(entity->id);

            // In-place rename takes over the row entirely.
            if (drawRename(entity)) { ImGui::PopID(); return; }

            // --- Stealth Styling ---
            ImGuiStyle& style    = ImGui::GetStyle();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 cursorPos     = ImGui::GetCursorScreenPos();
            float fullWidth      = ImGui::GetContentRegionAvail().x;
            float nodeHeight     = 25.0f;

            // --- Entity Icons ---
            const char* icon = ICON_FA_CUBE;
            if (entity->getType() == "Circle")    icon = ICON_FA_CIRCLE;
            if (entity->getType() == "Rectangle") icon = ICON_FA_VECTOR_SQUARE;
            if (entity->getType() == "Plane")     icon = ICON_FA_LAYER_GROUP;
            if (entity->getType() == "Sprite")    icon = ICON_FA_IMAGE;
            if (entity->getType() == "Tilemap")   icon = ICON_FA_TABLE_CELLS;
            for (const auto& c : entity->components) { if (dynamic_cast<CameraComponent*>(c.get())) { icon = ICON_FA_CAMERA; break; } }

            // 1. Draw Rounded Selection Background (If selected)
            bool inMultiSel_ = std::find(multiSelection_.begin(), multiSelection_.end(), index) != multiSelection_.end();
            if (selectedIndex == index)
            {
                ImU32 selCol = ImGui::GetColorU32(ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
                drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + fullWidth, cursorPos.y + nodeHeight), selCol, 5.0f);
                drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + 3, cursorPos.y + nodeHeight), ImColor(100, 100, 100, 255), 5.0f);
            }
            else if (inMultiSel_)
            {
                ImU32 multiSelCol = ImGui::GetColorU32(ImVec4(0.10f, 0.20f, 0.35f, 1.0f));
                drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + fullWidth, cursorPos.y + nodeHeight), multiSelCol, 4.0f);
                drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + 3, cursorPos.y + nodeHeight), ImColor(0, 140, 220, 255), 4.0f);
            }

            // 2. Node Styling
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));

            // Clear default header colors to use custom background
            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, selectedIndex == index ? ImVec4(0,0,0,0) : ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  selectedIndex == index ? ImVec4(0,0,0,0) : ImVec4(0.10f, 0.10f, 0.10f, 0.5f));

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

            bool isLeaf = entity->children.empty();
            if (isLeaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (selectedIndex == index) flags |= ImGuiTreeNodeFlags_Selected;

            // 3. Draw the Node
            char label[256];
            snprintf(label, sizeof(label), "%s %s", icon, entity->name.c_str());
            bool nodeOpen = ImGui::TreeNodeEx("##node", flags, "%s", label);
            bool wasPushed = nodeOpen && !isLeaf;

            // --- Interaction Logic ---
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                if (CtrlDown())
                {
                    auto it = std::find(multiSelection_.begin(), multiSelection_.end(), index);
                    if (it != multiSelection_.end()) multiSelection_.erase(it);
                    else { multiSelection_.push_back(index); selectedIndex = index; }
                }
                else
                {
                    selectedIndex = index;
                    multiSelection_.clear();
                }
            }

            // Double-click the label to rename in place.
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) beginRename(entity);

            // Drag Source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload("ENTITY_ID", &entity->id, sizeof(int));
                ImGui::Text("Moving %s", entity->name.c_str());
                ImGui::EndDragDropSource();
            }

            // Drop Target
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID"))
                {
                    int draggedId = *(const int*)payload->Data;
                    if (draggedId != entity->id)
                    {
                        Entity* dragged = scene.FindEntity(draggedId);
                        if (dragged)
                        {
                            bool isDescendant = false;
                            Entity* temp = entity->parent;
                            while (temp) { if (temp->id == draggedId) { isDescendant = true; break; } temp = temp->parent; }

                            if (!isDescendant)
                            {
                                TakeSnapshot();
                                Vector2 gPos = dragged->getGlobalPosition();
                                dragged->setParent(entity); // fires onEnable/onDisable if hierarchy-active changed
                                dragged->setGlobalPosition(gPos);
                            }
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Context Menu
            if (ImGui::BeginPopupContextItem())
            {
                selectedIndex = index;
                ImGui::TextDisabled("Entity: %s", entity->name.c_str());
                ImGui::Separator();
                {
                    bool hm = multiSelection_.size() > 1;
                    if (ImGui::MenuItem("Cut", "Ctrl+X"))
                    {
                        CopySelected();
                        if (hm) { for (int i : multiSelection_) entitiesToDelete.push_back(i); multiSelection_.clear(); }
                        else entitiesToDelete.push_back(index);
                    }
                    if (ImGui::MenuItem("Copy", "Ctrl+C")) CopySelected();
                    bool canPasteH = !entityClipboard.is_null() || !multiClipboard_.empty();
                    if (ImGui::MenuItem("Paste", "Ctrl+V", false, canPasteH)) PasteAt(entity->position);
                    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) DuplicateSelected(index);
                }

                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_BOX "  Save as Prefab...") && pm.IsProjectOpen())
                {
                    prefabSourceIndex_ = index;
                    strncpy(prefabNameBuf_, entity->name.c_str(), sizeof(prefabNameBuf_) - 1);
                    showSavePrefabModal_ = true;
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Unparent", nullptr, false, entity->parent != nullptr))
                {
                    if (entity->parent)
                    {
                        TakeSnapshot();
                        Vector2 gPos = entity->getGlobalPosition();
                        entity->setParent(nullptr); // fires onEnable/onDisable if hierarchy-active changed
                        entity->position = gPos;
                    }
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Del"))
                {
                    if (multiSelection_.size() > 1) { for (int i : multiSelection_) entitiesToDelete.push_back(i); multiSelection_.clear(); }
                    else entitiesToDelete.push_back(index);
                }

                ImGui::EndPopup();
            }

            // Recurse into children
            if (wasPushed)
            {
                if (!entity->children.empty())
                {
                    for (Entity* child : entity->children)
                    {
                        auto it = entityIndexMap.find(child);
                        if (it != entityIndexMap.end()) DrawEntityNode(child, it->second);
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
            ImGui::PopID();
        };

        // --- Multi-scene Hierarchy ---
        const std::vector<std::string> allScenes    = pm.IsProjectOpen() ? pm.GetSceneList() : std::vector<std::string>{};
        const std::string              currentStem  = pm.GetCurrentSceneName();
        const std::string              currentFile  = currentStem + ".scene";

        // Active scene node — expanded by default, blue tint
        {
            ImGuiTreeNodeFlags sceneFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.82f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 5));
            bool sceneOpen = ImGui::TreeNodeEx("##ActiveScene", sceneFlags, "%s  %s", ICON_FA_FOLDER_OPEN, currentStem.empty() ? "Scene" : currentStem.c_str());
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            // Right-click on active scene node
            if (ImGui::BeginPopupContextItem("ActiveSceneCtx"))
            {
                ImGui::TextDisabled("%s", currentStem.empty() ? "Scene" : currentStem.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  Save Scene", "Ctrl+S"))
                {
                    pm.SaveCurrentProject(scene);
                    isDirty = false;
                }
                if (ImGui::MenuItem("Rename Scene..."))
                {
                    sceneRenameTarget = currentFile;
                    strncpy(sceneRenameBuffer, currentStem.c_str(), sizeof(sceneRenameBuffer) - 1);
                    showRenameSceneModal_ = true;
                }
                if (ImGui::MenuItem("New Scene...")) showNewSceneModal_ = true;
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                if (ImGui::MenuItem("Delete Scene...", nullptr, false, allScenes.size() > 1))
                {
                    sceneDeleteTarget = currentFile;
                    showDeleteSceneModal_ = true;
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            if (sceneOpen)
            {
                if (hSearching)
                {
                    // Flat, filtered view — ignores hierarchy so matches anywhere show up.
                    bool anyMatch = false;
                    for (int i = 0; i < (int)scene.entities.size(); i++)
                    {
                        Entity* e = scene.entities[i].get();
                        if (!nameMatches(e->name)) continue;
                        anyMatch = true;

                        ImGui::PushID(e->id);
                        if (drawRename(e)) { ImGui::PopID(); continue; }
                        char label[256];
                        snprintf(label, sizeof(label), "%s  %s", iconFor(e), e->name.c_str());
                        if (ImGui::Selectable(label, selectedIndex == i)) { selectedIndex = i; multiSelection_.clear(); }
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) beginRename(e);
                        ImGui::PopID();
                    }
                    if (!anyMatch) ImGui::TextDisabled("  No entities match.");
                    ImGui::TreePop();
                }
                else
                {
                // Root entities of the active scene
                for (int i = 0; i < (int)scene.entities.size(); i++)
                {
                    if (scene.entities[i]->parent == nullptr) { DrawEntityNode(scene.entities[i].get(), i); }
                }

                // Drop onto scene node = unparent
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID"))
                    {
                        int draggedId = *(const int*)payload->Data;
                        Entity* dragged = scene.FindEntity(draggedId);
                        if (dragged && dragged->parent)
                        {
                            TakeSnapshot();
                            Vector2 gPos = dragged->getGlobalPosition();
                            dragged->setParent(nullptr);
                            dragged->position = gPos;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TreePop();
                } // else (non-search tree)
            }
        }

        // Other (unloaded) scene nodes — greyed out leaf nodes
        for (const auto& sceneFile : allScenes)
        {
            if (sceneFile == currentFile) continue;

            const std::string stemName = fs::path(sceneFile).stem().string();

            ImGuiTreeNodeFlags leafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 5));
            ImGui::TreeNodeEx(sceneFile.c_str(), leafFlags, "%s  %s", ICON_FA_FILE, stemName.c_str());
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            // Double-click to switch to this scene
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && state == GameState::Editor)
            {
                pm.SaveCurrentProject(scene);
                isDirty = false;
                if (pm.SwitchScene(sceneFile, scene))
                {
                    selectedIndex = -1;
                    undoStack.clear();
                    redoStack.clear();
                }
            }

            // Right-click on inactive scene node
            if (ImGui::BeginPopupContextItem())
            {
                ImGui::TextDisabled("%s", stemName.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Load Scene") && state == GameState::Editor)
                {
                    pm.SaveCurrentProject(scene);
                    isDirty = false;
                    if (pm.SwitchScene(sceneFile, scene))
                    {
                        selectedIndex = -1;
                        undoStack.clear();
                        redoStack.clear();
                    }
                }
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                if (ImGui::MenuItem("Delete Scene...")) { sceneDeleteTarget     = sceneFile; showDeleteSceneModal_ = true; }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }
        }

        // Execute deletion safely outside the loop
        if (!entitiesToDelete.empty())
        {
            TakeSnapshot();
            DeleteEntitiesAt(entitiesToDelete);
        }

        // Save as Prefab modal
        if (showSavePrefabModal_) { ImGui::OpenPopup("SavePrefabModal##H"); showSavePrefabModal_ = false; }
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("SavePrefabModal##H", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("Prefab Name:");
            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputText("##PrefabName", prefabNameBuf_, sizeof(prefabNameBuf_));
            ImGui::Spacing();
            if (ImGui::Button("Save", ImVec2(126, 0)))
            {
                if (prefabNameBuf_[0] != '\0' && prefabSourceIndex_ >= 0 && prefabSourceIndex_ < (int)scene.entities.size())
                {
                    nlohmann::json j = scene.entities[prefabSourceIndex_]->serialize();
                    if (PrefabManager::Save(j, pm.GetCurrentProjectPath(), prefabNameBuf_))
                    {
                        consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO] ", std::string("Prefab saved: ") + prefabNameBuf_ + ".prefab", ICON_FA_BOX});
                    }
                    else consoleLogs.push_back({ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "[ERROR]", std::string("Failed to save prefab: ") + prefabNameBuf_, ICON_FA_TRIANGLE_EXCLAMATION});
                }
                prefabSourceIndex_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(126, 0))) { prefabSourceIndex_ = -1; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        // --- Keyboard Shortcuts ---
        if (!ImGui::GetIO().WantTextInput && state == GameState::Editor)
        {
            bool hasSel    = selectedIndex >= 0 && selectedIndex < (int)scene.entities.size();
            bool hasMulti  = multiSelection_.size() > 1;
            bool hasAny    = hasSel || hasMulti;

            // F2 — rename the selected entity in place
            if (hasSel && IsKeyPressed(KEY_F2)) beginRename(scene.entities[selectedIndex].get());

            // Cut
            if (hasAny && CtrlDown() && IsKeyPressed(KEY_X))
            {
                TakeSnapshot();
                CopySelected();
                if (hasMulti)
                {
                    DeleteEntitiesAt(multiSelection_);
                    multiSelection_.clear();
                }
                else DeleteEntity(*scene.entities[selectedIndex]);
            }

            // Copy
            if (hasAny && CtrlDown() && IsKeyPressed(KEY_C)) CopySelected();

            // Duplicate
            if (hasAny && CtrlDown() && IsKeyPressed(KEY_D)) DuplicateSelected(selectedIndex);

            // Paste
            bool canPasteKb = !entityClipboard.is_null() || !multiClipboard_.empty();
            if (canPasteKb && CtrlDown() && IsKeyPressed(KEY_V)) PasteAt(selectedIndex >= 0 ? scene.entities[selectedIndex]->position : editorCamera.target);

            // Delete
            if (hasAny && (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)))

            {
                TakeSnapshot();
                if (hasMulti)
                {
                    DeleteEntitiesAt(multiSelection_);
                    multiSelection_.clear();
                }
                else DeleteEntity(*scene.entities[selectedIndex]);
            }
        }

        // Right-click context menu for empty space in the Hierarchy
        if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::BeginMenu("2D Object"))
            {
                if (ImGui::MenuItem(ICON_FA_CIRCLE "  Circle"))           CreateEntityAt("Circle",    editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_VECTOR_SQUARE "  Rectangle")) CreateEntityAt("Rectangle", editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_LAYER_GROUP "  Surface"))     CreateEntityAt("Surface",   editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_IMAGE "  Image (Sprite)"))    CreateEntityAt("Sprite",    editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_TABLE_CELLS "  Tilemap"))     CreateEntityAt("Tilemap",   editorCamera.target);
                if (ImGui::MenuItem(ICON_FA_CAMERA "  Camera"))           CreateEntityAt("Camera",    editorCamera.target);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            {
                bool canPasteEmpty = !entityClipboard.is_null() || !multiClipboard_.empty();
                if (ImGui::MenuItem("Paste", "Ctrl+V", false, canPasteEmpty)) PasteAt(editorCamera.target);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Deselect All", nullptr, false, selectedIndex >= 0)) selectedIndex = -1;
            ImGui::EndPopup();
        }
        ImGui::End();
    }
}
