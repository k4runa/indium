#include "../Editor.hpp"

namespace Indium
{
    // Catalog of every built-in component, grouped by category. Adding a new
    // component to the editor = one line here (keeps the menu complete & ordered).
    // The factory lambda performs the add on the currently selected entity.
    struct ComponentEntry
    {
        const char* name;
        std::function<void(Entity*)> add;
    };
    struct ComponentCategory
    {
        const char* name;
        std::vector<ComponentEntry> items;
    };

    static std::vector<ComponentCategory> BuildComponentCatalog()
    {
        return {
            { "Rendering", {
                { "Shape Renderer",   [](Entity* e){ e->addComponent<ShapeRendererComponent>(); } },
                { "Sprite Renderer",  [](Entity* e){ e->addComponent<SpriteRendererComponent>(); } },
                { "Text Renderer",    [](Entity* e){ e->addComponent<TextRendererComponent>(); } },
                { "Particle System",  [](Entity* e){ e->addComponent<ParticleSystemComponent>(); } },
                { "Tilemap",          [](Entity* e){ e->addComponent<TilemapComponent>(); } },
                { "Line Renderer",    [](Entity* e){ e->addComponent<LineRendererComponent>(); } },
                { "Trail Renderer",   [](Entity* e){ e->addComponent<TrailRendererComponent>(); } },
                { "Decal",            [](Entity* e){ e->addComponent<DecalComponent>(); } },
                { "Sprite Sheet",     [](Entity* e){ e->addComponent<SpriteSheetComponent>(); } },
                { "Sorting Group",    [](Entity* e){ e->addComponent<SortingGroup>(); } },
                { "Flip",             [](Entity* e){ e->addComponent<FlipComponent>(); } },
                { "Post Process",     [](Entity* e){ e->addComponent<PostProcessComponent>(); } },
            }},
            { "Lighting", {
                { "Light 2D",         [](Entity* e){ e->addComponent<Light2DComponent>(); } },
            }},
            { "Collision", {
                { "Box Collider 2D",     [](Entity* e){ e->addComponent<BoxCollider2D>(); } },
                { "Circle Collider 2D",  [](Entity* e){ e->addComponent<CircleCollider2D>(); } },
                { "Polygon Collider 2D", [](Entity* e){ e->addComponent<PolygonCollider2D>(); } },
                { "Edge Collider 2D",    [](Entity* e){ e->addComponent<EdgeCollider2D>(); } },
                { "Trigger",             [](Entity* e){ e->addComponent<TriggerComponent>(); } },
            }},
            { "Physics", {
                { "Rigidbody",          [](Entity* e){ e->addComponent<RigidbodyComponent>(); } },
                { "Bouncer",            [](Entity* e){ e->addComponent<BouncerComponent>(); } },
                { "Physics Material 2D",[](Entity* e){ e->addComponent<PhysicsMaterial2DComponent>(); } },
                { "Distance Joint 2D",  [](Entity* e){ e->addComponent<DistanceJoint2D>(); } },
                { "Hinge Joint 2D",     [](Entity* e){ e->addComponent<HingeJoint2D>(); } },
                { "Spring Joint 2D",    [](Entity* e){ e->addComponent<SpringJoint2D>(); } },
                { "Area Effect 2D",     [](Entity* e){ e->addComponent<AreaEffect2DComponent>(); } },
            }},
            { "Movement", {
                { "Path Follower",        [](Entity* e){ e->addComponent<PathFollowerComponent>(); } },
                { "Navigation Agent 2D",  [](Entity* e){ e->addComponent<NavigationAgent2DComponent>(); } },
                { "Navigation Region 2D", [](Entity* e){ e->addComponent<NavigationRegion2DComponent>(); } },
            }},
            { "Gameplay", {
                { "Spawn Point",      [](Entity* e){ e->addComponent<SpawnPointComponent>(); } },
                { "Checkpoint",       [](Entity* e){ e->addComponent<CheckpointComponent>(); } },
            }},
            { "Audio", {
                { "Audio Source",   [](Entity* e){ e->addComponent<AudioSourceComponent>(); } },
                { "Audio Listener", [](Entity* e){ e->addComponent<AudioListenerComponent>(); } },
            }},
            { "Camera", {
                { "Camera",           [](Entity* e){ e->addComponent<CameraComponent>(); } },
            }},
            { "Animation", {
                { "Animator",         [](Entity* e){ e->addComponent<AnimatorComponent>(); } },
                { "Animator State Machine", [](Entity* e){ e->addComponent<AnimatorStateMachineComponent>(); } },
                { "Tween",            [](Entity* e){ e->addComponent<TweenComponent>(); } },
            }},
            { "Utility", {
                { "Timer",             [](Entity* e){ e->addComponent<TimerComponent>(); } },
            }},
            { "Interaction", {
                { "Interactable",      [](Entity* e){ e->addComponent<InteractableComponent>(); } },
                { "Player Interactor", [](Entity* e){ e->addComponent<PlayerInteractorComponent>(); } },
            }},
        };
    }

    // Case-insensitive substring test for the search box.
    static bool MatchesFilter(const char* text, const char* filterLower)
    {
        if (!filterLower || !filterLower[0]) return true;
        std::string t = text;
        std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        return t.find(filterLower) != std::string::npos;
    }

    void Editor::ShowInspector()
    {
        // NoScrollbar/NoScrollWithMouse on the window itself: the inner ##inspectorBody
        // child owns scrolling (hidden bar + wheel), and the Add Component button stays
        // pinned at the bottom. Without this the docked window grew its own scrollbar.
        ImGui::Begin("Inspector", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const bool inPlay   = (state == GameState::Play || state == GameState::Pause);
        const bool inEditor = (state == GameState::Editor);

        // LIVE mode banner
        if (inPlay)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.30f, 0.30f, 0.030f, 1.0f));
            ImGui::BeginChild("##livebanner", ImVec2(-1, 28), false);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.2f, 1.0f), "  " ICON_FA_CIRCLE_PLAY "  LIVE — edits apply now, discarded on Stop");
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (selectedIndex != -1 && selectedIndex < (int)scene.entities.size() && inEditor)
        {
            Entity* inspected = scene.entities[selectedIndex].get();
            inspected->pendingRemoveComponentIndex = -1;

            // Component properties scroll inside this child region so overflowing
            // content never pushes the Add Component button off-screen or introduces
            // a panel-level scrollbar that shifts the layout. NoScrollbar + NoScrollWithMouse
            // is NOT set: we hide the bar visually but keep wheel-scrolling via
            // AlwaysVerticalScrollbar=false + NoScrollbar (bar hidden, wheel works).
            // Use the full available width explicitly to avoid clipping on the right.
            float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::BeginChild("##inspectorBody", ImVec2(-1, -footer), false, ImGuiWindowFlags_NoScrollbar);
            ImGui::PushID(inspected);
            inspected->inspect([this]() { TakeSnapshot(); });
            ImGui::PopID();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            if (inspected->pendingRemoveComponentIndex != -1 && !inPlay)
            {
                TakeSnapshot();
                inspected->removeComponent(inspected->pendingRemoveComponentIndex);
                inspected->pendingRemoveComponentIndex = -1;
            }
            else { inspected->pendingRemoveComponentIndex = -1; }

            ImGui::Separator();
            if (inPlay) ImGui::BeginDisabled();
            if (ImGui::Button("Add Component", ImVec2(-1, 0)))
            {
                componentSearchBuf_[0] = '\0';   // reset search each open
                ImGui::OpenPopup("Add Component##addcomp");
            }
            if (inPlay) ImGui::EndDisabled();
            ImVec2 addBtnMin = ImGui::GetItemRectMin();
            if (inPlay && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Stop simulation to add components");

            // Modal: resizable, taller default, fills remaining height automatically.
            float popupW = std::min(ImGui::GetWindowWidth() - 16.0f, 340.0f);
            ImGui::SetNextWindowSize(ImVec2(popupW, 520.0f), ImGuiCond_Appearing);
            ImGui::SetNextWindowPos(ImVec2(addBtnMin.x, addBtnMin.y - 540.0f), ImGuiCond_Appearing);
            ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0));
            bool modalOpen = true;
            if (ImGui::BeginPopupModal("Add Component##addcomp", &modalOpen,
                                       ImGuiWindowFlags_NoScrollbar))
            {
                Entity* target = scene.entities[selectedIndex].get();

                // --- Search box (auto-focused on open) ---
                ImGui::SetNextItemWidth(-1);
                if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                ImGui::InputTextWithHint("##compSearch", ICON_FA_MAGNIFYING_GLASS "  Search components...",
                                         componentSearchBuf_, sizeof(componentSearchBuf_));
                std::string filter = componentSearchBuf_;
                std::transform(filter.begin(), filter.end(), filter.begin(),
                               [](unsigned char c){ return (char)std::tolower(c); });
                ImGui::Separator();

                // Fill all remaining modal height — respects user resize.
                ImGui::BeginChild("##compList", ImVec2(-1, -1), false, ImGuiWindowFlags_NoScrollbar);

                const bool searching = !filter.empty();
                auto addAndClose = [&](const ComponentEntry& item)
                {
                    TakeSnapshot();
                    item.add(target);
                    ImGui::CloseCurrentPopup();
                };

                for (const auto& cat : BuildComponentCatalog())
                {
                    // Skip categories with no matches under the current filter.
                    bool anyVisible = false;
                    for (const auto& it : cat.items)
                        if (MatchesFilter(it.name, filter.c_str())) { anyVisible = true; break; }
                    if (!anyVisible) continue;

                    // When searching: auto-expand so matches are immediately visible.
                    // When browsing: CLOSED by default (user can open what they need;
                    // ImGui remembers the open/close state by ID for the session).
                    if (searching) ImGui::SetNextItemOpen(true, ImGuiCond_Always);

                    // Use "cat_<name>" as ID to avoid clashes when a category and a
                    // component inside it share the same label (e.g. "Camera").
                    ImGui::PushID(cat.name);
                    std::string catHeader = std::string(cat.name) + "##hdr";
                    if (ImGui::CollapsingHeader(catHeader.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
                    {
                        ImGui::Indent(8.0f);
                        for (const auto& it : cat.items)
                        {
                            if (!MatchesFilter(it.name, filter.c_str())) continue;
                            if (ImGui::Selectable(it.name)) addAndClose(it);
                        }
                        ImGui::Unindent(8.0f);
                    }
                    ImGui::PopID();
                }

                // --- Scripts category (dynamic — populated from the compiled DLL) ---
                const auto& scripts = ScriptManager::Get().GetAvailableScripts();
                bool anyScript = false;
                for (const auto& s : scripts) if (MatchesFilter(s.c_str(), filter.c_str())) { anyScript = true; break; }
                if (anyScript || (!searching && !scripts.empty()))
                {
                    if (searching) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                    if (ImGui::CollapsingHeader("Scripts##hdr", ImGuiTreeNodeFlags_SpanAvailWidth))
                    {
                        ImGui::Indent(8.0f);
                        if (scripts.empty()) ImGui::TextDisabled("(No scripts compiled)");
                        for (const auto& sName : scripts)
                        {
                            if (!MatchesFilter(sName.c_str(), filter.c_str())) continue;
                            if (ImGui::Selectable(sName.c_str()))
                            {
                                Component* newComp = ScriptManager::Get().InstantiateScript(sName);
                                if (newComp)
                                {
                                    TakeSnapshot();
                                    auto* ptr = target->addComponent(std::unique_ptr<Component>(newComp));
                                    if (state == GameState::Play) ptr->start(&scene);
                                }
                                ImGui::CloseCurrentPopup();
                            }
                        }
                        ImGui::Unindent(8.0f);
                    }
                }

                ImGui::EndChild();
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor(); // ModalWindowDimBg
        }
        ImGui::End();
    }
}
