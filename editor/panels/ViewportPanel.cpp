#include "../Editor.hpp"

namespace Indium
{
    void Editor::ShowViewport()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // --- Scene / Game tab bar ---
        ImGui::SetCursorPosX(8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
        if (ImGui::BeginTabBar("##ViewTabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem(ICON_FA_PENCIL "  Scene"))
            {
                viewportTab_ = 0;
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(ICON_FA_GAMEPAD "  Game"))
            {
                viewportTab_ = 1;
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::PopStyleVar(); // FramePadding

        // --- Transform tool toolbar ---
        if (viewportTab_ == 0 && state == GameState::Editor)
        {
            ImGui::SetCursorPosX(6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 3));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(3, 0));

            auto toolBtn = [&](const char* icon, TransformTool tool, const char* tip)
            {
                bool active = (activeTool_ == tool);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.76f, 1.0f));
                if (ImGui::Button(icon, ImVec2(26, 20))) activeTool_ = tool;
                if (active) ImGui::PopStyleColor();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tip);
                ImGui::SameLine();
            };

            toolBtn(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, TransformTool::Move,      "Move  [W]");
            toolBtn(ICON_FA_ROTATE,                    TransformTool::Rotate,    "Rotate  [E]");
            toolBtn(ICON_FA_EXPAND,                    TransformTool::Rect,      "Rect Transform  [R]");
            toolBtn(ICON_FA_WAND_MAGIC_SPARKLES,       TransformTool::Universal, "Universal  [Y]");

            // Separator
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            // Grid toggle
            bool gridWasOn = showGrid_;
            if (gridWasOn) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.76f, 1.0f));
            if (ImGui::Button(ICON_FA_TABLE_CELLS, ImVec2(26, 20))) showGrid_ = !showGrid_;
            if (gridWasOn) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Toggle Grid  [G]");
            ImGui::SameLine();

            // Snap toggle
            bool snapWasOn = snapEnabled_;
            if (snapWasOn) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.76f, 1.0f));
            if (ImGui::Button(ICON_FA_MAGNET, ImVec2(26, 20))) snapEnabled_ = !snapEnabled_;
            if (snapWasOn) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Toggle Snap  [S]");
            ImGui::SameLine();

            // Snap size + rotation snap inputs
            if (snapEnabled_)
            {
                ImGui::SetNextItemWidth(52.0f);
                ImGui::DragFloat("##SnapSz", &snapSize_, 1.0f, 1.0f, 512.0f, "%.0f");
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Grid snap size (world units)");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(52.0f);
                ImGui::DragFloat("##RotSnap", &rotSnapDegrees_, 1.0f, 0.0f, 180.0f, "%.0f\xc2\xb0");
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Rotation snap (degrees, 0 = off)");
                ImGui::SameLine();
            }

            ImGui::PopStyleVar(2);
            ImGui::Separator();
        }

        ImVec2 viewportAvail = ImGui::GetContentRegionAvail();
        ImVec2 renderArea = ImVec2(std::max(viewportAvail.x, 1.0f), std::max(viewportAvail.y, 1.0f));

        ImGui::BeginChild("ViewportRender", renderArea, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Capture the viewport's position and size for mouse coordinate mapping
        {
            ImVec2 csp   = ImGui::GetCursorScreenPos();
            ImVec2 avail = ImGui::GetContentRegionAvail();
            viewportPos.x  = csp.x;
            viewportPos.y  = csp.y;
            viewportSize.x = avail.x;
            viewportSize.y = avail.y;
        }
        // With docking, the viewport is its own window — its hover state is authoritative.
        viewportHovered     = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        rlImGuiImageRenderTextureFit(&viewport, false);

        // Prefab drop onto viewport
        if (state == GameState::Editor && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                std::string relPath((const char*)payload->Data, payload->DataSize - 1);
                fs::path droppedPath = fs::path(pm.GetCurrentProjectPath()) / relPath;
                if (droppedPath.extension() == ".prefab" && pm.IsProjectOpen())
                {
                    nlohmann::json j = PrefabManager::Load(droppedPath.string());
                    if (!j.is_null())
                    {
                        TakeSnapshot();
                        auto e = factory.LoadEntity(j);
                        if (e)
                        {
                            e->id       = scene.nextEntityId++;
                            e->position = worldMouse;
                            scene.entities.push_back(std::move(e));
                            selectedIndex = (int)scene.entities.size() - 1;
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (viewportHovered)
        {
            // Update worldMouse accurately based on current frame viewport data
            Camera2D activeCamera = GetActiveCamera();
            Vector2 screenMouse   = GetMousePosition();

            float scaleX = (viewportSize.x > 0) ? (float)viewport.texture.width  / viewportSize.x : 1.0f;
            float scaleY = (viewportSize.y > 0) ? (float)viewport.texture.height / viewportSize.y : 1.0f;

            Vector2 scaledMouse =
            {
                (screenMouse.x - viewportPos.x) * scaleX,
                (screenMouse.y - viewportPos.y) * scaleY
            };

            worldMouse = GetScreenToWorld2D(scaledMouse, activeCamera);

            // 1. Camera Panning (Middle Mouse)
            if (state == GameState::Editor && IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
            {
                Vector2 delta = GetMouseDelta();
                delta = Vector2Scale(delta, -1.0f / editorCamera.zoom);
                editorCamera.target = Vector2Add(editorCamera.target, delta);
            }

            // 2. Camera Zoom (Mouse Wheel) — zooms toward cursor position
            if (state == GameState::Editor)
            {
                float wheel = GetMouseWheelMove();
                if (wheel != 0.0f)
                {
                    float oldZoom = editorCamera.zoom;
                    float newZoom = Clamp(oldZoom * (1.0f + wheel * 0.1f), 0.05f, 32.0f);
                    // Keep the world point under the cursor stationary during zoom
                    editorCamera.target.x = worldMouse.x - (worldMouse.x - editorCamera.target.x) * (oldZoom / newZoom);
                    editorCamera.target.y = worldMouse.y - (worldMouse.y - editorCamera.target.y) * (oldZoom / newZoom);
                    editorCamera.zoom = newZoom;
                }
            }

            // Tool hotkeys
            if (state == GameState::Editor && !ImGui::GetIO().WantTextInput)
            {
                if (IsKeyPressed(KEY_W)) activeTool_ = TransformTool::Move;
                if (IsKeyPressed(KEY_E)) activeTool_ = TransformTool::Rotate;
                if (IsKeyPressed(KEY_R)) activeTool_ = TransformTool::Rect;
                if (IsKeyPressed(KEY_Y)) activeTool_ = TransformTool::Universal;
                if (IsKeyPressed(KEY_G)) showGrid_    = !showGrid_;
                if (IsKeyPressed(KEY_S) && !CtrlDown()) snapEnabled_ = !snapEnabled_;

                // F — frame (focus + zoom-to-fit) the selected entity, like Unity.
                if (IsKeyPressed(KEY_F) && selectedIndex >= 0 && selectedIndex < (int)scene.entities.size())
                {
                    Entity* sel = scene.entities[selectedIndex].get();
                    ::Rectangle b = sel->getBounds();
                    editorCamera.target = { b.x + b.width * 0.5f, b.y + b.height * 0.5f };
                    if (b.width > 1.0f && b.height > 1.0f && viewportSize.x > 1.0f)
                    {
                        float zx = viewportSize.x / (b.width  * 1.6f);
                        float zy = viewportSize.y / (b.height * 1.6f);
                        editorCamera.zoom = Clamp(std::min(zx, zy), 0.05f, 32.0f);
                    }
                }
            }

            // Sorted pick order (draw order = visual depth, used for click priority).
            // Built once per hovered frame and shared by left-click and right-click handlers.
            std::vector<int> sortedPickOrder((int)scene.entities.size());
            std::iota(sortedPickOrder.begin(), sortedPickOrder.end(), 0);
            std::sort(sortedPickOrder.begin(), sortedPickOrder.end(), [&](int a, int b)
            {
                return scene.entities[a]->computeSortKey() < scene.entities[b]->computeSortKey();
            });

            // 3. Left-Click — selection, handle start, body drag
            if (viewportTab_ == 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                draggingEntity = nullptr;
                activeHandle_  = HandleType::None;
                isSelectingBox = false;
                bool handleHit = false;

                // First, if an entity is already selected, try to hit its handles.
                // This ensures handles outside the entity's body (like rotation handle)
                // can be clicked without deselecting the entity.
                if (selectedIndex >= 0 && selectedIndex < (int)scene.entities.size() && state == GameState::Editor)
                {
                    Entity* sel    = scene.entities[selectedIndex].get();
                    Vector2 center = sel->getGlobalPosition();
                    float   rot    = sel->getGlobalRotation();
                    float   hw     = sel->scale.x / 2.0f;
                    float   hh     = sel->scale.y / 2.0f;
                    float   HR     = 10.0f / editorCamera.zoom;

                    auto toWorld = [&](float lx, float ly) -> Vector2
                    {
                        float rad  = rot * DEG2RAD;
                        float c    = cosf(rad);
                        float s    = sinf(rad);
                        return {center.x + lx*c - ly*s, center.y + lx*s + ly*c};
                    };

                    // Rect handles (Rect or Universal mode) — disabled for parented entities
                    if (!handleHit && (activeTool_ == TransformTool::Rect || activeTool_ == TransformTool::Universal) && !sel->getVertices().empty())
                    {
                        if (sel->parentId == -1) // scale handles only work on root entities
                        {
                            Vector2 hpts[8] =
                            {
                                toWorld(-hw,-hh), toWorld(0,-hh), toWorld(+hw,-hh), toWorld(+hw,0),
                                toWorld(+hw,+hh), toWorld(0,+hh), toWorld(-hw,+hh), toWorld(-hw,0)
                            };
                            HandleType htypes[8] =
                            {
                                HandleType::H_TL, HandleType::H_TM, HandleType::H_TR, HandleType::H_RM,
                                HandleType::H_BR, HandleType::H_BM, HandleType::H_BL, HandleType::H_LM
                            };
                            for (int k = 0; k < 8; k++)
                            {
                                if (Vector2Distance(worldMouse, hpts[k]) <= HR)
                                {
                                    TakeSnapshot();
                                    isDirty                   = true;
                                    activeHandle_             = htypes[k];
                                    handleDragStartMouse_     = worldMouse;
                                    handleDragStartGlobalPos_ = center;
                                    handleDragStartScale_     = sel->scale;
                                    handleDragStartRot_       = rot;
                                    handleHit                 = true;
                                    break;
                                }
                            }
                        }
                    }

                    // Rotation handle (Rotate or Universal mode)
                    if (!handleHit && (activeTool_ == TransformTool::Rotate || activeTool_ == TransformTool::Universal))
                    {
                        float gizmoR = fmaxf(hw, hh) + 28.0f / editorCamera.zoom;
                        float thresh = 12.0f / editorCamera.zoom;
                        if (fabsf(Vector2Distance(worldMouse, center) - gizmoR) <= thresh)
                        {
                            TakeSnapshot(); isDirty = true;
                            activeHandle_             = HandleType::H_Rotate;
                            handleDragStartMouse_     = worldMouse;
                            handleDragStartGlobalPos_ = center;
                            handleDragStartRot_       = sel->getGlobalRotation();
                            handleHit = true;
                        }
                    }
                }

                // If no handle was clicked, perform normal entity picking
                if (!handleHit)
                {
                    const auto& pickOrder = sortedPickOrder;
                    int clickedIndex = -1;
                    for (int pi = (int)pickOrder.size() - 1; pi >= 0; pi--)
                    {
                        if (scene.entities[pickOrder[pi]]->Contains(worldMouse)) { clickedIndex = pickOrder[pi]; break; }
                    }

                    bool ctrlHeld = CtrlDown() || IsKeyDown(KEY_RIGHT_CONTROL);

                    if (clickedIndex == -1)
                    {
                        // Empty space: deselect all + begin box select
                        if (!ctrlHeld)
                        {
                            selectedIndex = -1;
                            multiSelection_.clear();
                        }
                        if (state == GameState::Editor) { isSelectingBox = true; selectBoxStart = worldMouse; }
                    }
                    else if (ctrlHeld && state == GameState::Editor)
                    {
                        // Ctrl+Click: toggle entity in multi-selection
                        auto it = std::find(multiSelection_.begin(), multiSelection_.end(), clickedIndex);
                        if (it != multiSelection_.end()) multiSelection_.erase(it);
                        else multiSelection_.push_back(clickedIndex);
                        selectedIndex = clickedIndex; // inspector shows last clicked
                    }
                    else if (clickedIndex != selectedIndex)
                    {
                        bool inMulti = std::find(multiSelection_.begin(), multiSelection_.end(), clickedIndex) != multiSelection_.end();
                        if (inMulti && multiSelection_.size() > 1 && state == GameState::Editor && activeTool_ == TransformTool::Move)
                        {
                            // Clicked a multi-selected entity that isn't the primary — start multi-drag
                            selectedIndex = clickedIndex;
                            TakeSnapshot();
                            multiDragStartMouse_ = worldMouse;
                            multiDragStartPos_.clear();
                            for (int idx : multiSelection_) multiDragStartPos_.push_back(scene.entities[idx]->position);
                        }
                        else
                        {
                            multiSelection_.clear();
                            selectedIndex = clickedIndex;
                            if (state == GameState::Editor && activeTool_ == TransformTool::Move)
                            {
                                TakeSnapshot();
                                draggingEntity = scene.entities[clickedIndex].get();
                                dragOffset     = {draggingEntity->position.x - worldMouse.x, draggingEntity->position.y - worldMouse.y};
                            }
                        }
                    }
                    else if (state == GameState::Editor)
                    {
                        // Clicked same entity — body drag (single) or begin multi-drag
                        if (multiSelection_.size() > 1)
                        {
                            // Start multi-drag
                            TakeSnapshot();
                            multiDragStartMouse_ = worldMouse;
                            multiDragStartPos_.clear();
                            for (int idx : multiSelection_) multiDragStartPos_.push_back(scene.entities[idx]->position);
                        }
                        else
                        {
                            Entity* sel = scene.entities[selectedIndex].get();
                            TakeSnapshot();
                            activeHandle_  = HandleType::Body;
                            draggingEntity = sel;
                            dragOffset     = {sel->position.x - worldMouse.x, sel->position.y - worldMouse.y};
                        }
                    }
                }
            }

            // 4. Context Menu (Right Click) — only in Scene tab
            if (viewportTab_ == 0 && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
            {
                contextEntityIndex = -1;
                const auto& pickOrder = sortedPickOrder;
                for (int pi = (int)pickOrder.size() - 1; pi >= 0; pi--)
                {
                    int i = pickOrder[pi];
                    if (scene.entities[i]->Contains(worldMouse))
                    {
                        contextEntityIndex = i;
                        selectedIndex      = i;
                        break;
                    }
                }
            }

            // 5. Dragging / Handle update
            if (viewportTab_ == 0)
            {
                // 5a. Body move drag (single entity)
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggingEntity != nullptr)
                {
                    float rawX = worldMouse.x + dragOffset.x;
                    float rawY = worldMouse.y + dragOffset.y;
                    if (snapEnabled_ && snapSize_ > 0.0f)
                    {
                        rawX = roundf(rawX / snapSize_) * snapSize_;
                        rawY = roundf(rawY / snapSize_) * snapSize_;
                    }
                    draggingEntity->position.x = rawX;
                    draggingEntity->position.y = rawY;
                    isDirty = true;
                }

                // 5a2. Multi-entity drag
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !multiSelection_.empty()
                    && !multiDragStartPos_.empty() && draggingEntity == nullptr)
                {
                    Vector2 delta = {worldMouse.x - multiDragStartMouse_.x,
                                     worldMouse.y - multiDragStartMouse_.y};
                    for (int i = 0; i < (int)multiSelection_.size() && i < (int)multiDragStartPos_.size(); ++i)
                    {
                        int idx = multiSelection_[i];
                        if (idx < 0 || idx >= (int)scene.entities.size()) continue;
                        float rawX = multiDragStartPos_[i].x + delta.x;
                        float rawY = multiDragStartPos_[i].y + delta.y;
                        if (snapEnabled_ && snapSize_ > 0.0f)
                        {
                            rawX = roundf(rawX / snapSize_) * snapSize_;
                            rawY = roundf(rawY / snapSize_) * snapSize_;
                        }
                        scene.entities[idx]->position = {rawX, rawY};
                    }
                    isDirty = true;
                }
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) multiDragStartPos_.clear();

                // 5b. Handle drag (Rect / Rotate tools)
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && activeHandle_ != HandleType::None && activeHandle_ != HandleType::Body && selectedIndex >= 0 && selectedIndex < (int)scene.entities.size())
                {
                    Entity* sel = scene.entities[selectedIndex].get();
                    Vector2 P   = handleDragStartGlobalPos_;
                    float   rot = handleDragStartRot_ * DEG2RAD;
                    float   hw  = handleDragStartScale_.x / 2.0f;
                    float   hh  = handleDragStartScale_.y / 2.0f;

                    // Mouse position in entity-local space (relative to drag-start center)
                    float dx = worldMouse.x - P.x;
                    float dy = worldMouse.y - P.y;
                    float uc = cosf(-rot), us = sinf(-rot);
                    float mx = dx * uc - dy * us;  // local X
                    float my = dx * us + dy * uc;  // local Y

                    // Apply new scale and recompute center from local offset
                    auto applyResize = [&](float newW, float newH, float localCX, float localCY)
                    {
                        if (sel->parentId != -1) return;  // skip for parented entities
                        sel->scale.x    = std::max(newW, 2.0f);
                        sel->scale.y    = std::max(newH, 2.0f);
                        float rc        = cosf(handleDragStartRot_ * DEG2RAD);
                        float rs        = sinf(handleDragStartRot_ * DEG2RAD);
                        sel->position   = {P.x + localCX * rc - localCY * rs, P.y + localCX * rs + localCY * rc};
                        isDirty = true;
                    };

                    switch (activeHandle_)
                    {
                        // Corners (opposite corner stays fixed in world space)
                        case HandleType::H_TL: applyResize(fabsf(hw-mx), fabsf(hh-my), (hw+mx)/2.f, (hh+my)/2.f); break;
                        case HandleType::H_TR: applyResize(fabsf(mx+hw), fabsf(hh-my), (-hw+mx)/2.f,(hh+my)/2.f); break;
                        case HandleType::H_BR: applyResize(fabsf(mx+hw), fabsf(my+hh), (-hw+mx)/2.f,(-hh+my)/2.f); break;
                        case HandleType::H_BL: applyResize(fabsf(hw-mx), fabsf(my+hh), (hw+mx)/2.f, (-hh+my)/2.f); break;
                        // Edges (one axis locked)
                        case HandleType::H_TM: applyResize(hw*2.f, fabsf(hh-my), 0.f, (hh+my)/2.f); break;
                        case HandleType::H_BM: applyResize(hw*2.f, fabsf(my+hh), 0.f, (-hh+my)/2.f); break;
                        case HandleType::H_LM: applyResize(fabsf(hw-mx), hh*2.f, (hw+mx)/2.f, 0.f); break;
                        case HandleType::H_RM: applyResize(fabsf(mx+hw), hh*2.f, (-hw+mx)/2.f, 0.f); break;
                        // Rotation
                        case HandleType::H_Rotate:
                        {
                            float a0           = atan2f(handleDragStartMouse_.y - P.y, handleDragStartMouse_.x - P.x);
                            float a1           = atan2f(worldMouse.y - P.y, worldMouse.x - P.x);
                            float newGlobalRot = handleDragStartRot_ + (a1 - a0) * RAD2DEG;
                            // Rotation snap — round to the nearest increment when snap is on.
                            if (snapEnabled_ && rotSnapDegrees_ > 0.0f)
                                newGlobalRot = roundf(newGlobalRot / rotSnapDegrees_) * rotSnapDegrees_;
                            float parentRot    = sel->parent ? sel->parent->getGlobalRotation() : 0.0f;
                            sel->rotation      = newGlobalRot - parentRot;
                            isDirty            = true;
                            break;
                        }
                        default: break;
                    }
                }

                // 5c. Release
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
                {
                    draggingEntity = nullptr;
                    activeHandle_  = HandleType::None;
                    if (isSelectingBox)
                    {
                        isSelectingBox = false;
                        Vector2 p1 = selectBoxStart;
                        Vector2 p2 = worldMouse;
                        float x = std::min(p1.x, p2.x);
                        float y = std::min(p1.y, p2.y);
                        float w = std::abs(p1.x - p2.x);
                        float h = std::abs(p1.y - p2.y);
                        if (w > 2.0f && h > 2.0f)
                        {
                            ::Rectangle r = {x, y, w, h};
                            multiSelection_.clear();
                            for (int i = 0; i < (int)scene.entities.size(); i++) { if (CheckCollisionRecs(r, scene.entities[i]->getBounds())) multiSelection_.push_back(i); }
                            if (!multiSelection_.empty()) selectedIndex = multiSelection_.back();
                            else selectedIndex = -1;
                        }
                    }
                }
            }
        }

        // Right-click Context Menu for Viewport
        // Restore proper padding — viewport window uses {0,0} for seamless rendering,
        // which would make the popup menu text flush against the edges without this override.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
        if (ImGui::BeginPopupContextWindow("ViewportContext", ImGuiPopupFlags_MouseButtonRight))
        {
            if(state != GameState::Play)
            {
                if (contextEntityIndex != -1)
                {
                    Entity* contextEntity = scene.entities[contextEntityIndex].get();
                    ImGui::TextDisabled("Entity: %s", contextEntity->name.c_str());
                    ImGui::Separator();
                    if (ImGui::MenuItem("Focus")) editorCamera.target = contextEntity->getGlobalPosition();
                    ImGui::Separator();

                    {
                        bool hm = multiSelection_.size() > 1; // has multi selection?
                        if (ImGui::MenuItem("Cut", "Ctrl+X"))
                        {
                            TakeSnapshot();
                            CopySelected();
                            if (hm)
                            {
                                DeleteEntitiesAt(multiSelection_);
                                multiSelection_.clear();
                            }
                            else DeleteEntity(*contextEntity);
                            contextEntityIndex = -1;
                        }
                        if (ImGui::MenuItem("Copy", "Ctrl+C")) CopySelected();
                        bool canPasteV = !entityClipboard.is_null() || !multiClipboard_.empty();
                        if (ImGui::MenuItem("Paste", "Ctrl+V", false, canPasteV)) PasteAt(Vector2Add(contextEntity->position, {16.0f, 16.0f}));
                        if (ImGui::MenuItem("Duplicate", "Ctrl+D"))               DuplicateSelected(contextEntityIndex);
                        ImGui::Separator();
                        if (ImGui::MenuItem("Delete", "Del"))
                        {
                            TakeSnapshot();
                            if (hm)
                            {
                                DeleteEntitiesAt(multiSelection_);
                                multiSelection_.clear();
                            }
                            else DeleteEntity(*contextEntity);
                        }
                    }
                }
                else
                {
                    if (ImGui::BeginMenu("Create"))
                    {
                        if (ImGui::MenuItem(ICON_FA_CUBE "  Empty"))              CreateEntityAt("Empty",          worldMouse);
                        ImGui::Separator();
                        if (ImGui::BeginMenu(ICON_FA_VECTOR_SQUARE "  2D Object"))
                        {
                            if (ImGui::MenuItem(ICON_FA_CIRCLE "  Circle"))           CreateEntityAt("Circle",    worldMouse);
                            if (ImGui::MenuItem(ICON_FA_VECTOR_SQUARE "  Rectangle")) CreateEntityAt("Rectangle", worldMouse);
                            if (ImGui::MenuItem(ICON_FA_LAYER_GROUP "  Surface"))     CreateEntityAt("Surface",   worldMouse);
                            if (ImGui::MenuItem(ICON_FA_IMAGE "  Image (Sprite)"))    CreateEntityAt("Sprite",    worldMouse);
                            ImGui::EndMenu();
                        }
                        if (ImGui::MenuItem(ICON_FA_FONT "  Text"))               CreateEntityAt("Text",           worldMouse);
                        if (ImGui::MenuItem(ICON_FA_LIGHTBULB "  Light 2D"))      CreateEntityAt("Light",          worldMouse);
                        if (ImGui::MenuItem(ICON_FA_CAMERA "  Camera"))           CreateEntityAt("Camera",         worldMouse);
                        ImGui::Separator();
                        if (ImGui::MenuItem(ICON_FA_STAR "  Particle System"))    CreateEntityAt("ParticleSystem", worldMouse);
                        if (ImGui::MenuItem(ICON_FA_TABLE_CELLS "  Tilemap"))     CreateEntityAt("Tilemap",        worldMouse);
                        if (ImGui::MenuItem(ICON_FA_VOLUME_HIGH "  Audio Source")) CreateEntityAt("AudioSource",   worldMouse);
                        ImGui::Separator();
                        if (ImGui::MenuItem(ICON_FA_VECTOR_SQUARE "  Trigger Zone")) CreateEntityAt("TriggerZone", worldMouse);
                        if (ImGui::MenuItem(ICON_FA_LOCATION_DOT "  Spawn Point"))   CreateEntityAt("SpawnPoint",  worldMouse);
                        if (ImGui::MenuItem(ICON_FA_FLAG "  Checkpoint"))            CreateEntityAt("Checkpoint",  worldMouse);
                        ImGui::EndMenu();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoStack.empty()))               Undo();
                    if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, !redoStack.empty()))         Redo();
                    ImGui::Separator();
                    if (ImGui::MenuItem("Paste", "Ctrl+V", false, !entityClipboard.is_null() || !multiClipboard_.empty())) PasteAt(worldMouse);
                    ImGui::Separator();
                    bool hasEntities = !scene.entities.empty();
                    if (ImGui::MenuItem("Select All", nullptr, false, hasEntities))
                    {
                        multiSelection_.clear();
                        for (int i = 0; i < (int)scene.entities.size(); i++) multiSelection_.push_back(i);
                        if (!multiSelection_.empty()) selectedIndex = multiSelection_.back();
                    }
                    if (ImGui::MenuItem("Deselect All", nullptr, false, selectedIndex >= 0 || !multiSelection_.empty()))
                    {
                        selectedIndex = -1;
                        multiSelection_.clear();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Frame All", nullptr, false, hasEntities))
                    {
                        float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
                        for (const auto& e : scene.entities)
                        {
                            auto b = e->getBounds();
                            minX = std::min(minX, b.x);           minY = std::min(minY, b.y);
                            maxX = std::max(maxX, b.x + b.width); maxY = std::max(maxY, b.y + b.height);
                        }
                        editorCamera.target = {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
                        float zoomX = viewportSize.x / std::max(maxX - minX, 1.0f) * 0.85f;
                        float zoomY = viewportSize.y / std::max(maxY - minY, 1.0f) * 0.85f;
                        editorCamera.zoom = Clamp(std::min(zoomX, zoomY), 0.05f, 32.0f);
                    }
                    if (ImGui::MenuItem("Reset View"))
                    {
                        editorCamera.target = {0, 0};
                        editorCamera.zoom = 1.0f;
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem(showGrid_ ? "Hide Grid" : "Show Grid", "G")) showGrid_ = !showGrid_;
                    if (ImGui::MenuItem(snapEnabled_ ? "Disable Snap" : "Enable Snap"))            snapEnabled_ = !snapEnabled_;
                }
            }
            else
            {
                ImGui::TextDisabled("No actions available in Play Mode");
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(); // restore WindowPadding from viewport override

        // --- Viewport status overlay (bottom-left) + drag HUD (near cursor) ---
        if (viewportTab_ == 0 && state == GameState::Editor && viewportSize.x > 1.0f)
        {
            ImDrawList* dl = ImGui::GetForegroundDrawList();

            // Status line
            const char* selName = (selectedIndex >= 0 && selectedIndex < (int)scene.entities.size())
                                ? scene.entities[selectedIndex]->name.c_str() : "-";
            char status[320];
            snprintf(status, sizeof(status),
                     "Zoom %.0f%%   Mouse (%.0f, %.0f)   Sel: %s   Entities: %d",
                     editorCamera.zoom * 100.0f, worldMouse.x, worldMouse.y, selName, (int)scene.entities.size());
            ImVec2 ts = ImGui::CalcTextSize(status);
            float sx = viewportPos.x + 8.0f;
            float sy = viewportPos.y + viewportSize.y - ts.y - 8.0f;
            dl->AddRectFilled(ImVec2(sx - 4, sy - 2), ImVec2(sx + ts.x + 4, sy + ts.y + 2), IM_COL32(0, 0, 0, 150), 4.0f);
            dl->AddText(ImVec2(sx, sy), IM_COL32(205, 205, 205, 255), status);

            // Drag HUD — numeric readout next to the cursor while transforming.
            std::string hud;
            Entity* sel = (selectedIndex >= 0 && selectedIndex < (int)scene.entities.size())
                        ? scene.entities[selectedIndex].get() : nullptr;
            if (draggingEntity)
                hud = TextFormat("X %.0f   Y %.0f", draggingEntity->position.x, draggingEntity->position.y);
            else if (sel && activeHandle_ == HandleType::H_Rotate)
                hud = TextFormat("%.1f deg", sel->getGlobalRotation());
            else if (sel && activeHandle_ != HandleType::None && activeHandle_ != HandleType::Body)
                hud = TextFormat("W %.0f   H %.0f", sel->scale.x, sel->scale.y);

            if (!hud.empty())
            {
                Vector2 m = GetMousePosition();
                ImVec2 hts = ImGui::CalcTextSize(hud.c_str());
                ImVec2 hp  = ImVec2(m.x + 18.0f, m.y + 8.0f);
                dl->AddRectFilled(ImVec2(hp.x - 5, hp.y - 3), ImVec2(hp.x + hts.x + 5, hp.y + hts.y + 3), IM_COL32(20, 20, 22, 220), 4.0f);
                dl->AddText(hp, IM_COL32(255, 230, 130, 255), hud.c_str());
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::End();
    }
}
