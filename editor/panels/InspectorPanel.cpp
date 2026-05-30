#include "../Editor.hpp"

namespace Indium
{
    void Editor::ShowInspector()
    {
        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

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

            ImGui::PushID(inspected);
            inspected->inspect([this]() { TakeSnapshot(); });
            ImGui::PopID();

            if (inspected->pendingRemoveComponentIndex != -1 && !inPlay)
            {
                TakeSnapshot();
                inspected->removeComponent(inspected->pendingRemoveComponentIndex);
                inspected->pendingRemoveComponentIndex = -1;
            }
            else { inspected->pendingRemoveComponentIndex = -1; }

            ImGui::Separator();
            if (inPlay) ImGui::BeginDisabled();
            if (ImGui::Button("Add Component", ImVec2(-1, 0))) ImGui::OpenPopup("Component Popup");
            if (inPlay) ImGui::EndDisabled();
            if (inPlay && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Stop simulation to add components");

            if (ImGui::BeginPopup("Component Popup"))
            {
                ImGui::TextDisabled("Rendering");
                if (ImGui::MenuItem("Shape Renderer"))    { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<ShapeRendererComponent>(); }
                if (ImGui::MenuItem("Sprite Renderer"))   { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<SpriteRendererComponent>(); }
                if (ImGui::MenuItem("Text Renderer"))     { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<TextRendererComponent>(); }
                if (ImGui::MenuItem("Particle System"))   { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<ParticleSystemComponent>(); }
                if (ImGui::MenuItem("Tilemap"))           { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<TilemapComponent>(); }
                ImGui::Separator();
                ImGui::TextDisabled("Collision");
                if (ImGui::MenuItem("Box Collider 2D"))    { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<BoxCollider2D>(); }
                if (ImGui::MenuItem("Circle Collider 2D")) { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<CircleCollider2D>(); }
                ImGui::Separator();
                ImGui::TextDisabled("Audio");
                if (ImGui::MenuItem("Audio Source"))  { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<AudioSourceComponent>(); }
                ImGui::Separator();
                ImGui::TextDisabled("Physics & Logic");
                if (ImGui::MenuItem("Add Rigidbody")) { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<RigidbodyComponent>(); }
                if (ImGui::MenuItem("Add Bouncer"))   { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<BouncerComponent>(); }
                if (ImGui::MenuItem("Add Camera"))    { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<CameraComponent>(); }
                if (ImGui::MenuItem("Add Trigger"))   { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<TriggerComponent>(); }
                if (ImGui::MenuItem("Add Animator"))  { TakeSnapshot(); scene.entities[selectedIndex]->addComponent<AnimatorComponent>(); }
                ImGui::Separator();
                ImGui::TextDisabled("Scripts");

                const auto& scripts = ScriptManager::Get().GetAvailableScripts();
                if (scripts.empty())
                {
                    ImGui::TextDisabled("(No scripts compiled)");
                }
                else
                {
                    for (const auto& sName : scripts)
                    {
                        if (ImGui::MenuItem(sName.c_str()))
                        {
                            Component* newComp = ScriptManager::Get().InstantiateScript(sName);
                            if (newComp)
                            {
                                TakeSnapshot();
                                auto* ptr = scene.entities[selectedIndex]->addComponent(std::unique_ptr<Component>(newComp));
                                if (state == GameState::Play) ptr->start(&scene);
                            }
                        }
                    }
                }

                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
}
