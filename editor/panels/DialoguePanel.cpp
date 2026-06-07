#include "../Editor.hpp"
#include "../../tools/FileBrowser.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <system_error>

namespace sfs = std::filesystem;

namespace Indium
{
    // Visual authoring for branching dialogue. Reads / writes the same
    // <project>/dialogue/<name>.json format DialogueManager runs at runtime, so a
    // dialogue authored here plays back with no extra glue. During Play it can also
    // start the open dialogue in the viewport for a live preview (mirrors how the
    // Quests panel live-drives quests).
    //
    // The working copy is held on the Editor as an ordered vector of nodes
    // (dlgNodes_/dlgStart_/dlgFile_) rather than DialogueManager's runtime map, so
    // authoring order is stable and node ids can be edited freely before save.
    namespace
    {
        sfs::path DialogueDir(const std::string& projectPath)
        {
            return sfs::path(projectPath) / "dialogue";
        }

        // dialogue/*.json file stems, sorted.
        std::vector<std::string> ListDialogues(const std::string& projectPath)
        {
            std::vector<std::string> out;
            std::error_code ec;
            const sfs::path dir = DialogueDir(projectPath);
            if (!sfs::exists(dir, ec)) return out;
            for (const auto& e : sfs::directory_iterator(dir, ec))
            {
                if (ec) break;
                if (e.is_regular_file() && e.path().extension() == ".json")
                    out.push_back(e.path().stem().string());
            }
            std::sort(out.begin(), out.end());
            return out;
        }

        // ImGui InputText bound to a std::string via a heap buffer (no large stack
        // arrays). Returns true on edit.
        bool StrField(const char* id, std::string& s, std::size_t cap,
                      bool multiline = false, float height = 0.0f)
        {
            std::vector<char> buf(cap, '\0');
            std::strncpy(buf.data(), s.c_str(), cap - 1);
            const bool changed = multiline
                ? ImGui::InputTextMultiline(id, buf.data(), cap, ImVec2(-1.0f, height))
                : ImGui::InputText(id, buf.data(), cap);
            if (changed) s.assign(buf.data());
            return changed;
        }
    }

    void Editor::ShowDialogue()
    {
        if (!pm.IsProjectOpen()) { ImGui::TextDisabled("No project open."); return; }

        const std::string projectPath = pm.GetCurrentProjectPath();
        DialogueManager::Get().SetProjectPath(projectPath); // keep runtime in sync for preview
        const bool playing = (state == GameState::Play);

        // --- working-model helpers ------------------------------------------------

        // Load dialogue/<stem>.json into the working vector. Resets state on any failure.
        auto loadFile = [&](const std::string& stem)
        {
            dlgNodes_.clear();
            dlgStart_.clear();
            dlgFile_.clear();
            dlgLoaded_ = false;
            dlgDirty_  = false;

            const sfs::path path = DialogueDir(projectPath) / (stem + ".json");
            std::ifstream f(path);
            if (!f.is_open()) { TraceLog(LOG_WARNING, "DIALOGUE: cannot open %s", path.string().c_str()); return; }

            nlohmann::json j;
            try { f >> j; }
            catch (...) { TraceLog(LOG_WARNING, "DIALOGUE: invalid JSON in %s", path.string().c_str()); return; }

            DialogueManager::FromJson(j, dlgStart_, dlgNodes_); // shared with the runtime loader
            dlgFile_   = stem;
            dlgLoaded_ = true;
        };

        // Serialize the working vector back to dialogue/<stem>.json (atomic temp+rename,
        // matching SaveManager). Empty-id nodes are skipped; duplicate ids collapse
        // (last wins) — the UI warns about both before you get here.
        auto saveFile = [&](const std::string& stem) -> bool
        {
            std::error_code ec;
            sfs::create_directories(DialogueDir(projectPath), ec);

            const nlohmann::json j = DialogueManager::ToJson(dlgStart_, dlgNodes_); // shared format

            const sfs::path path = DialogueDir(projectPath) / (stem + ".json");
            sfs::path tmp = path; tmp += ".tmp";
            {
                std::ofstream out(tmp);
                if (!out.is_open()) { TraceLog(LOG_ERROR, "DIALOGUE: cannot write %s", tmp.string().c_str()); return false; }
                out << std::setw(2) << j << std::endl;
            }
            sfs::rename(tmp, path, ec);
            if (ec) { TraceLog(LOG_WARNING, "DIALOGUE: save failed for %s (%s)", path.string().c_str(), ec.message().c_str()); return false; }

            dlgFile_  = stem;
            dlgDirty_ = false;
            TraceLog(LOG_INFO, "DIALOGUE: saved %s", path.string().c_str());
            return true;
        };

        // Combo bound to a node-id string ("" = end-of-dialogue). A target that no
        // longer matches a node is shown as "<id>  (missing)" so dangling links surface.
        auto NextCombo = [&](const char* id, std::string& target)
        {
            std::vector<std::string> labels, values;
            labels.push_back("(end)"); values.emplace_back("");
            bool found = target.empty();
            for (const auto& n : dlgNodes_)
            {
                labels.push_back(n.id.empty() ? "(unnamed)" : n.id);
                values.push_back(n.id);
                if (n.id == target) found = true;
            }
            if (!found) { labels.push_back(target + "  (missing)"); values.push_back(target); }

            int cur = 0;
            for (int i = 0; i < (int)values.size(); ++i) if (values[i] == target) { cur = i; break; }
            std::vector<const char*> ptrs;
            ptrs.reserve(labels.size());
            for (const auto& s : labels) ptrs.push_back(s.c_str());

            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo(id, &cur, ptrs.data(), (int)ptrs.size())) { target = values[cur]; dlgDirty_ = true; }
        };

        // Replace the working copy with a fresh single-node document named `stem`.
        auto doNew = [&](const std::string& stem)
        {
            dlgNodes_.clear();
            DialogueNode n; n.id = "start"; n.text = "Hello.";
            dlgNodes_.push_back(std::move(n));
            dlgStart_  = "start";
            dlgFile_   = stem;
            dlgLoaded_ = true;
            dlgDirty_  = true;   // unsaved until the user clicks Save
        };

        // Destructive actions (switch file / new) route through these so unsaved edits
        // aren't silently discarded: when the working copy is dirty they stash a pending
        // action for the confirmation bar instead of acting immediately.
        auto requestLoad = [&](const std::string& stem)
        {
            if (stem.empty()) return;
            if (dlgLoaded_ && dlgDirty_) { dlgPendingAction_ = 1; dlgPendingArg_ = stem; }
            else                         { loadFile(stem); }
        };
        auto requestNew = [&](const std::string& stem)
        {
            if (stem.empty()) return;
            if (dlgLoaded_ && dlgDirty_) { dlgPendingAction_ = 2; dlgPendingArg_ = stem; }
            else                         { doNew(stem); }
        };

        // --- header + toolbar -----------------------------------------------------
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        ImGui::TextDisabled("   " ICON_FA_COMMENT "  Dialogue Editor (project's dialogue/ folder)");
        ImGui::PopStyleVar();
        ImGui::Separator();

        // Unsaved-changes confirmation: a destructive action (load / new) on a dirty
        // working copy parks here until the user decides, so edits can't vanish silently.
        if (dlgPendingAction_ != 0)
        {
            const int         action = dlgPendingAction_;
            const std::string arg    = dlgPendingArg_;
            auto resolve = [&](bool saveFirst)
            {
                if (saveFirst && !dlgFile_.empty()) saveFile(dlgFile_);
                dlgPendingAction_ = 0; dlgPendingArg_.clear();
                if      (action == 1) loadFile(arg);
                else if (action == 2) doNew(arg);
            };

            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                ICON_FA_TRIANGLE_EXCLAMATION " \"%s\" has unsaved changes.", dlgFile_.c_str());
            if (ImGui::Button("Save & continue"))   resolve(true);
            ImGui::SameLine();
            if (ImGui::Button("Discard & continue")) resolve(false);
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) { dlgPendingAction_ = 0; dlgPendingArg_.clear(); }
            ImGui::Separator();
            return; // hold the editor body until the decision is made
        }

        const std::vector<std::string> files = ListDialogues(projectPath);

        ImGui::TextDisabled("File"); ImGui::SameLine();
        int fileIdx = -1;
        for (int i = 0; i < (int)files.size(); ++i) if (files[i] == dlgFile_) { fileIdx = i; break; }
        std::vector<const char*> fptrs;
        fptrs.reserve(files.size());
        for (const auto& s : files) fptrs.push_back(s.c_str());
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::Combo("##dlgfile", &fileIdx, fptrs.data(), (int)fptrs.size()) && fileIdx >= 0)
            requestLoad(files[fileIdx]);

        ImGui::SameLine();
        if (ImGui::SmallButton("Reload") && !dlgFile_.empty()) requestLoad(dlgFile_);

        ImGui::SameLine();
        {
            std::string saveLbl = ICON_FA_FLOPPY_DISK "  Save";
            if (dlgDirty_) saveLbl += " *";
            if (ImGui::SmallButton(saveLbl.c_str()) && dlgLoaded_ && !dlgFile_.empty()) saveFile(dlgFile_);
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputTextWithHint("##dlgnew", "new name", dlgNewNameBuf_, sizeof(dlgNewNameBuf_));
        ImGui::SameLine();
        std::string newStem = dlgNewNameBuf_;
        newStem.erase(std::remove(newStem.begin(), newStem.end(), ' '), newStem.end());
        // Refuse a name that already exists so a later Save can't silently clobber it.
        const bool newExists = !newStem.empty() &&
                               std::find(files.begin(), files.end(), newStem) != files.end();
        if (ImGui::SmallButton("New") && !newStem.empty() && !newExists)
        {
            requestNew(newStem);            // routed through the unsaved-changes guard
            dlgNewNameBuf_[0] = '\0';
        }
        if (newExists)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "name exists \xE2\x80\x94 open it instead");
        }

        if (!dlgLoaded_)
        {
            ImGui::Spacing();
            ImGui::TextWrapped("Pick a dialogue from the File dropdown, or type a name and click New "
                               "to start dialogue/<name>.json. Saved files run at runtime via "
                               "DialogueManager::Get().Start(\"<name>\") or an Interactable's Dialogue Id.");
            if (files.empty()) ImGui::TextDisabled("(This project has no dialogue files yet.)");
            return;
        }

        ImGui::Separator();

        // --- start node -----------------------------------------------------------
        ImGui::TextDisabled("Start node"); ImGui::SameLine();
        {
            // Mirror NextCombo: a start matching no node shows as "(missing)" rather than
            // silently displaying node 0, so a dangling start (e.g. a hand-edited file) is visible.
            std::vector<std::string> labels, values;
            bool found = false;
            for (const auto& n : dlgNodes_)
            {
                labels.push_back(n.id.empty() ? "(unnamed)" : n.id);
                values.push_back(n.id);
                if (n.id == dlgStart_) found = true;
            }
            if (!found) { labels.push_back(dlgStart_ + "  (missing)"); values.push_back(dlgStart_); }
            int cur = 0;
            for (int i = 0; i < (int)values.size(); ++i) if (values[i] == dlgStart_) { cur = i; break; }
            std::vector<const char*> ptrs;
            ptrs.reserve(labels.size());
            for (const auto& s : labels) ptrs.push_back(s.c_str());
            ImGui::SetNextItemWidth(200.0f);
            if (!labels.empty() && ImGui::Combo("##dlgstart", &cur, ptrs.data(), (int)ptrs.size()))
            { dlgStart_ = values[cur]; dlgDirty_ = true; }
        }

        // Surface authoring hazards the JSON format can't represent cleanly.
        {
            bool hasEmpty = false;
            std::vector<std::string> ids;
            for (const auto& n : dlgNodes_)
            {
                if (n.id.empty()) { hasEmpty = true; continue; }
                ids.push_back(n.id);
            }
            std::sort(ids.begin(), ids.end());
            const bool hasDup = std::adjacent_find(ids.begin(), ids.end()) != ids.end();
            if (hasEmpty || hasDup)
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                    ICON_FA_TRIANGLE_EXCLAMATION " Unnamed or duplicate node ids — saving skips/merges them.");
        }

        // A start that points at no node makes the runtime end the dialogue instantly.
        {
            bool startMissing = !dlgStart_.empty();
            for (const auto& n : dlgNodes_) if (n.id == dlgStart_) { startMissing = false; break; }
            if (startMissing)
                ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f),
                    ICON_FA_TRIANGLE_EXCLAMATION " Start node \"%s\" doesn't exist — pick a Start node above or it won't run.",
                    dlgStart_.c_str());
        }

        // --- node list ------------------------------------------------------------
        int deleteNode = -1;
        ImGui::BeginChild("dlgNodes", ImVec2(0, playing ? -62.0f : -36.0f), false);
        for (int i = 0; i < (int)dlgNodes_.size(); ++i)
        {
            DialogueNode& n = dlgNodes_[i];
            ImGui::PushID(i);

            std::string title = n.id.empty() ? "(unnamed)" : n.id;
            if (!n.id.empty() && n.id == dlgStart_) title += "   [start]";
            const bool open = ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem("nodectx"))
            {
                if (ImGui::MenuItem("Delete Node")) deleteNode = i;
                ImGui::EndPopup();
            }

            if (open)
            {
                ImGui::Indent(8.0f);

                ImGui::TextDisabled("id");      ImGui::SameLine(96); ImGui::SetNextItemWidth(-1);
                if (StrField("##id", n.id, 64))       dlgDirty_ = true;
                ImGui::TextDisabled("speaker"); ImGui::SameLine(96); ImGui::SetNextItemWidth(-1);
                if (StrField("##sp", n.speaker, 96))  dlgDirty_ = true;

                ImGui::TextDisabled("text");
                if (StrField("##tx", n.text, 2048, true, 54.0f)) dlgDirty_ = true;

                ImGui::TextDisabled("portrait"); ImGui::SameLine(96);
                ImGui::SetNextItemWidth(-72.0f);
                if (StrField("##pt", n.portrait, 256)) dlgDirty_ = true;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Speaker image shown left of the text box. Stored project-relative.");
                ImGui::SameLine();
                if (ImGui::SmallButton("Browse")) ImGui::OpenPopup("Portrait");
                {
                    // Scoped to this node's ImGui::PushID(i), so each Browse opens its own picker.
                    std::string picked;
                    if (FileBrowser::Draw("Portrait", picked, { ".png", ".jpg", ".bmp", ".tga" }))
                    {
                        // Store project-relative when the file lives under the project, else absolute.
                        std::error_code ec;
                        sfs::path rel = sfs::relative(picked, projectPath, ec);
                        n.portrait = (!ec && !rel.empty() && rel.string().rfind("..", 0) != 0)
                                   ? rel.generic_string() : picked;
                        dlgDirty_  = true;
                    }
                }
                if (!n.portrait.empty())
                {
                    Texture2D tex = AssetManager::Get().GetTexture(DialogueManager::ResolvePortraitPath(n.portrait, projectPath));
                    if (tex.id != 0)
                    {
                        const float th = 48.0f, tw = th * (float)tex.width / (float)tex.height;
                        ImGui::Image((ImTextureID)(uintptr_t)tex.id, ImVec2(tw, th), ImVec2(0, 0), ImVec2(1, 1));
                    }
                    else
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION " portrait not found");
                }

                ImGui::TextDisabled("set flag"); ImGui::SameLine(96); ImGui::SetNextItemWidth(-1);
                if (StrField("##sf", n.setFlag, 96))  dlgDirty_ = true;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("StoryState flag set true when this node is shown.");

                ImGui::TextDisabled("give item"); ImGui::SameLine(96); ImGui::SetNextItemWidth(140);
                if (StrField("##ngi", n.giveItem, 96)) dlgDirty_ = true;
                ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                if (ImGui::DragInt("##ngc", &n.giveCount, 0.1f, 1, 9999)) dlgDirty_ = true;
                ImGui::TextDisabled("take item"); ImGui::SameLine(96); ImGui::SetNextItemWidth(140);
                if (StrField("##nti", n.takeItem, 96)) dlgDirty_ = true;
                ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                if (ImGui::DragInt("##ntc", &n.takeCount, 0.1f, 1, 9999)) dlgDirty_ = true;

                ImGui::Spacing();

                if (n.choices.empty())
                {
                    ImGui::TextDisabled("next"); ImGui::SameLine(96);
                    NextCombo("##next", n.next);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Narration node: shown after [Space]. (end) finishes the dialogue.");
                }
                else
                {
                    ImGui::TextDisabled("Choices");
                    int delChoice = -1;
                    for (int c = 0; c < (int)n.choices.size(); ++c)
                    {
                        DialogueChoice& ch = n.choices[c];
                        ImGui::PushID(c);
                        ImGui::Separator();

                        ImGui::TextDisabled("%d.", c + 1); ImGui::SameLine();
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 28.0f);
                        if (StrField("##ctext", ch.text, 256)) dlgDirty_ = true;
                        ImGui::SameLine();
                        if (ImGui::SmallButton(ICON_FA_XMARK)) delChoice = c;

                        ImGui::TextDisabled("\xE2\x86\x92 next"); ImGui::SameLine(96); NextCombo("##cnext", ch.next);
                        ImGui::TextDisabled("set flag");          ImGui::SameLine(96); ImGui::SetNextItemWidth(-1);
                        if (StrField("##cset", ch.setFlag, 96)) dlgDirty_ = true;
                        ImGui::TextDisabled("require flag");      ImGui::SameLine(96); ImGui::SetNextItemWidth(-1);
                        if (StrField("##creq", ch.requireFlag, 96)) dlgDirty_ = true;
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choice hidden unless this condition holds (e.g. item.gold >= 5).");
                        ImGui::TextDisabled("give item");         ImGui::SameLine(96); ImGui::SetNextItemWidth(120);
                        if (StrField("##cgi", ch.giveItem, 96)) dlgDirty_ = true;
                        ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragInt("##cgc", &ch.giveCount, 0.1f, 1, 9999)) dlgDirty_ = true;
                        ImGui::TextDisabled("take item");         ImGui::SameLine(96); ImGui::SetNextItemWidth(120);
                        if (StrField("##cti", ch.takeItem, 96)) dlgDirty_ = true;
                        ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragInt("##ctc", &ch.takeCount, 0.1f, 1, 9999)) dlgDirty_ = true;

                        ImGui::PopID();
                    }
                    if (delChoice >= 0) { n.choices.erase(n.choices.begin() + delChoice); dlgDirty_ = true; }
                }

                ImGui::Spacing();
                if (ImGui::SmallButton("+ Add Choice")) { n.choices.push_back(DialogueChoice{}); dlgDirty_ = true; }
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete Node")) deleteNode = i;

                ImGui::Unindent(8.0f);
            }

            ImGui::PopID();
        }
        ImGui::EndChild();

        if (deleteNode >= 0)
        {
            dlgNodes_.erase(dlgNodes_.begin() + deleteNode);
            dlgStart_ = DialogueManager::NormalizeStart(dlgStart_, dlgNodes_); // self-heal a deleted start
            dlgDirty_ = true;
        }

        // --- footer: add node + (Play) live preview -------------------------------
        if (ImGui::Button("+ Add Node"))
        {
            DialogueNode n; n.id = "node" + std::to_string(dlgNodes_.size() + 1);
            const std::string newId = n.id;
            dlgNodes_.push_back(std::move(n));
            if (dlgStart_.empty()) dlgStart_ = newId;
            dlgDirty_ = true;
        }

        if (playing)
        {
            ImGui::SameLine();
            if (ImGui::Button("Preview in Viewport"))
            {
                if (dlgDirty_) saveFile(dlgFile_);          // run the latest edits
                DialogueManager::Get().Start(dlgFile_);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Saves, then starts this dialogue in the running game.");
            ImGui::SameLine();
            ImGui::TextDisabled(DialogueManager::Get().IsActive() ? "(dialogue active)" : "");
        }
    }
}
