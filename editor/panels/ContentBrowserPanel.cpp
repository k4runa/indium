#include "../Editor.hpp"

namespace Indium
{
    void Editor::ShowContentBrowser()
    {
        if (!pm.IsProjectOpen()) { ImGui::TextDisabled("No project open."); return; }

        static std::string selectedFolder;
        static std::string lastKnownProjectPath;
        static std::string cbCachedFolder;
        static std::vector<fs::directory_entry> cbCachedEntries;

        if (lastKnownProjectPath != pm.GetCurrentProjectPath())
        {
            lastKnownProjectPath = pm.GetCurrentProjectPath();
            selectedFolder = pm.GetCurrentProjectPath();
            cbCachedFolder.clear();
        }

        float availH = ImGui::GetContentRegionAvail().y;
        static float treeWidth = 250.0f;
        static float treeMinWidth = 200.0f;
        if(treeWidth <= treeMinWidth) treeWidth = treeMinWidth;
        // --- Left Side: Folder Tree ---
        ImGui::BeginChild("FolderTree", ImVec2(treeWidth, availH), true, ImGuiWindowFlags_HorizontalScrollbar);
        std::function<void(const fs::path&, bool)> DrawFolderTree;
        DrawFolderTree = [&](const fs::path& path, bool isRoot)
        {
            std::string name = path.filename().string();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (selectedFolder == path.string()) flags |= ImGuiTreeNodeFlags_Selected;
            if (isRoot) flags |= ImGuiTreeNodeFlags_DefaultOpen;

            bool hasSubdirs = false;
            if (fs::exists(path))
            {
                for (auto& entry : fs::directory_iterator(path)) { if (entry.is_directory()) { hasSubdirs = true; break; } }
            }
            if (!hasSubdirs) flags |= ImGuiTreeNodeFlags_Leaf;

            std::string label = std::string(ICON_FA_FOLDER "  ") + name;
            bool open = ImGui::TreeNodeEx(label.c_str(), flags);
            if (ImGui::IsItemClicked()) selectedFolder = path.string();

            if (open)
            {
                if (fs::exists(path)) { for (auto& entry : fs::directory_iterator(path)) { if (entry.is_directory()) DrawFolderTree(entry.path(), false); } }
                ImGui::TreePop();
            }
        };
        DrawFolderTree(pm.GetCurrentProjectPath(), true);
        ImGui::EndChild();

        // --- Resize Handle ---
        ImGui::SameLine();
        ImGui::Button("##treeSplitter", ImVec2(4.0f, availH));
        if (ImGui::IsItemActive()) treeWidth += ImGui::GetIO().MouseDelta.x;
        treeWidth = std::clamp(treeWidth, 100.0f, 400.0f);
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        ImGui::SameLine();

        // --- Right Side: Content View ---
        ImGui::BeginChild("ContentView", ImVec2(0, availH), false);

        // Navigation bar & Actions
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
        if (ImGui::Button(ICON_FA_PLUS "  Create Script")) ImGui::OpenPopup("Create A New Script");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ARROWS_ROTATE "  Refresh")) cbCachedFolder.clear();
        ImGui::PopStyleVar();

        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputTextWithHint("##cbSearch", ICON_FA_MAGNIFYING_GLASS "  Search...",
                                 contentSearchBuf_, sizeof(contentSearchBuf_));
        std::string cbFilter = contentSearchBuf_;
        std::transform(cbFilter.begin(), cbFilter.end(), cbFilter.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });

        ImGui::SameLine();
        if (selectedFolder != pm.GetCurrentProjectPath())
        {
            if (ImGui::Button(ICON_FA_ARROW_LEFT)) {
                fs::path p(selectedFolder);
                if (p.has_parent_path() && p.parent_path().string() >= pm.GetCurrentProjectPath()) selectedFolder = p.parent_path().string();
            }
            ImGui::SameLine();
        }
        ImGui::TextDisabled(ICON_FA_FOLDER_OPEN "  %s", fs::relative(selectedFolder, pm.GetCurrentProjectPath()).string().c_str());
        ImGui::Separator();

        // New Script Modal
        if (ImGui::BeginPopupModal("Create A New Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static char scriptName[64] = "NewScript";
            ImGui::InputText("Script Name", scriptName, 64);

            auto scriptNameOk = [](const char* s)
            {
                std::string n(s);
                if (n.empty() || n.find_first_not_of(" \t") == std::string::npos) return false;
                if (n.find('/') != std::string::npos || n.find('\\') != std::string::npos) return false;
                if (n.find("..") != std::string::npos) return false;
                // C++ class name: must start with letter/underscore, only alnum + underscore
                if (!std::isalpha((unsigned char)n[0]) && n[0] != '_') return false;
                for (char c : n) if (!std::isalnum((unsigned char)c) && c != '_') return false;
                return true;
            };
            bool snameOk = scriptNameOk(scriptName);
            if (!snameOk) ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION "  Must be a valid C++ identifier.");

            ImGui::Spacing();
            if (!snameOk) ImGui::BeginDisabled();
            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                std::string sName(scriptName);
                std::string projectPath = pm.GetCurrentProjectPath();
                std::string scriptDir = projectPath + "/scripts";

                // Ensure scripts directory exists
                if (!fs::exists(scriptDir)) fs::create_directories(scriptDir);

                std::string filePath = scriptDir + "/" + sName + ".cpp";
                if (!fs::exists(filePath))
                {
                    // 1. Create the template script
                    std::ofstream f(filePath);
                    f << "#include \"IndiumEngine.hpp\"\n\n"
                      << "using namespace Indium;\n\n"
                      << "class " << sName << " : public NativeScript {\n"
                      << "public:\n"
                      << "    // IND_PROP(type, name, defaultValue) — exposes a variable to the Inspector.\n"
                      << "    // Do NOT declare the variable separately; IND_PROP does it for you.\n"
                      << "    IND_PROP(float, speed, 200.0f);\n\n"
                      << "    void OnStart() override {\n"
                      << "        // Called once when the game starts (or when this entity is spawned).\n"
                      << "    }\n\n"
                      << "    void OnUpdate(float dt) override {\n"
                      << "        // Called every frame. 'entity' is the owning entity.\n"
                      << "        // Spawn:       auto* c = Spawn<Circle>(\"name\");\n"
                      << "        // Find:        Entity* e = FindByName(\"Player\");\n"
                      << "        // Destroy:     Destroy();  or  Destroy(target);\n"
                      << "        // Components:  auto* rb = GetComponent<RigidbodyComponent>();\n"
                      << "        // Story:       StoryState::Get().SetFlag(\"key\");\n"
                      << "    }\n\n"
                      << "    void OnDraw() const override {\n"
                      << "        // Called every frame for custom drawing (world space).\n"
                      << "        // DrawText(\"Hi\", entity->position.x, entity->position.y, 20, WHITE);\n"
                      << "    }\n\n"
                      << "    void OnDestroy() override {\n"
                      << "        // Called just before this entity is removed from the scene.\n"
                      << "    }\n"
                      << "};\n\n"
                      << "REGISTER_SCRIPT(" << sName << ")\n";
                    f.close();

                    consoleLogs.push_back({ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO] ", "Created script: " + sName + ".cpp", ICON_FA_CIRCLE_CHECK});
                    cbCachedFolder.clear();
                }
                ImGui::CloseCurrentPopup();
            }
            if (!snameOk) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        // --- Grid: Unity-style compact tiles ---
        float cellSize = 80.0f;
        float itemSpacing = 6.0f;
        float gridWidth = ImGui::GetContentRegionAvail().x;
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Rebuild directory listing only when the selected folder changes.
        if (selectedFolder != cbCachedFolder)
        {
            cbCachedFolder = selectedFolder;
            cbCachedEntries.clear();
            if (fs::exists(selectedFolder)) for (auto& e : fs::directory_iterator(selectedFolder)) cbCachedEntries.push_back(e);
        }

        {
            int itemIdx = 0;
            for (const auto& entry : cbCachedEntries)
            {
                auto path = entry.path();
                std::string name = path.filename().string();

                // Search filter — skip non-matching entries (don't advance itemIdx so
                // the flow layout stays contiguous).
                if (!cbFilter.empty())
                {
                    std::string ln = name;
                    std::transform(ln.begin(), ln.end(), ln.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    if (ln.find(cbFilter) == std::string::npos) continue;
                }

                // Flow layout: place items side by side, wrap when needed
                float nextX = (cellSize + itemSpacing) * itemIdx;
                int perRow = (int)(gridWidth / (cellSize + itemSpacing));
                if (perRow < 1) perRow = 1;
                if (itemIdx % perRow != 0) ImGui::SameLine(0.0f, itemSpacing);
                ImGui::PushID(name.c_str());
                bool isDir = entry.is_directory();
                const char* icon = ICON_FA_FILE;
                ImVec4 iconColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                std::string ext = path.extension().string();

                if (isDir)                                { icon = ICON_FA_FOLDER;    iconColor = ImVec4(0.95f, 0.75f, 0.2f, 1.0f); }
                else if (ext == ".cpp" || ext == ".hpp")  { icon = ICON_FA_FILE_CODE; iconColor = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);  }
                else if (ext == ".scene")                 { icon = ICON_FA_MAP;       iconColor = ImVec4(0.4f, 0.8f, 0.4f, 1.0f);  }
                else if (ext == ".png" || ext == ".jpg")  { icon = ICON_FA_IMAGE;     iconColor = ImVec4(0.8f, 0.5f, 0.9f, 1.0f);  }
                else if (ext == ".glb" || ext == ".gltf" || ext == ".obj")
                                                          { icon = ICON_FA_CUBE;      iconColor = ImVec4(0.4f, 0.85f, 0.9f, 1.0f);  }
                else if (ext == ".prefab")                { icon = ICON_FA_BOX;       iconColor = ImVec4(1.0f, 0.75f, 0.3f, 1.0f);  }

                // --- Render tile with InvisibleButton (no BeginChild overhead) ---
                ImVec2 tileSize = ImVec2(cellSize, cellSize + 16.0f);
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 p1 = ImVec2(p0.x + tileSize.x, p0.y + tileSize.y);

                ImGui::InvisibleButton("##tile", tileSize);
                bool hovered = ImGui::IsItemHovered();
                bool clicked = ImGui::IsItemClicked();

                // --- Drag and Drop Source ---
                if (!isDir && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    std::string relPath = fs::relative(path, pm.GetCurrentProjectPath()).string();
                    ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", relPath.c_str(), relPath.size() + 1);
                    ImGui::Text("%s %s", icon, name.c_str());
                    ImGui::EndDragDropSource();
                }

                // Hover background
                if (hovered) drawList->AddRectFilled(p0, p1, ImColor(1.0f, 1.0f, 1.0f, 0.06f), 4.0f);

                // Icon (centered, large)
                float oldScale = ImGui::GetFont()->Scale;
                ImGui::GetFont()->Scale *= 3.0f;
                ImGui::PushFont(ImGui::GetFont());
                float iconW = ImGui::CalcTextSize(icon).x;
                float iconH = ImGui::CalcTextSize(icon).y;
                ImGui::PopFont();
                ImGui::GetFont()->Scale = oldScale;

                ImVec2 iconPos = ImVec2(
                    p0.x + (cellSize - iconW) * 0.5f,
                    p0.y + (cellSize - iconH) * 0.5f - 4.0f
                );

                // Draw icon directly via DrawList (avoids layout issues)
                ImGui::GetFont()->Scale *= 3.0f;
                ImGui::PushFont(ImGui::GetFont());
                drawList->AddText(iconPos, ImGui::GetColorU32(iconColor), icon);
                ImGui::PopFont();
                ImGui::GetFont()->Scale = oldScale;

                // Text label (centered below icon)
                std::string displayName = name;
                float textW = ImGui::CalcTextSize(name.c_str()).x;
                if (textW > cellSize - 4.0f)
                {
                    for (int len = (int)name.size() - 1; len > 3; len--)
                    {
                        displayName = name.substr(0, len) + "..";
                        if (ImGui::CalcTextSize(displayName.c_str()).x <= cellSize - 4.0f) break;
                    }
                    textW = ImGui::CalcTextSize(displayName.c_str()).x;
                }

                ImVec2 textPos = ImVec2(
                    p0.x + (cellSize - textW) * 0.5f,
                    p1.y - ImGui::GetTextLineHeight() - 2.0f
                );
                drawList->AddText(textPos, ImColor(180, 180, 180), displayName.c_str());

                // Click handler
                if (clicked && isDir) selectedFolder = path.string();
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && !isDir && ext == ".prefab" && pm.IsProjectOpen() && state == GameState::Editor)
                {
                    nlohmann::json j = PrefabManager::Load(path.string());
                    if (!j.is_null())
                    {
                        TakeSnapshot();
                        auto e = factory.LoadEntity(j);
                        if (e)
                        {
                            e->id       = scene.nextEntityId++;
                            e->position = editorCamera.target;
                            scene.entities.push_back(std::move(e));
                            selectedIndex = (int)scene.entities.size() - 1;
                        }
                    }
                }

                ImGui::PopID();
                itemIdx++;
            }
        }

        ImGui::EndChild();
    }
}
