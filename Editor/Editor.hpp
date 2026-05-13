#pragma once

#include "raylib.h"
#include "imgui.h"
#include "../include/rlImGui.h"
#include "../src/Config.hpp"
#include "../Scene/Scene.hpp"
#include "../Entity/EntityFactory.hpp"
#include "../Entity/Circle.hpp"
#include "../Entity/Rectangle.hpp"
#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <string>

// Bring Indium types into scope for the Editor
using Indium::Config;
using Indium::Scene;
using Indium::EntityFactory;

enum class GameState { Editor, Play };

/**
 * @brief Main Editor class that handles the UI, input, and game state.
 */
class Editor
{
private:
    Config              config;
    GameState           state = GameState::Editor;
    Scene               scene;
    int                 selectedIndex = -1;
    EntityFactory       factory;
    
    Vector2             dragOffset = { 0, 0 };
    Indium::Entity*     draggingEntity = nullptr;

public:

    /** @brief Deletes an entity and manages the selection index */
    void DeleteEntity(Indium::Entity& e)
    {
        for (auto it = scene.entities.begin(); it != scene.entities.end();)
        {
            if (it->get() == &e)
            {
                int deletedIndex = (int)(it - scene.entities.begin());
                it = scene.entities.erase(it);

                if (deletedIndex == selectedIndex)
                    selectedIndex = -1;
                else if (selectedIndex > deletedIndex)
                    selectedIndex--;
            }
            else
            {
                ++it;
            }
        }
    }

    /** @brief Updates the editor logic, input handling, and dragging */
    void Update(float dt)
    {
        Vector2 mouse = GetMousePosition();

        // Handle Entity Selection and Drag Initiation
        // We only check if the mouse is NOT over an ImGui window
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ImGui::GetIO().WantCaptureMouse)
        {
            draggingEntity = nullptr;
            // Iterate backwards to select the topmost entity
            for (int i = (int)scene.entities.size() - 1; i >= 0; i--)
            {
                if (scene.entities[i]->Contains(mouse))
                {
                    draggingEntity = scene.entities[i].get();
                    dragOffset = { draggingEntity->position.x - mouse.x, draggingEntity->position.y - mouse.y };
                    selectedIndex = i;
                    break;
                }
            }
        }

        // Handle Dragging Logic
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggingEntity != nullptr)
        {
            draggingEntity->position = { mouse.x + dragOffset.x, mouse.y + dragOffset.y };
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
        {
            draggingEntity = nullptr;
        }

        // Update Scene if in Play Mode
        if (state == GameState::Play)
        {
            scene.Update(dt);
        }
    }

    /** @brief Main render loop for the Editor UI and the Scene */
    void Run()
    {
        BeginDrawing();
        ClearBackground(Color{ 30, 30, 30, 255 });

        // Draw entities directly on screen
        scene.Draw();

        // Draw the Editor UI on top
        rlImGuiBegin();

        ShowMainMenuBar();
        ShowHierarchy();
        ShowInspector();

        rlImGuiEnd();

        if (config.showFps)
            DrawFPS(10, 30);

        EndDrawing();
    }

private:
    /** @brief Draws the top menu bar */
    void ShowMainMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Exit")) { /* Handle exit if needed */ }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem("Circle"))    scene.entities.push_back(factory.CreateCircle(scene));
                if (ImGui::MenuItem("Rectangle")) scene.entities.push_back(factory.CreateRectangle(scene));
                if (ImGui::MenuItem("Plane"))     scene.entities.push_back(factory.CreatePlane(scene));
                ImGui::EndMenu();
            }

            // Play / Stop Button centered in the MenuBar
            float windowWidth = ImGui::GetWindowSize().x;
            ImGui::SetCursorPosX(windowWidth / 2.0f - 25);

            const char* btnLabel = (state == GameState::Editor) ? "Play" : "Stop";
            ImVec4 btnColor = (state == GameState::Editor)
                ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f)
                : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
            if (ImGui::Button(btnLabel, ImVec2(50, 0)))
            {
                if (state == GameState::Editor)
                {
                    scene.Save();
                    state = GameState::Play;
                }
                else
                {
                    scene.Restore();
                    state = GameState::Editor;
                }
            }
            ImGui::PopStyleColor();

            ImGui::EndMainMenuBar();
        }
    }

    /** @brief Draws the Hierarchy window (left panel) */
    void ShowHierarchy()
    {
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        ImGui::SetNextWindowPos(ImVec2(0, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(250, (float)screenH - 20), ImGuiCond_FirstUseEver);
        ImGui::Begin("Hierarchy");

        if (ImGui::Button("Add Circle", ImVec2(-1, 0)))    scene.entities.push_back(factory.CreateCircle(scene));
        if (ImGui::Button("Add Rectangle", ImVec2(-1, 0))) scene.entities.push_back(factory.CreateRectangle(scene));
        if (ImGui::Button("Add Plane", ImVec2(-1, 0)))     scene.entities.push_back(factory.CreatePlane(scene));

        ImGui::Separator();
        ImGui::Text("Entities");
        ImGui::Separator();

        for (int i = 0; i < (int)scene.entities.size(); i++)
        {
            char label[128];
            snprintf(label, sizeof(label), "%s##%d", scene.entities[i]->name.c_str(), i);

            if (ImGui::Selectable(label, selectedIndex == i))
            {
                selectedIndex = i;
            }
        }

        ImGui::End();
    }

    /** @brief Draws the Inspector window (right panel) */
    void ShowInspector()
    {
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        ImGui::SetNextWindowPos(ImVec2((float)screenW - 300, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, (float)screenH - 20), ImGuiCond_FirstUseEver);
        ImGui::Begin("Inspector");

        if (selectedIndex != -1 && selectedIndex < (int)scene.entities.size())
        {
            Indium::Entity* selected = scene.entities[selectedIndex].get();
            selected->inspect();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Delete Entity", ImVec2(-1, 40)))
            {
                DeleteEntity(*selected);
            }
        }
        else
        {
            ImGui::Text("Select an entity to inspect.");
        }

        ImGui::End();
    }
};
