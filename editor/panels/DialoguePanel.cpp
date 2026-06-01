#include "../Editor.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cstring>

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

            dlgStart_ = j.value("start", std::string{});
            if (j.contains("nodes") && j["nodes"].is_object())
            {
                for (auto it = j["nodes"].begin(); it != j["nodes"].end(); ++it)
                {
                    const auto&  nj = it.value();
                    DialogueNode n;
                    n.id      = it.key();
                    n.speaker = nj.value("speaker", std::string{});
                    n.text    = nj.value("text", std::string{});
                    n.setFlag = nj.value("setFlag", std::string{});
                    n.next    = nj.value("next", std::string{});
                    if (nj.contains("choices") && nj["choices"].is_array())
                        for (const auto& cj : nj["choices"])
                        {
                            DialogueChoice c;
                            c.text        = cj.value("text", std::string{});
                            c.next        = cj.value("next", std::string{});
                            c.setFlag     = cj.value("setFlag", std::string{});
                            c.requireFlag = cj.value("requireFlag", std::string{});
                            n.choices.push_back(std::move(c));
                        }
                    dlgNodes_.push_back(std::move(n));
                }
            }
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

            nlohmann::json nodes = nlohmann::json::object();
            for (const auto& n : dlgNodes_)
            {
                if (n.id.empty()) continue;
                nlohmann::json nj;
                nj["speaker"] = n.speaker;
                nj["text"]    = n.text;
                if (!n.setFlag.empty()) nj["setFlag"] = n.setFlag;
                if (!n.next.empty())    nj["next"]    = n.next;
                if (!n.choices.empty())
                {
                    nlohmann::json cs = nlohmann::json::array();
                    for (const auto& c : n.choices)
                    {
                        nlohmann::json cj;
                        cj["text"] = c.text;
                        cj["next"] = c.next;
                        if (!c.setFlag.empty())     cj["setFlag"]     = c.setFlag;
                        if (!c.requireFlag.empty()) cj["requireFlag"] = c.requireFlag;
                        cs.push_back(std::move(cj));
                    }
                    nj["choices"] = cs;
                }
                nodes[n.id] = std::move(nj);
            }

            nlohmann::json j;
            j["start"] = dlgStart_;
            j["nodes"] = std::move(nodes);

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

        // --- header + toolbar -----------------------------------------------------
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        ImGui::TextDisabled("   " ICON_FA_COMMENT "  Dialogue Editor (project's dialogue/ folder)");
        ImGui::PopStyleVar();
        ImGui::Separator();

        const std::vector<std::string> files = ListDialogues(projectPath);

        ImGui::TextDisabled("File"); ImGui::SameLine();
        int fileIdx = -1;
        for (int i = 0; i < (int)files.size(); ++i) if (files[i] == dlgFile_) { fileIdx = i; break; }
        std::vector<const char*> fptrs;
        fptrs.reserve(files.size());
        for (const auto& s : files) fptrs.push_back(s.c_str());
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::Combo("##dlgfile", &fileIdx, fptrs.data(), (int)fptrs.size()) && fileIdx >= 0)
            loadFile(files[fileIdx]);

        ImGui::SameLine();
        if (ImGui::SmallButton("Reload") && !dlgFile_.empty()) loadFile(dlgFile_);

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
        if (ImGui::SmallButton("New"))
        {
            std::string stem = dlgNewNameBuf_;
            stem.erase(std::remove(stem.begin(), stem.end(), ' '), stem.end());
            if (!stem.empty())
            {
                dlgNodes_.clear();
                DialogueNode n; n.id = "start"; n.text = "Hello.";
                dlgNodes_.push_back(std::move(n));
                dlgStart_         = "start";
                dlgFile_          = stem;
                dlgLoaded_        = true;
                dlgDirty_         = true;   // unsaved until the user clicks Save
                dlgNewNameBuf_[0] = '\0';
            }
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
            std::vector<std::string> ids;
            for (const auto& n : dlgNodes_) ids.push_back(n.id.empty() ? "(unnamed)" : n.id);
            int cur = 0;
            for (int i = 0; i < (int)dlgNodes_.size(); ++i) if (dlgNodes_[i].id == dlgStart_) { cur = i; break; }
            std::vector<const char*> ptrs;
            ptrs.reserve(ids.size());
            for (const auto& s : ids) ptrs.push_back(s.c_str());
            ImGui::SetNextItemWidth(200.0f);
            if (!ids.empty() && ImGui::Combo("##dlgstart", &cur, ptrs.data(), (int)ptrs.size()) && cur < (int)dlgNodes_.size())
            { dlgStart_ = dlgNodes_[cur].id; dlgDirty_ = true; }
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

                ImGui::TextDisabled("set flag"); ImGui::SameLine(96); ImGui::SetNextItemWidth(-1);
                if (StrField("##sf", n.setFlag, 96))  dlgDirty_ = true;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("StoryState flag set true when this node is shown.");

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
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choice hidden until this StoryState flag is set.");

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

        if (deleteNode >= 0) { dlgNodes_.erase(dlgNodes_.begin() + deleteNode); dlgDirty_ = true; }

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
